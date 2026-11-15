import Foundation

/// Copies user-selected archives into the application's Documents directory.
/// Never modifies the source folder. Call off the main thread for large files.
enum STArchiveStore {
    enum ImportError: LocalizedError {
        case incompleteArchiveSet
        case noBaseArchive
        case invalidDestination
        case insufficientSpace
        var errorDescription: String? {
            switch self {
            case .incompleteArchiveSet: return "Archive set has missing, duplicate, or empty files"
            case .noBaseArchive: return "gamedata.db0 was not found in the selected folder"
            case .invalidDestination: return "The destination folder is not safe"
            case .insufficientSpace: return "Not enough free space for game archives"
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
        guard report.hasBaseArchive else { throw ImportError.noBaseArchive }
        guard report.missingNumbers.isEmpty,
              !report.archives.contains(where: { $0.bytes == 0 }),
              !report.warnings.contains(where: { $0.hasPrefix("Duplicate archive index:") }) else {
            throw ImportError.incompleteArchiveSet
        }
        let destination = try destinationRoot(fileManager: fileManager)
        let parent = destination.deletingLastPathComponent()
        let requiredBytes = report.archives.reduce(UInt64(0)) { partial, archive in
            let (sum, overflow) = partial.addingReportingOverflow(archive.bytes)
            return overflow ? UInt64.max : sum
        }
        if let values = try? parent.resourceValues(forKeys: [.volumeAvailableCapacityKey]),
           let available = values.volumeAvailableCapacity,
           available >= 0, requiredBytes > UInt64(available) {
            throw ImportError.insufficientSpace
        }
        let staging = parent.appendingPathComponent(".stalker-import-\(UUID().uuidString)", isDirectory: true)
        try fileManager.createDirectory(at: staging, withIntermediateDirectories: false)
        var committed = false
        defer { if !committed { try? fileManager.removeItem(at: staging) } }

        for archive in report.archives {
            let from = source.appendingPathComponent(archive.filename, isDirectory: false)
            let to = staging.appendingPathComponent(archive.filename, isDirectory: false)
            let attributes = try fileManager.attributesOfItem(atPath: from.path)
            guard attributes[.type] as? FileAttributeType == .typeRegular else {
                throw ImportError.invalidDestination
            }
            try fileManager.copyItem(at: from, to: to)
            let copiedSize = (try fileManager.attributesOfItem(atPath: to.path)[.size] as? NSNumber)?.uint64Value
            guard copiedSize == archive.bytes else {
                throw CocoaError(.fileReadCorruptFile)
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
