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
        // Reject importing our own managed directory (or a descendant) to avoid
        // recursive copies, accidental replacement, and storage exhaustion.
        guard isSafeImportSource(source, managedRoot: root) else {
            throw ImportFailure.managedFolderSelected
        }
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
            try validateEdition(in: incomingFolder, selected: gameID)
            if fm.fileExists(atPath: target.path) { try fm.moveItem(at: target, to: backup) }
            do {
                try fm.moveItem(at: incomingFolder, to: target)
            } catch {
                // Do not hide a failed rollback behind the original move error.
                if fm.fileExists(atPath: backup.path) {
                    do {
                        try fm.moveItem(at: backup, to: target)
                    } catch {
                        throw ImportFailure.restoreFailed
                    }
                }
                throw error
            }
            // The replacement is committed. A cleanup failure must not
            // report the import as failed or delete the newly installed game.
            if fm.fileExists(atPath: backup.path) { try? fm.removeItem(at: backup) }
        } catch {
            try? fm.removeItem(at: incomingFolder)
            throw error
        }
    }

    /// Returns false for the managed root, its descendants, and its ancestors.
    /// Canonical paths prevent simple symlink-based bypasses.
    static func isSafeImportSource(_ source: URL, managedRoot: URL) -> Bool {
        let sourcePath = source.standardizedFileURL.resolvingSymlinksInPath().path
        let managedPath = managedRoot.standardizedFileURL.resolvingSymlinksInPath().path
        return sourcePath != managedPath &&
            !sourcePath.hasPrefix(managedPath + "/") &&
            !managedPath.hasPrefix(sourcePath + "/")
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

    /// Known edition-specific level folders provide a conservative mismatch warning.
    /// If no distinctive level folders are found, do not reject legitimate modded installations.
    private static func validateEdition(in folder: URL, selected: String) throws {
        let fm = FileManager.default
        let levels = folder.appendingPathComponent("gamedata/levels", isDirectory: true)
        guard let names = try? fm.contentsOfDirectory(atPath: levels.path).map({ $0.lowercased() }) else { return }
        let set = Set(names)
        let hasZaton = set.contains("zaton")
        let hasJupiter = set.contains("jupiter")
        let hasMarsh = set.contains("marsh")
        if selected != "pripyat" && (hasZaton || hasJupiter) { throw ImportFailure.editionMismatch }
        if selected == "pripyat" && hasMarsh && !hasZaton && !hasJupiter { throw ImportFailure.editionMismatch }
    }

    private static func containsResources(_ url: URL) throws -> Bool {
        guard let files = FileManager.default.enumerator(at: url, includingPropertiesForKeys: nil) else {
            throw ImportFailure.invalidFolder
        }
        var count = 0
        var foundMarker = false
        var foundFile = false
        while let file = files.nextObject() as? URL {
            count += 1
            if count > 200_000 { throw ImportFailure.tooManyFiles }
            let name = file.lastPathComponent.lowercased()
            if name == "fsgame.ltx" || name == "gamedata" || name.hasPrefix("gamedata.db") {
                foundMarker = true
            }
            if (try? file.resourceValues(forKeys: [.isRegularFileKey]).isRegularFile) == true {
                foundFile = true
            }
        }
        return foundMarker && foundFile
    }

    enum ImportFailure: LocalizedError {
        case invalidEdition, invalidFolder, insufficientStorage, missingResources, tooManyFiles, editionMismatch, managedFolderSelected, restoreFailed
        var errorDescription: String? {
            switch self {
            case .restoreFailed: return "Import failed and the previous game backup could not be restored; the backup was kept for recovery"
            case .managedFolderSelected: return "Choose a game folder outside the app-managed storage directory"
            case .invalidEdition: return "Unsupported game edition"
            case .invalidFolder: return "Select an extracted game folder"
            case .insufficientStorage: return "Not enough free storage on this device"
            case .missingResources: return "No recognizable X-Ray game resources found"
            case .tooManyFiles: return "Too many files to validate safely"
            case .editionMismatch: return "These files appear to belong to a different S.T.A.L.K.E.R. game"
            }
        }
    }
}
