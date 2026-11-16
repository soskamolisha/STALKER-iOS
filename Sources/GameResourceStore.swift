import Foundation
import Combine

@MainActor
final class GameResourceStore: ObservableObject {
    @Published private(set) var status: [String: String] = [:]
    @Published private(set) var fileCounts: [String: Int] = [:]
    @Published private(set) var detectedResources: [String: Bool] = [:]
    @Published private(set) var importInProgress = false
    @Published private(set) var diagnostics: [String: String] = [:]
    @Published private(set) var storageBytes: [String: Int64] = [:]
    @Published private(set) var archiveCounts: [String: Int] = [:]
    @Published private(set) var archiveHeaders: [String: String] = [:]
    @Published private(set) var archiveSummaries: [String: String] = [:]
    private var refreshGenerations: [String: Int] = [:]

    func refresh(gameID: String) async {
        guard ["shadow", "clearSky", "pripyat"].contains(gameID) else { return }
        let generation = (refreshGenerations[gameID] ?? 0) + 1
        refreshGenerations[gameID] = generation
        guard let folder = try? XRayGameResourcePaths.root(for: gameID) else { return }
        let inventory = await Task.detached(priority: .utility) {
            GameResourceScanner.scan(folder)
        }.value
        // Returning from Files and dismissing the importer can overlap.
        // A slower old scan must not replace a newer result.
        guard refreshGenerations[gameID] == generation else { return }
        guard let inventory else {
            fileCounts.removeValue(forKey: gameID)
            detectedResources.removeValue(forKey: gameID)
            diagnostics.removeValue(forKey: gameID)
            storageBytes.removeValue(forKey: gameID)
            archiveCounts.removeValue(forKey: gameID)
            archiveHeaders.removeValue(forKey: gameID)
            archiveSummaries.removeValue(forKey: gameID)
            status.removeValue(forKey: gameID)
            return
        }
        fileCounts[gameID] = inventory.count
        detectedResources[gameID] = inventory.detected
        diagnostics[gameID] = inventory.diagnostics
        storageBytes[gameID] = inventory.bytes
        archiveCounts[gameID] = inventory.archiveCount
        archiveHeaders[gameID] = inventory.firstArchiveHeader
        archiveSummaries[gameID] = inventory.archiveSummary
        status[gameID] = inventory.detected
            ? "Found \(inventory.count) files; X-Ray resource markers detected"
            : "Found \(inventory.count) files; X-Ray resource markers not found"
    }

    func importFolder(_ source: URL, gameID: String) async throws {
        guard !importInProgress else { throw ImportError.alreadyImporting }
        importInProgress = true
        defer { importInProgress = false }
        guard ["shadow", "clearSky", "pripyat"].contains(gameID) else {
            throw ImportError.invalidEdition
        }
        try await Task.detached(priority: .userInitiated) {
            try GameResourceImporter.copy(source, gameID: gameID)
        }.value
        await refresh(gameID: gameID)
    }

    enum ImportError: LocalizedError {
        case invalidEdition, alreadyImporting
        var errorDescription: String? {
            switch self {
            case .invalidEdition: return "Unsupported game edition"
            case .alreadyImporting: return "Another import is in progress"
            }
        }
    }
}
