import Foundation
import UniformTypeIdentifiers

@MainActor
final class GameResourceStore: ObservableObject {
    @Published private(set) var status: [String: String] = [:]
    @Published private(set) var fileCounts: [String: Int] = [:]

    private var root: URL {
        FileManager.default.urls(for: .applicationSupportDirectory, in: .userDomainMask)[0]
            .appendingPathComponent("ImportedGames", isDirectory: true)
    }

    func refresh(gameID: String) {
        let folder = root.appendingPathComponent(gameID, isDirectory: true)
        guard FileManager.default.fileExists(atPath: folder.path) else { return }
        let enumerator = FileManager.default.enumerator(at: folder, includingPropertiesForKeys: [.isRegularFileKey])
        var count = 0
        while let file = enumerator?.nextObject() as? URL {
            if (try? file.resourceValues(forKeys: [.isRegularFileKey]).isRegularFile) == true { count += 1 }
        }
        fileCounts[gameID] = count
        status[gameID] = "Imported \(count) files (engine compatibility not verified)"
    }

    func importFolder(_ source: URL, gameID: String) throws {
        guard ["shadow", "clearSky", "pripyat"].contains(gameID) else {
            throw ImportError.invalidEdition
        }
        let access = source.startAccessingSecurityScopedResource()
        defer { if access { source.stopAccessingSecurityScopedResource() } }
        let manager = FileManager.default
        var isDirectory: ObjCBool = false
        guard manager.fileExists(atPath: source.path, isDirectory: &isDirectory), isDirectory.boolValue else {
            throw ImportError.invalidFolder
        }
        try manager.createDirectory(at: root, withIntermediateDirectories: true)
        let destination = root.appendingPathComponent(gameID, isDirectory: true)
        let staging = root.appendingPathComponent(gameID + ".incoming", isDirectory: true)
        let backup = root.appendingPathComponent(gameID + ".backup", isDirectory: true)
        if manager.fileExists(atPath: staging.path) { try manager.removeItem(at: staging) }
        if manager.fileExists(atPath: backup.path) {
            if !manager.fileExists(atPath: destination.path) { try manager.moveItem(at: backup, to: destination) }
            else { try manager.removeItem(at: backup) }
        }
        do {
            try manager.copyItem(at: source, to: staging)
            let files = manager.enumerator(at: staging, includingPropertiesForKeys: [.isRegularFileKey])
            var count = 0
            while let file = files?.nextObject() as? URL {
                if (try? file.resourceValues(forKeys: [.isRegularFileKey]).isRegularFile) == true {
                    count += 1
                }
            }
            guard count > 0 else { throw ImportError.emptyFolder }
            // Keep previous installation if import fails before replacement.
            if manager.fileExists(atPath: destination.path) { try manager.moveItem(at: destination, to: backup) }
            do {
                try manager.moveItem(at: staging, to: destination)
            } catch {
                if manager.fileExists(atPath: backup.path) { try? manager.moveItem(at: backup, to: destination) }
                throw error
            }
            if manager.fileExists(atPath: backup.path) { try? manager.removeItem(at: backup) }
            fileCounts[gameID] = count
            status[gameID] = "Imported \(count) files (engine compatibility not verified)"
        } catch {
            try? manager.removeItem(at: staging)
            throw error
        }
    }

    enum ImportError: LocalizedError {
        case invalidEdition, invalidFolder, emptyFolder
        var errorDescription: String? {
            switch self {
            case .invalidEdition: return "Unsupported game edition"
            case .invalidFolder: return "Select an extracted game folder, not an archive"
            case .emptyFolder: return "The selected folder contains no files"
            }
        }
    }
}
