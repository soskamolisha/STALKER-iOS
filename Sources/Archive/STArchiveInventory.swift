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
        // A filename inventory alone cannot establish archive integrity or playability.
        var isEngineReady: Bool { false }
    }

    /// Shadow of Chernobyl uses dba–dbf after db9. Some sets spell
    /// these indices as decimal db10–db15.
    static func archiveNumber(_ name: String) -> Int? {
        let prefix = "gamedata.db"
        let lower = name.lowercased()
        guard lower.hasPrefix(prefix) else { return nil }
        let suffix = lower.dropFirst(prefix.count).utf8
        if suffix.count == 1, let letter = suffix.first, (97...102).contains(letter) {
            return Int(letter) - 87
        }
        guard (1...4).contains(suffix.count), suffix.allSatisfy({ (48...57).contains($0) }) else {
            return nil
        }
        return Int(lower.dropFirst(prefix.count))
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
        for url in urls.sorted(by: { $0.lastPathComponent < $1.lastPathComponent }) {
            let name = url.lastPathComponent
            let prefix = "gamedata.db"
            guard name.lowercased().hasPrefix(prefix) else { continue }
            if name.lowercased().hasSuffix(".zip") {
                warnings.append("Extract ZIP before importing: \(name)")
                continue
            }
            guard let number = archiveNumber(name) else {
                warnings.append("Unrecognized archive name: \(name)")
                continue
            }
            let values = try url.resourceValues(forKeys: keys)
            guard values.isSymbolicLink != true, values.isRegularFile == true else {
                warnings.append("Not a regular file: \(name)")
                continue
            }
            guard !seen.contains(number) else {
                warnings.append("Duplicate archive index: \(number)")
                continue
            }
            let size = UInt64(max(0, values.fileSize ?? 0))
            if size == 0 { warnings.append("Empty archive: \(name)"); continue }
            // Reject ZIP containers renamed to gamedata.dbN. Only read a tiny
            // signature; never load a potentially multi-gigabyte archive.
            let handle = try FileHandle(forReadingFrom: url)
            let signature = try handle.read(upToCount: 4) ?? Data()
            try handle.close()
            if signature.count >= 4 &&
                signature[0] == 0x50 && signature[1] == 0x4B &&
                ((signature[2] == 0x03 && signature[3] == 0x04) ||
                 (signature[2] == 0x05 && signature[3] == 0x06) ||
                 (signature[2] == 0x07 && signature[3] == 0x08)) {
                warnings.append("ZIP content detected in \(name); extract it first")
                continue
            }
            seen.insert(number)
            archives.append(Archive(number: number, filename: name, bytes: size))
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
