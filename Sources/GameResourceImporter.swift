import Foundation

/// Performs long-running disk operations off the main actor.
/// The caller retains the selected security-scoped URL until this method returns.
enum GameResourceImporter {
    static func copy(_ source: URL, gameID: String) throws {
        guard ["shadow", "clearSky", "pripyat"].contains(gameID) else { throw ImportFailure.invalidEdition }
        let scoped = source.startAccessingSecurityScopedResource()
        defer { if scoped { source.stopAccessingSecurityScopedResource() } }
        let fm = FileManager.default
        var isDirectory: ObjCBool = false
        guard fm.fileExists(atPath: source.path, isDirectory: &isDirectory), isDirectory.boolValue else {
            throw ImportFailure.invalidFolder
        }
        let root = fm.urls(for: .applicationSupportDirectory, in: .userDomainMask)[0]
            .appendingPathComponent("ImportedGames", isDirectory: true)
        try fm.createDirectory(at: root, withIntermediateDirectories: true)
        let available = (try? root.resourceValues(forKeys: [.volumeAvailableCapacityForImportantUsageKey]))?.volumeAvailableCapacityForImportantUsage
        let incoming = try folderSize(source)
        // Replacement keeps the existing installation until the new copy is validated.
        // Reserve headroom for filesystem metadata and temporary writes.
        let headroom: Int64 = max(256 * 1024 * 1024, incoming / 20)
        if let available, incoming > Int64(available) - headroom {
            throw ImportFailure.insufficientStorage
        }
        let target = root.appendingPathComponent(gameID, isDirectory: true)
        let incomingFolder = root.appendingPathComponent(gameID + ".incoming", isDirectory: true)
        let backup = root.appendingPathComponent(gameID + ".backup", isDirectory: true)
        if fm.fileExists(atPath: incomingFolder.path) { try fm.removeItem(at: incomingFolder) }
        if fm.fileExists(atPath: backup.path) {
            if !fm.fileExists(atPath: target.path) { try fm.moveItem(at: backup, to: target) }
            else { try fm.removeItem(at: backup) }
        }
        do {
            try fm.copyItem(at: source, to: incomingFolder)
            guard try containsResources(incomingFolder) else { throw ImportFailure.missingResources }
            if fm.fileExists(atPath: target.path) { try fm.moveItem(at: target, to: backup) }
            do {
                try fm.moveItem(at: incomingFolder, to: target)
            } catch {
                if fm.fileExists(atPath: backup.path) { try? fm.moveItem(at: backup, to: target) }
                throw error
            }
            if fm.fileExists(atPath: backup.path) { try? fm.removeItem(at: backup) }
        } catch {
            try? fm.removeItem(at: incomingFolder)
            throw error
        }
    }

    private static func folderSize(_ url: URL) throws -> Int64 {
        let fm = FileManager.default
        guard let files = fm.enumerator(at: url, includingPropertiesForKeys: [.isRegularFileKey, .fileSizeKey]) else {
            throw ImportFailure.invalidFolder
        }
        var bytes: Int64 = 0
        while let file = files.nextObject() as? URL {
            let values = try file.resourceValues(forKeys: [.isRegularFileKey, .fileSizeKey])
            if values.isRegularFile == true { bytes += Int64(values.fileSize ?? 0) }
        }
        return bytes
    }

    private static func containsResources(_ url: URL) throws -> Bool {
        guard let files = FileManager.default.enumerator(at: url, includingPropertiesForKeys: nil) else {
            throw ImportFailure.invalidFolder
        }
        var count = 0
        while let file = files.nextObject() as? URL {
            count += 1
            if count > 200_000 { throw ImportFailure.tooManyFiles }
            let name = file.lastPathComponent.lowercased()
            if name == "fsgame.ltx" || name == "gamedata" || name.hasPrefix("gamedata.db") {
                return true
            }
        }
        return false
    }

    enum ImportFailure: LocalizedError {
        case invalidEdition, invalidFolder, insufficientStorage, missingResources, tooManyFiles
        var errorDescription: String? {
            switch self {
            case .invalidEdition: return "Unsupported game edition"
            case .invalidFolder: return "Select an extracted game folder"
            case .insufficientStorage: return "Not enough free storage on this device"
            case .missingResources: return "No recognizable X-Ray game resources found"
            case .tooManyFiles: return "Too many files to validate safely"
            }
        }
    }
}
