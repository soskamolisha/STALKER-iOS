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
    static func importArchives(from source: URL, fileManager: FileManager = .default) throws -> STArchiveInventory.Report {
        let granted = source.startAccessingSecurityScopedResource()
        defer { if granted { source.stopAccessingSecurityScopedResource() } }

        let report = try STArchiveInventory.scan(directory: source)
        if !report.hasBaseArchive {
            let entries = try fileManager.contentsOfDirectory(atPath: source.path)
            if entries.contains(where: { $0.lowercased() == "gamedata.db0.zip" }) {
                throw ImportError.compressedArchive
            }
            throw ImportError.noBaseArchive
        }
        guard report.missingNumbers.isEmpty,
              !report.archives.contains(where: { $0.bytes == 0 }),
              !report.warnings.contains(where: { $0.hasPrefix("Duplicate archive index:") || $0.hasPrefix("Not a regular file:") || $0.hasPrefix("Unrecognized archive name:") }) else {
            throw ImportError.incompleteArchiveSet
        }
        let destination = try destinationRoot(fileManager: fileManager)
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
        let sourceConfig = source.appendingPathComponent("fsgame.ltx", isDirectory: false)
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
            let from = source.appendingPathComponent(archive.filename, isDirectory: false)
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

        // Preserve the original X-Ray filesystem configuration alongside archives.
        // The native runtime preflight requires fsgame.ltx at ImportedGame root.
        // Do not follow symlinks or silently import an empty configuration.
        let configSource = source.appendingPathComponent("fsgame.ltx", isDirectory: false)
        // Check the directory entry itself: FileManager attributes may follow a
        // symbolic link, which must not be accepted as an imported config.
        let sourceEntries = try fileManager.contentsOfDirectory(atPath: source.path)
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
