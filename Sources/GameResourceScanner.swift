import Foundation

/// Single-pass, off-main-thread inventory of imported X-Ray resource folders.
enum GameResourceScanner {
    struct Inventory {
        let count: Int
        let bytes: Int64
        let detected: Bool
        let diagnostics: String
        let archiveCount: Int
        let archiveSummary: String
        let firstArchiveHeader: String
    }

    static func scan(_ folder: URL) -> Inventory? {
        let fm = FileManager.default
        guard fm.fileExists(atPath: folder.path) else { return nil }
        let entries = (try? fm.contentsOfDirectory(atPath: folder.path)) ?? []
        let names = Set(entries.map { $0.lowercased() })
        let config = names.contains("fsgame.ltx")
        let data = names.contains("gamedata") || names.contains(where: { $0.hasPrefix("gamedata.db") })
        let diagnostics = "fsgame.ltx: \(config ? "yes" : "no") · gamedata: \(data ? "yes" : "no")"

        let enumerator = fm.enumerator(at: folder, includingPropertiesForKeys: [.isRegularFileKey, .fileSizeKey])
        var count = 0
        var bytes: Int64 = 0
        var detected = false
        var archiveCount = 0
        var archiveBytes: Int64 = 0
        var emptyArchives = 0
        var header = "No X-Ray archive header available"
        while let file = enumerator?.nextObject() as? URL {
            let name = file.lastPathComponent.lowercased()
            if name == "fsgame.ltx" || name == "gamedata" || name.hasPrefix("gamedata.db") {
                detected = true
            }
            guard let info = try? file.resourceValues(forKeys: [.isRegularFileKey, .fileSizeKey]),
                  info.isRegularFile == true else { continue }
            count += 1
            let size = Int64(info.fileSize ?? 0)
            bytes += size
            guard name.hasPrefix("gamedata.db") else { continue }
            archiveCount += 1
            archiveBytes += size
            if size == 0 { emptyArchives += 1 }
            if size > 0 && header == "No X-Ray archive header available",
               let handle = try? FileHandle(forReadingFrom: file) {
                let prefix = (try? handle.read(upToCount: 8)) ?? Data()
                try? handle.close()
                header = "\(file.lastPathComponent): \(XRayArchiveInspector.headerHex(prefix))"
            }
        }
        let archiveSummary = archiveCount == 0 ? "No gamedata.db archives found" :
            "\(archiveCount) archives, \(ByteCountFormatter.string(fromByteCount: archiveBytes, countStyle: .file)), \(emptyArchives) empty"
        return Inventory(count: count, bytes: bytes, detected: detected,
                         diagnostics: diagnostics, archiveCount: archiveCount,
                         archiveSummary: archiveSummary, firstArchiveHeader: header)
    }
}
