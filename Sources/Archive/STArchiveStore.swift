import Foundation

/// Copies user-selected archives into the application's Documents directory.
/// Never modifies the source folder. Call off the main thread for large files.
enum STArchiveStore {
    enum ImportError: LocalizedError {
        case incompleteArchiveSet
        case noBaseArchive
        case compressedArchive
        case invalidDestination
        case insufficientSpace
        case sourceChanged
        var errorDescription: String? {
            switch self {
            case .incompleteArchiveSet: return "Набор архивов неполный: есть пропущенные, повторяющиеся, пустые или неподходящие файлы"
            case .noBaseArchive: return "В выбранной папке не найден gamedata.db0"
            case .compressedArchive: return "Найден ZIP-архив gamedata.db0.zip. Сначала распакуй его в приложении «Файлы»: X-Ray нужен исходный gamedata.db0 без .zip"
            case .invalidDestination: return "Небезопасная папка или файл для импорта"
            case .insufficientSpace: return "Недостаточно свободного места для игровых архивов"
            case .sourceChanged: return "Исходный архив изменился во время копирования; импорт остановлен"
            }
        }
    }

    static func destinationRoot(fileManager: FileManager = .default) throws -> URL {
        let documents = try fileManager.url(for: .documentDirectory, in: .userDomainMask,
                                            appropriateFor: nil, create: true)
        return documents.appendingPathComponent("ImportedGame", isDirectory: true)
    }

    /// An existing destination is never overwritten. A failed copy removes its staging folder.
    static func importArchives(from source: URL, fileManager: FileManager = .default, destinationOverride: URL? = nil) throws -> STArchiveInventory.Report {
        let granted = source.startAccessingSecurityScopedResource()
        defer { if granted { source.stopAccessingSecurityScopedResource() } }

        // Accept either Data itself or the enclosing StalkerMobile.app folder.
        let nestedData = source.appendingPathComponent("Data", isDirectory: true)
        let nestedValues = try? nestedData.resourceValues(forKeys: [.isDirectoryKey, .isSymbolicLinkKey])
        let gameSource = (nestedValues?.isDirectory == true && nestedValues?.isSymbolicLink == false)
            ? nestedData : source
        let report = try STArchiveInventory.scan(directory: gameSource)
        if !report.hasBaseArchive {
            let entries = try fileManager.contentsOfDirectory(atPath: gameSource.path)
            if entries.contains(where: { $0.lowercased() == "gamedata.db0.zip" }) {
                throw ImportError.compressedArchive
            }
            if report.warnings.contains(where: { $0.hasPrefix("ZIP content detected in ") || $0.hasPrefix("Extract ZIP before importing:") }) {
                throw ImportError.compressedArchive
            }
            if report.warnings.contains(where: { $0.hasPrefix("Empty archive:") || $0.hasPrefix("Not a regular file:") }) {
                throw ImportError.incompleteArchiveSet
            }
            throw ImportError.noBaseArchive
        }
        guard report.missingNumbers.isEmpty,
              !report.warnings.contains(where: { $0.hasPrefix("ZIP content detected in ") || $0.hasPrefix("Extract ZIP before importing:") }),
              !report.warnings.contains(where: { $0.hasPrefix("Empty archive:") }),
              !report.warnings.contains(where: { $0.hasPrefix("Duplicate archive index:") || $0.hasPrefix("Not a regular file:") || $0.hasPrefix("Unrecognized archive name:") }) else {
            throw ImportError.incompleteArchiveSet
        }
        let destination = try destinationOverride ?? destinationRoot(fileManager: fileManager)
        let parent = destination.deletingLastPathComponent()
        // Never copy the imported folder onto itself.
        guard source.standardizedFileURL.resolvingSymlinksInPath() != destination.standardizedFileURL.resolvingSymlinksInPath() else {
            throw ImportError.invalidDestination
        }
        let requiredBytes = report.archives.reduce(UInt64(0)) { partial, archive in
            let (sum, overflow) = partial.addingReportingOverflow(archive.bytes)
            return overflow ? UInt64.max : sum
        }
        // Include the optional original config in the capacity estimate.
        // The working iOS port stores fsgame.ltx beside Data, not inside it.
        // Prefer a config inside Data when provided; otherwise use its parent.
        let dataConfig = gameSource.appendingPathComponent("fsgame.ltx", isDirectory: false)
        let parentConfig = source.appendingPathComponent("fsgame.ltx", isDirectory: false)
        let sourceConfig = fileManager.fileExists(atPath: dataConfig.path)
            ? dataConfig : parentConfig
        let configBytes = ((try? fileManager.attributesOfItem(atPath: sourceConfig.path))?[.size] as? NSNumber)?.uint64Value ?? 0
        let (totalRequired, sizeOverflow) = requiredBytes.addingReportingOverflow(configBytes)
        guard !sizeOverflow else { throw ImportError.insufficientSpace }
        if let values = try? parent.resourceValues(forKeys: [.volumeAvailableCapacityKey]),
           let available = values.volumeAvailableCapacity,
           available >= 0, totalRequired > UInt64(available) {
            throw ImportError.insufficientSpace
        }
        let staging = parent.appendingPathComponent(".stalker-import-\(UUID().uuidString)", isDirectory: true)
        try fileManager.createDirectory(at: staging, withIntermediateDirectories: false)
        var committed = false
        defer { if !committed { try? fileManager.removeItem(at: staging) } }

        for archive in report.archives {
            let from = gameSource.appendingPathComponent(archive.filename, isDirectory: false)
            let to = staging.appendingPathComponent(archive.filename, isDirectory: false)
            // Recheck each entry after inventory scanning. A file provider
            // may replace an archive with a symlink before the copy starts.
            let entry = try from.resourceValues(forKeys: [.isSymbolicLinkKey, .isRegularFileKey])
            guard entry.isSymbolicLink == false, entry.isRegularFile == true else {
                throw ImportError.sourceChanged
            }
            let attributes = try fileManager.attributesOfItem(atPath: from.path)
            guard attributes[.type] as? FileAttributeType == .typeRegular else {
                throw ImportError.invalidDestination
            }
            let originalSize = (attributes[.size] as? NSNumber)?.uint64Value
            guard originalSize == archive.bytes else { throw ImportError.sourceChanged }
            let originalModified = attributes[.modificationDate] as? Date
            try fileManager.copyItem(at: from, to: to)
            let copiedSize = (try fileManager.attributesOfItem(atPath: to.path)[.size] as? NSNumber)?.uint64Value
            let currentAttributes = try fileManager.attributesOfItem(atPath: from.path)
            let currentSourceSize = (currentAttributes[.size] as? NSNumber)?.uint64Value
            let currentModified = currentAttributes[.modificationDate] as? Date
            guard originalModified == currentModified else { throw ImportError.sourceChanged }
            guard currentSourceSize == archive.bytes else { throw ImportError.sourceChanged }
            guard copiedSize == archive.bytes else {
                throw CocoaError(.fileReadCorruptFile)
            }
        }

        // Preserve the port's loose configs, levels and textures as well.
        let looseSource = gameSource.appendingPathComponent("gamedata", isDirectory: true)
        if fileManager.fileExists(atPath: looseSource.path) {
            let rootValues = try looseSource.resourceValues(forKeys: [.isDirectoryKey, .isSymbolicLinkKey])
            guard rootValues.isDirectory == true, rootValues.isSymbolicLink == false else {
                throw ImportError.invalidDestination
            }
            guard let enumerator = fileManager.enumerator(at: looseSource,
                includingPropertiesForKeys: [.isSymbolicLinkKey, .isDirectoryKey, .isRegularFileKey],
                options: [], errorHandler: nil) else { throw ImportError.invalidDestination }
            var looseBytes: UInt64 = 0
            for case let entry as URL in enumerator {
                let values = try entry.resourceValues(forKeys: [
                    .isSymbolicLinkKey, .isDirectoryKey, .isRegularFileKey, .fileSizeKey
                ])
                guard values.isSymbolicLink == false,
                    values.isDirectory == true || values.isRegularFile == true else {
                    throw ImportError.invalidDestination
                }
                if values.isRegularFile == true {
                    let size = UInt64(max(0, values.fileSize ?? 0))
                    let (next, overflow) = looseBytes.addingReportingOverflow(size)
                    guard !overflow else { throw ImportError.insufficientSpace }
                    looseBytes = next
                }
            }
            if let values = try? parent.resourceValues(forKeys: [.volumeAvailableCapacityKey]),
               let available = values.volumeAvailableCapacity,
               available >= 0, looseBytes > UInt64(available) {
                throw ImportError.insufficientSpace
            }
            try fileManager.copyItem(at: looseSource,
                to: staging.appendingPathComponent("gamedata", isDirectory: true))
        }

        // Preserve the original X-Ray filesystem configuration alongside archives.
        // The native runtime preflight requires fsgame.ltx at ImportedGame root.
        // Do not follow symlinks or silently import an empty configuration.
        let configSource = sourceConfig
        // Check the directory entry itself: FileManager attributes may follow a
        // symbolic link, which must not be accepted as an imported config.
        let sourceEntries = try fileManager.contentsOfDirectory(atPath: configSource.deletingLastPathComponent().path)
        if sourceEntries.contains("fsgame.ltx") {
            let configEntry = try configSource.resourceValues(forKeys: [.isSymbolicLinkKey, .isRegularFileKey])
            guard configEntry.isSymbolicLink == false,
                  configEntry.isRegularFile == true else {
                throw ImportError.invalidDestination
            }
            let configAttributes = try fileManager.attributesOfItem(atPath: configSource.path)
            guard configAttributes[.type] as? FileAttributeType == .typeRegular,
                  let configSize = (configAttributes[.size] as? NSNumber)?.uint64Value,
                  configSize > 0 else {
                throw ImportError.invalidDestination
            }
            let configModified = configAttributes[.modificationDate] as? Date
            let configDestination = staging.appendingPathComponent("fsgame.ltx", isDirectory: false)
            try fileManager.copyItem(at: configSource, to: configDestination)
            let copiedSize = (try fileManager.attributesOfItem(atPath: configDestination.path)[.size] as? NSNumber)?.uint64Value
            let currentAttributes = try fileManager.attributesOfItem(atPath: configSource.path)
            guard copiedSize == configSize,
                  (currentAttributes[.size] as? NSNumber)?.uint64Value == configSize,
                  (currentAttributes[.modificationDate] as? Date) == configModified else {
                throw ImportError.sourceChanged
            }
        }

        // A new archive-only import must not silently discard the valid
        // filesystem configuration from an earlier import. This matters when
        // users import large numbered archives in a separate Files operation.
        let stagedConfig = staging.appendingPathComponent("fsgame.ltx", isDirectory: false)
        let previousConfig = destination.appendingPathComponent("fsgame.ltx", isDirectory: false)
        if !fileManager.fileExists(atPath: stagedConfig.path),
           fileManager.fileExists(atPath: previousConfig.path) {
            let entry = try previousConfig.resourceValues(forKeys: [.isSymbolicLinkKey, .isRegularFileKey, .fileSizeKey])
            guard entry.isSymbolicLink == false,
                  entry.isRegularFile == true,
                  (entry.fileSize ?? 0) > 0 else {
                throw ImportError.invalidDestination
            }
            try fileManager.copyItem(at: previousConfig, to: stagedConfig)
        }

        // Preserve any previously imported game until the new set is fully staged.
        if fileManager.fileExists(atPath: destination.path) {
            let backup = parent.appendingPathComponent(".stalker-backup-\(UUID().uuidString)", isDirectory: true)
            try fileManager.moveItem(at: destination, to: backup)
            do {
                try fileManager.moveItem(at: staging, to: destination)
                committed = true
                try? fileManager.removeItem(at: backup)
            } catch {
                try? fileManager.moveItem(at: backup, to: destination)
                throw error
            }
        } else {
            try fileManager.moveItem(at: staging, to: destination)
            committed = true
        }
        return try STArchiveInventory.scan(directory: destination)
    }
}
