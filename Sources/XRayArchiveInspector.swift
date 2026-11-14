import Foundation

/// Read-only inspection of X-Ray archive candidates.
/// This does not decompress or execute archive content.
enum XRayArchiveInspector {
    struct ArchiveInfo {
        let name: String
        let size: Int64
        let header: Data
        let appearsEmpty: Bool
    }

    static func inspect(in root: URL, limit: Int = 256) -> [ArchiveInfo] {
        let fm = FileManager.default
        guard let enumerator = fm.enumerator(at: root, includingPropertiesForKeys: [.isRegularFileKey, .fileSizeKey]) else {
            return []
        }
        var results: [ArchiveInfo] = []
        while let file = enumerator.nextObject() as? URL {
            let name = file.lastPathComponent.lowercased()
            guard name.hasPrefix("gamedata.db") else { continue }
            guard let values = try? file.resourceValues(forKeys: [.isRegularFileKey, .fileSizeKey]),
                  values.isRegularFile == true else { continue }
            guard let handle = try? FileHandle(forReadingFrom: file) else { continue }
            let header = (try? handle.read(upToCount: 32)) ?? Data()
            try? handle.close()
            results.append(ArchiveInfo(name: file.lastPathComponent,
                                       size: Int64(values.fileSize ?? 0),
                                       header: header,
                                       appearsEmpty: (values.fileSize ?? 0) == 0))
            if results.count >= limit { break }
        }
        return results
    }

    static func summary(_ archives: [ArchiveInfo]) -> String {
        guard !archives.isEmpty else { return "No gamedata.db archives found" }
        let empty = archives.filter { $0.appearsEmpty }.count
        let bytes = archives.reduce(Int64(0)) { $0 + $1.size }
        return "\(archives.count) archives, \(ByteCountFormatter.string(fromByteCount: bytes, countStyle: .file)), \(empty) empty"
    }

    static func headerHex(_ data: Data) -> String {
        data.prefix(8).map { String(format: "%02X", $0) }.joined(separator: " ")
    }
}
