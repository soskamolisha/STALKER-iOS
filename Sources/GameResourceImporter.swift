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
            // Some iOS ports store the game root inside a Data/ directory.
            // Flatten only a recognized wrapper, never an arbitrary nested folder.
            try unwrapPortDataDirectory(in: incomingFolder)
            guard let inventory = GameResourceScanner.scan(incomingFolder), inventory.detected else {
                throw ImportFailure.missingResources
            }
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

    private static func unwrapPortDataDirectory(in staging: URL) throws {
        let fm = FileManager.default
        let nested = staging.appendingPathComponent("Data", isDirectory: true)
        var isDirectory: ObjCBool = false
        guard fm.fileExists(atPath: nested.path, isDirectory: &isDirectory),
              isDirectory.boolValue else { return }
        let nestedNames = Set((try fm.contentsOfDirectory(atPath: nested.path))
            .map { $0.lowercased() })
        guard nestedNames.contains("gamedata") ||
              nestedNames.contains(where: { $0.hasPrefix("gamedata.db") }) else { return }
        let rootNames = Set((try fm.contentsOfDirectory(atPath: staging.path))
            .map { $0.lowercased() })
        // Never overwrite existing files or flatten an already valid game root.
        guard !rootNames.contains("gamedata"),
              !rootNames.contains(where: { $0.hasPrefix("gamedata.db") }) else { return }
        let entries = try fm.contentsOfDirectory(at: nested, includingPropertiesForKeys: nil)
        guard entries.allSatisfy({ !fm.fileExists(atPath:
            staging.appendingPathComponent($0.lastPathComponent).path) }) else { return }
        for entry in entries {
            try fm.moveItem(at: entry, to: staging.appendingPathComponent(entry.lastPathComponent))
        }
        try fm.removeItem(at: nested)
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
        func hasLevelDirectory(_ name: String) -> Bool {
            guard set.contains(name) else { return false }
            var isDirectory: ObjCBool = false
            let level = levels.appendingPathComponent(name, isDirectory: true)
            return fm.fileExists(atPath: level.path, isDirectory: &isDirectory) && isDirectory.boolValue
        }
        let hasZaton = hasLevelDirectory("zaton")
        let hasJupiter = hasLevelDirectory("jupiter")
        let hasMarsh = hasLevelDirectory("marsh")
        let hasPripyatMarker = hasZaton || hasJupiter
        // Mixed markers can occur in mods or merged game-data folders.
        // Do not reject ambiguous content based on a single loose level name.
        guard hasPripyatMarker != hasMarsh else { return }
        if hasPripyatMarker && selected != "pripyat" { throw ImportFailure.editionMismatch }
        if hasMarsh && selected != "clearSky" { throw ImportFailure.editionMismatch }
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
