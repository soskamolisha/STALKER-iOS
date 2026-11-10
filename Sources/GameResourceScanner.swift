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

    /// X-Ray archive naming convention: gamedata.db0, gamedata.dba, gamedata.db10.
    /// Reject lookalikes such as gamedata.db.backup.
    private static func isArchiveName(_ name: String) -> Bool {
        let prefix = "gamedata.db"
        guard name.hasPrefix(prefix) else { return false }
        let suffix = name.dropFirst(prefix.count)
        return !suffix.isEmpty && suffix.unicodeScalars.allSatisfy { scalar in
            (scalar.value >= 48 && scalar.value <= 57) ||
            (scalar.value >= 65 && scalar.value <= 90) ||
            (scalar.value >= 97 && scalar.value <= 122)
        }
    }

    /// Edition hints from loose level names only; packed archives are ambiguous.
    private static func looseLevelEditionHint(in root: URL) -> String {
        let levels = root.appendingPathComponent("gamedata/levels", isDirectory: true)
        guard let entries = try? FileManager.default.contentsOfDirectory(atPath: levels.path) else {
            return "edition: unknown (packed or no loose levels)"
        }
        let names = Set(entries.map { $0.lowercased() })
        let cop = names.contains("zaton") || names.contains("jupiter")
        let cs = names.contains("marsh")
        if cop && cs { return "edition: mixed level markers" }
        if cop { return "edition hint: Call of Pripyat" }
        if cs { return "edition hint: Clear Sky" }
        return "edition: unknown (no distinctive loose levels)"
    }

    static func scan(_ folder: URL) -> Inventory? {
        let fm = FileManager.default
        var rootIsDirectory: ObjCBool = false
        guard fm.fileExists(atPath: folder.path, isDirectory: &rootIsDirectory),
              rootIsDirectory.boolValue else { return nil }
        let entries = (try? fm.contentsOfDirectory(atPath: folder.path)) ?? []
        let names = Set(entries.map { $0.lowercased() })
        let config = names.contains("fsgame.ltx")
        let data = names.contains("gamedata") || names.contains(where: isArchiveName)
        let diagnostics = "fsgame.ltx: \(config ? "yes" : "no") · gamedata: \(data ? "yes" : "no") · \(looseLevelEditionHint(in: folder))"

        let enumerator = fm.enumerator(at: folder, includingPropertiesForKeys: [.isRegularFileKey, .fileSizeKey])
        var count = 0
        var bytes: Int64 = 0
        var archiveCount = 0
        var archiveBytes: Int64 = 0
        var emptyArchives = 0
        var hasLooseGameFile = false
        var hasNonemptyArchive = false
        var header = "No X-Ray archive header available"
        while let file = enumerator?.nextObject() as? URL {
            let name = file.lastPathComponent.lowercased()
            guard let info = try? file.resourceValues(forKeys: [.isRegularFileKey, .fileSizeKey]),
                  info.isRegularFile == true else { continue }
            count += 1
            let size = Int64(info.fileSize ?? 0)
            bytes += size
            let relativePath = String(file.standardizedFileURL.path.dropFirst(folder.standardizedFileURL.path.count + 1)).lowercased()
            if relativePath.hasPrefix("gamedata/") && size > 0 { hasLooseGameFile = true }
            guard file.deletingLastPathComponent().standardizedFileURL == folder.standardizedFileURL,
                  isArchiveName(name) else { continue }
            archiveCount += 1
            archiveBytes += size
            if size == 0 { emptyArchives += 1 } else { hasNonemptyArchive = true }
            if size > 0 && header == "No X-Ray archive header available",
               let handle = try? FileHandle(forReadingFrom: file) {
                let prefix = (try? handle.read(upToCount: 8)) ?? Data()
                try? handle.close()
                header = "\(file.lastPathComponent): \(XRayArchiveInspector.headerHex(prefix))"
            }
        }
        let archiveSummary = archiveCount == 0 ? "No gamedata.db archives found" :
            "\(archiveCount) archives, \(ByteCountFormatter.string(fromByteCount: archiveBytes, countStyle: .file)), \(emptyArchives) empty"
        // A config file by itself must never appear as imported game data.
        let detected = data && (hasLooseGameFile || hasNonemptyArchive)
        return Inventory(count: count, bytes: bytes, detected: detected,
                         diagnostics: diagnostics, archiveCount: archiveCount,
                         archiveSummary: archiveSummary, firstArchiveHeader: header)
    }
}
