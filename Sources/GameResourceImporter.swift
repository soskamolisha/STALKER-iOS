import Foundation

/// Performs long-running disk operations off the main actor.
enum GameResourceImporter {
    @discardableResult
    static func copy(_ source: URL, gameID: String, destinationOverride: URL? = nil) throws -> GameResourceScanner.Inventory {
        XRayGameResourcePaths.importLock.lock()
        defer { XRayGameResourcePaths.importLock.unlock() }
        guard ["shadow", "clearSky", "pripyat"].contains(gameID) else { throw ImportFailure.invalidEdition }
        let target = try destinationOverride ?? XRayGameResourcePaths.destination(for: gameID)
        let legacy = try destinationOverride ?? XRayGameResourcePaths.legacyRoot(for: gameID)
        #if os(iOS) || os(macOS)
        let scoped = source.startAccessingSecurityScopedResource()
        defer { if scoped { source.stopAccessingSecurityScopedResource() } }
        var coordinationError: NSError?
        var result: Result<GameResourceScanner.Inventory, Error>?
        NSFileCoordinator().coordinate(readingItemAt: source, options: [], error: &coordinationError) { folder in
            result = Result { try copyFolder(folder, gameID: gameID, target: target, legacy: legacy) }
        }
        if let result { return try result.get() }
        if let coordinationError { throw coordinationError }
        throw ImportFailure.invalidFolder
        #else
        return try copyFolder(source, gameID: gameID, target: target, legacy: legacy)
        #endif
    }

    static func filesInbox() throws -> URL {
        let documents = try XRayGameResourcePaths.destination(for: "shadow").deletingLastPathComponent()
        let folder = documents.appendingPathComponent("Import", isDirectory: true)
        try FileManager.default.createDirectory(at: folder, withIntermediateDirectories: true)
        return folder
    }

    private static func copyFolder(_ source: URL, gameID: String, target: URL,
                                   legacy: URL) throws -> GameResourceScanner.Inventory {
        let fm = FileManager.default
        let sourceValues = try source.resourceValues(forKeys: [.isDirectoryKey, .isSymbolicLinkKey])
        guard sourceValues.isDirectory == true, sourceValues.isSymbolicLink != true else {
            throw ImportFailure.invalidFolder
        }
        let root = target.deletingLastPathComponent()
        let incomingFolder = root.appendingPathComponent(target.lastPathComponent + ".incoming", isDirectory: true)
        let backup = root.appendingPathComponent(target.lastPathComponent + ".backup", isDirectory: true)
        guard [target, legacy, incomingFolder, backup].allSatisfy({ isSafeImportSource(source, managedRoot: $0) }) else {
            throw ImportFailure.managedFolderSelected
        }
        let game = try findGameFolder(in: source)
        let looseFolder = game.lastPathComponent.lowercased() == "gamedata"
        let incoming = try folderSize(game)
        try fm.createDirectory(at: root, withIntermediateDirectories: true)
        // Replacement keeps the existing installation until the new copy is validated.
        let headroom: Int64 = max(256 * 1024 * 1024, incoming / 20)
        if let available = try? root.resourceValues(forKeys: [.volumeAvailableCapacityKey]).volumeAvailableCapacity,
           incoming > max(0, Int64(available) - headroom) {
            throw ImportFailure.insufficientStorage
        }
        if fm.fileExists(atPath: incomingFolder.path) { try fm.removeItem(at: incomingFolder) }
        if fm.fileExists(atPath: backup.path) {
            if !fm.fileExists(atPath: target.path) { try fm.moveItem(at: backup, to: target) }
            else { try fm.removeItem(at: backup) }
        }
        do {
            if looseFolder {
                try fm.createDirectory(at: incomingFolder, withIntermediateDirectories: false)
                try fm.copyItem(at: game, to: incomingFolder.appendingPathComponent("gamedata", isDirectory: true))
            } else {
                try fm.copyItem(at: game, to: incomingFolder)
            }
            guard try folderSize(incomingFolder) == incoming else {
                throw ImportFailure.incompleteCopy
            }
            if looseFolder || game.lastPathComponent.lowercased() == "data" {
                try copyPortConfig(beside: game, to: incomingFolder)
            }
            try validateEdition(in: incomingFolder, selected: gameID)
            try preserveAppData(from: target, in: incomingFolder)
            try adaptFilesystemConfig(in: incomingFolder)
            guard let inventory = GameResourceScanner.scan(incomingFolder) else {
                throw ImportFailure.missingResources
            }
            guard inventory.detected else {
                throw inventory.hasCompressedArchives ? ImportFailure.compressedArchive : ImportFailure.missingResources
            }
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
            return inventory
        } catch {
            try? fm.removeItem(at: incomingFolder)
            throw error
        }
    }

    /// Import one original X-Ray archive segment without reading it into memory.
    /// A staged copy is moved into place only after its size is verified.
    static func copyArchive(_ source: URL, gameID: String) throws {
        XRayGameResourcePaths.importLock.lock()
        defer { XRayGameResourcePaths.importLock.unlock() }
        guard ["shadow", "clearSky", "pripyat"].contains(gameID) else {
            throw ImportFailure.invalidEdition
        }
        let name = source.lastPathComponent
        let lower = name.lowercased()
        guard STArchiveInventory.archiveNumber(lower) != nil else {
            throw ImportFailure.invalidArchive
        }
        #if os(iOS) || os(macOS)
        let scoped = source.startAccessingSecurityScopedResource()
        defer { if scoped { source.stopAccessingSecurityScopedResource() } }
        #endif
        let values = try source.resourceValues(forKeys: [.isRegularFileKey, .fileSizeKey])
        guard values.isRegularFile == true, let size = values.fileSize, size > 0 else {
            throw ImportFailure.invalidArchive
        }
        let fm = FileManager.default
        let target = try XRayGameResourcePaths.destination(for: gameID)
        let root = target.deletingLastPathComponent()
        guard isSafeImportSource(source, managedRoot: target),
              isSafeImportSource(source, managedRoot: try XRayGameResourcePaths.legacyRoot(for: gameID)) else {
            throw ImportFailure.managedFolderSelected
        }
        let available = (try? root.resourceValues(forKeys: [.volumeAvailableCapacityKey]))?.volumeAvailableCapacity
        if let available, Int64(size) > Int64(available) - 256 * 1024 * 1024 {
            throw ImportFailure.insufficientStorage
        }
        let targetFolder = target
        try fm.createDirectory(at: targetFolder, withIntermediateDirectories: true)
        let destination = targetFolder.appendingPathComponent(lower)
        let staging = targetFolder.appendingPathComponent("." + lower + "." + UUID().uuidString + ".incoming")
        defer { try? fm.removeItem(at: staging) }
        try fm.copyItem(at: source, to: staging)
        let staged = try staging.resourceValues(forKeys: [.isRegularFileKey, .fileSizeKey])
        guard staged.isRegularFile == true, staged.fileSize == size else {
            throw ImportFailure.invalidArchive
        }
        // Replace atomically when possible; keep the previous archive if replacement fails.
        if fm.fileExists(atPath: destination.path) {
            let backup = targetFolder.appendingPathComponent("." + lower + "." + UUID().uuidString + ".backup")
            try fm.moveItem(at: destination, to: backup)
            do {
                try fm.moveItem(at: staging, to: destination)
                try? fm.removeItem(at: backup)
            } catch {
                try? fm.moveItem(at: backup, to: destination)
                throw error
            }
        } else {
            try fm.moveItem(at: staging, to: destination)
        }
    }

    /// Edit only the ASCII save-path setting so CP1251 comments and CRLF survive.
    private static func adaptFilesystemConfig(in staging: URL) throws {
        let config = staging.appendingPathComponent("fsgame.ltx")
        let fm = FileManager.default
        guard fm.fileExists(atPath: config.path) else { return }
        // A copied symlink could point outside the staged import. Never read
        // or rewrite an external configuration through that link.
        let configValues = try config.resourceValues(forKeys: [.isSymbolicLinkKey, .isRegularFileKey, .fileSizeKey])
        guard configValues.isSymbolicLink != true, configValues.isRegularFile == true,
              let size = configValues.fileSize, size > 0, size <= 1024 * 1024 else {
            throw ImportFailure.invalidFilesystemConfig
        }
        let original = try Data(contentsOf: config)
        let bom: [UInt8] = [0xef, 0xbb, 0xbf]
        let hasBOM = original.starts(with: bom)
        var lines = original.dropFirst(hasBOM ? bom.count : 0)
            .split(separator: 10, omittingEmptySubsequences: false)
            .map { Data($0) }
        let matches = lines.indices.filter { index in
            let line = lines[index]
            guard let equals = line.firstIndex(of: 61) else { return false }
            let key = String(decoding: line[..<equals], as: UTF8.self)
            return key.trimmingCharacters(in: .whitespaces).lowercased() == "$app_data_root$"
        }
        guard matches.count == 1, let index = matches.first else { throw ImportFailure.invalidFilesystemConfig }
        let ending = lines[index].last == 13 ? "\r" : ""
        lines[index] = Data(("$app_data_root$ = true| false| $fs_root$| appdata\\" + ending).utf8)
        var bytes = hasBOM ? Data(bom) : Data()
        for (index, line) in lines.enumerated() {
            if index != 0 { bytes.append(10) }
            bytes.append(line)
        }
        // Do not overwrite the source folder; modify only the staged import.
        let appdata = staging.appendingPathComponent("appdata", isDirectory: true)
        // Never allow a preexisting symlink or regular file to redirect
        // saves/logs outside the imported game sandbox.
        if (try? appdata.resourceValues(forKeys: [.isSymbolicLinkKey, .isDirectoryKey])) != nil {
            let values = try appdata.resourceValues(forKeys: [.isSymbolicLinkKey, .isDirectoryKey])
            guard values.isSymbolicLink != true, values.isDirectory == true else {
                throw ImportFailure.invalidFilesystemConfig
            }
        }
        try fm.createDirectory(at: appdata, withIntermediateDirectories: true)
        try bytes.write(to: config, options: .atomic)
    }

    /// Find one installation inside an extracted folder or the local Import inbox.
    private static func findGameFolder(in source: URL) throws -> URL {
        let fm = FileManager.default
        let keys: Set<URLResourceKey> = [.isDirectoryKey, .isSymbolicLinkKey]
        var folders = [source]
        var foundZip = false
        for _ in 0...4 {
            var matches: [URL] = []
            var children: [URL] = []
            for folder in folders {
                let entries = try fm.contentsOfDirectory(at: folder, includingPropertiesForKeys: Array(keys),
                                                        options: [.skipsHiddenFiles])
                let names = entries.map { $0.lastPathComponent.lowercased() }
                if folder.lastPathComponent.lowercased() == "gamedata" ||
                    (folder.pathExtension.lowercased() != "app" &&
                     (names.contains("gamedata") || names.contains(where: GameResourceScanner.isArchiveName))) {
                    matches.append(folder)
                    continue
                }
                foundZip = foundZip || names.contains { $0.hasSuffix(".zip") }
                for entry in entries {
                    let values = try entry.resourceValues(forKeys: keys)
                    if values.isDirectory == true && values.isSymbolicLink != true {
                        children.append(entry)
                    }
                }
            }
            guard matches.count <= 1 else { throw ImportFailure.multipleGameFolders }
            if let match = matches.first { return match }
            guard !children.isEmpty else { break }
            guard children.count <= 256 else { throw ImportFailure.multipleGameFolders }
            folders = children
        }
        throw foundZip ? ImportFailure.compressedArchive : ImportFailure.missingResources
    }

    private static func copyPortConfig(beside game: URL, to staging: URL) throws {
        let fm = FileManager.default
        let parent = game.deletingLastPathComponent()
        // Files may give access to Data only. Use sibling files when the
        // provider makes them available to the app.
        guard let entries = try? fm.contentsOfDirectory(atPath: parent.path) else { return }
        for (name, maxBytes) in [("fsgame.ltx", 1024 * 1024), ("ios_args.txt", 4096)] {
            let destination = staging.appendingPathComponent(name)
            guard !fm.fileExists(atPath: destination.path), entries.contains(name) else { continue }
            let source = parent.appendingPathComponent(name)
            let values = try source.resourceValues(forKeys: [.isSymbolicLinkKey, .isRegularFileKey, .fileSizeKey])
            guard values.isSymbolicLink != true, values.isRegularFile == true,
                  let size = values.fileSize, size > 0, size <= maxBytes else {
                throw ImportFailure.invalidFilesystemConfig
            }
            try fm.copyItem(at: source, to: destination)
        }
    }

    private static func preserveAppData(from target: URL, in staging: URL) throws {
        let fm = FileManager.default
        let current = target.appendingPathComponent("appdata", isDirectory: true)
        guard fm.fileExists(atPath: current.path) else { return }
        let values = try current.resourceValues(forKeys: [.isDirectoryKey, .isSymbolicLinkKey])
        guard values.isDirectory == true, values.isSymbolicLink != true else {
            throw ImportFailure.invalidFilesystemConfig
        }
        _ = try folderSize(current)
        let destination = staging.appendingPathComponent("appdata", isDirectory: true)
        if fm.fileExists(atPath: destination.path) { try fm.removeItem(at: destination) }
        try fm.copyItem(at: current, to: destination)
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
        let keys: [URLResourceKey] = [.isRegularFileKey, .isDirectoryKey, .isSymbolicLinkKey, .fileSizeKey]
        var traversalError: Error?
        guard let files = fm.enumerator(at: url, includingPropertiesForKeys: keys, errorHandler: { _, error in
            traversalError = error
            return false
        }) else {
            throw ImportFailure.invalidFolder
        }
        var bytes: Int64 = 0
        while let file = files.nextObject() as? URL {
            let values = try file.resourceValues(forKeys: Set(keys))
            // A game folder must not contain links that escape the selected tree.
            // Reject links rather than following or silently dropping them.
            if values.isSymbolicLink == true { throw ImportFailure.unsafeSymlink }
            if values.isRegularFile == true {
                let size = Int64(values.fileSize ?? 0)
                let (total, overflow) = bytes.addingReportingOverflow(size)
                guard size >= 0, !overflow else { throw ImportFailure.invalidFolder }
                bytes = total
            } else if values.isDirectory != true {
                throw ImportFailure.invalidFolder
            }
        }
        if let traversalError { throw traversalError }
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
        case invalidEdition, invalidFolder, invalidArchive, unsafeSymlink, incompleteCopy, insufficientStorage
        case missingResources, multipleGameFolders, compressedArchive, tooManyFiles, editionMismatch
        case managedFolderSelected, restoreFailed, invalidFilesystemConfig

        static func message(for error: Error, russian: Bool) -> String {
            guard russian, let failure = error as? Self else { return error.localizedDescription }
            switch failure {
            case .invalidEdition, .editionMismatch: return "Для этих файлов нужна другая карточка игры."
            case .invalidFolder: return "Не удалось открыть папку игры."
            case .invalidArchive: return "Нужен исходный файл gamedata.db."
            case .unsafeSymlink: return "В папке есть ссылки на другие файлы. Выбери папку с самими файлами игры."
            case .incompleteCopy: return "Скопировались не все файлы. Убедись, что папка полностью загружена в «Файлах»."
            case .insufficientStorage: return "На iPhone не хватает места для копии игры."
            case .missingResources: return "В папке не найдены gamedata или архивы gamedata.db."
            case .multipleGameFolders: return "Найдено несколько папок. Оставь в Import одну игру или выбери её папку отдельно."
            case .compressedArchive: return "Распакуй ZIP в «Файлах», затем выбери распакованную папку."
            case .tooManyFiles: return "В папке слишком много файлов для проверки."
            case .managedFolderSelected: return "Выбери исходную папку игры за пределами ImportedGame."
            case .restoreFailed: return "Не удалось вернуть прежнюю папку игры. Резервная копия сохранена."
            case .invalidFilesystemConfig: return "Не удалось прочитать настройки fsgame.ltx или ios_args.txt."
            }
        }

        var errorDescription: String? {
            switch self {
            case .invalidFilesystemConfig: return "Invalid fsgame.ltx filesystem configuration"
            case .restoreFailed: return "Import failed and the previous game backup could not be restored; the backup was kept for recovery"
            case .managedFolderSelected: return "Choose a game folder outside the app-managed storage directory"
            case .invalidEdition: return "Unsupported game edition"
            case .invalidFolder: return "Select an extracted game folder"
            case .invalidArchive: return "Select a nonempty original gamedata.dbN file"
            case .unsafeSymlink: return "Game folder contains symbolic links; import regular files instead"
            case .incompleteCopy: return "Copied game folder is incomplete; check free space and Files provider downloads"
            case .insufficientStorage: return "Not enough free storage on this device"
            case .missingResources: return "No recognizable X-Ray game resources found"
            case .multipleGameFolders: return "Select one game folder or leave only one game in Import"
            case .compressedArchive: return "Extract the ZIP in Files, then select the extracted game folder"
            case .tooManyFiles: return "Too many files to validate safely"
            case .editionMismatch: return "These files appear to belong to a different S.T.A.L.K.E.R. game"
            }
        }
    }
}

