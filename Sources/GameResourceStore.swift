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

    func importFolder(_ source: URL, gameID: String) throws {
        guard ["shadow", "clearSky", "pripyat"].contains(gameID) else {
            throw ImportError.invalidEdition
        }
        let access = source.startAccessingSecurityScopedResource()
        defer { if access { source.stopAccessingSecurityScopedResource() } }
        let manager = FileManager.default
        try manager.createDirectory(at: root, withIntermediateDirectories: true)
        let destination = root.appendingPathComponent(gameID, isDirectory: true)
        let staging = root.appendingPathComponent(gameID + ".incoming", isDirectory: true)
        if manager.fileExists(atPath: staging.path) { try manager.removeItem(at: staging) }
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
            if manager.fileExists(atPath: destination.path) { try manager.removeItem(at: destination) }
            try manager.moveItem(at: staging, to: destination)
            fileCounts[gameID] = count
            status[gameID] = "Imported \(count) files (engine compatibility not verified)"
        } catch {
            try? manager.removeItem(at: staging)
            throw error
        }
    }

    enum ImportError: LocalizedError {
        case invalidEdition, emptyFolder
        var errorDescription: String? {
            switch self {
            case .invalidEdition: return "Unsupported game edition"
            case .emptyFolder: return "The selected folder contains no files"
            }
        }
    }
}
