import Foundation

/// Read-only inventory of X-Ray gamedata.dbN archives.
enum STArchiveInventory {
    struct Archive: Equatable {
        let number: Int
        let filename: String
        let bytes: UInt64
    }

    struct Report {
        let archives: [Archive]
        let missingNumbers: [Int]
        let warnings: [String]
        var hasBaseArchive: Bool { archives.contains { $0.number == 0 } }
    }

    static func scan(directory: URL) throws -> Report {
        let manager = FileManager.default
        let keys: Set<URLResourceKey> = [.isRegularFileKey, .isSymbolicLinkKey, .fileSizeKey]
        let urls = try manager.contentsOfDirectory(at: directory,
                                                    includingPropertiesForKeys: Array(keys),
                                                    options: [.skipsHiddenFiles])
        var archives: [Archive] = []
        var warnings: [String] = []
        var seen: Set<Int> = []
        for url in urls {
            let name = url.lastPathComponent
            let prefix = "gamedata.db"
            guard name.lowercased().hasPrefix(prefix) else { continue }
            let suffix = String(name.dropFirst(prefix.count))
            guard !suffix.isEmpty, suffix.utf8.allSatisfy({ $0 >= 48 && $0 <= 57 }),
                  let number = Int(suffix), number <= 9999 else {
                warnings.append("Unrecognized archive name: \(name)")
                continue
            }
            let values = try url.resourceValues(forKeys: keys)
            guard values.isSymbolicLink != true, values.isRegularFile == true else {
                warnings.append("Not a regular file: \(name)")
                continue
            }
            guard seen.insert(number).inserted else {
                warnings.append("Duplicate archive index: \(number)")
                continue
            }
            let size = UInt64(max(0, values.fileSize ?? 0))
            archives.append(Archive(number: number, filename: name, bytes: size))
            if size == 0 { warnings.append("Empty archive: \(name)") }
        }
        archives.sort { $0.number < $1.number }
        let indices = Set(archives.map(\.number))
        let maximum = archives.last?.number ?? -1
        let missing = maximum >= 0 ? (0...maximum).filter { !indices.contains($0) } : []
        if archives.isEmpty { warnings.append("No gamedata.dbN archives found") }
        if !indices.contains(0) { warnings.append("Base archive gamedata.db0 missing") }
        return Report(archives: archives, missingNumbers: missing, warnings: warnings)
    }
}
