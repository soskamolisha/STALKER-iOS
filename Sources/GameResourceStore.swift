import Foundation
import UniformTypeIdentifiers

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
        let detected = Self.containsXRayResources(in: folder)
        detectedResources[gameID] = detected
        diagnostics[gameID] = Self.resourceSummary(in: folder)
        storageBytes[gameID] = Self.folderSize(in: folder)
        archiveCounts[gameID] = Self.xrayArchiveCount(in: folder)
        archiveHeaders[gameID] = Self.archiveHeaderSummary(in: folder)
        archiveSummaries[gameID] = XRayArchiveInspector.summary(XRayArchiveInspector.inspect(in: folder))
        status[gameID] = detected ? "Found \(count) files; X-Ray resource markers detected" : "Found \(count) files; X-Ray resource markers not found"
    }

    func importFolder(_ source: URL, gameID: String) throws {
        guard !importInProgress else { throw ImportError.alreadyImporting }
        importInProgress = true
        defer { importInProgress = false }
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
        let incomingBytes = Self.folderSize(in: source)
        let availableBytes = (try? root.resourceValues(forKeys: [.volumeAvailableCapacityForImportantUsageKey]))?.volumeAvailableCapacityForImportantUsage
        if let availableBytes, incomingBytes > availableBytes {
            throw ImportError.insufficientStorage
        }
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
            guard Self.containsXRayResources(in: staging) else { throw ImportError.missingGameResources }
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
            detectedResources[gameID] = true
            diagnostics[gameID] = Self.resourceSummary(in: destination)
            storageBytes[gameID] = Self.folderSize(in: destination)
            archiveCounts[gameID] = Self.xrayArchiveCount(in: destination)
            archiveHeaders[gameID] = Self.archiveHeaderSummary(in: destination)
            archiveSummaries[gameID] = XRayArchiveInspector.summary(XRayArchiveInspector.inspect(in: destination))
            status[gameID] = "Imported \(count) files; X-Ray resource markers detected"
        } catch {
            try? manager.removeItem(at: staging)
            throw error
        }
    }

    private static func archiveHeaderSummary(in folder: URL) -> String {
        guard let first = XRayArchiveInspector.inspect(in: folder, limit: 1).first else {
            return "No X-Ray archive header available"
        }
        return "\(first.name): \(XRayArchiveInspector.headerHex(first.header))"
    }

    private static func xrayArchiveCount(in folder: URL) -> Int {
        let files = FileManager.default.enumerator(at: folder, includingPropertiesForKeys: [.isRegularFileKey])
        var count = 0
        while let file = files?.nextObject() as? URL {
            let name = file.lastPathComponent.lowercased()
            if name.hasPrefix("gamedata.db"),
               (try? file.resourceValues(forKeys: [.isRegularFileKey]).isRegularFile) == true {
                count += 1
            }
        }
        return count
    }

    private static func folderSize(in folder: URL) -> Int64 {
        let enumerator = FileManager.default.enumerator(at: folder, includingPropertiesForKeys: [.isRegularFileKey, .fileSizeKey])
        var bytes: Int64 = 0
        while let file = enumerator?.nextObject() as? URL {
            guard let info = try? file.resourceValues(forKeys: [.isRegularFileKey, .fileSizeKey]),
                  info.isRegularFile == true else { continue }
            bytes += Int64(info.fileSize ?? 0)
        }
        return bytes
    }

    private static func resourceSummary(in folder: URL) -> String {
        let manager = FileManager.default
        let entries = (try? manager.contentsOfDirectory(atPath: folder.path)) ?? []
        let names = Set(entries.map { $0.lowercased() })
        let hasConfig = names.contains("fsgame.ltx")
        let hasData = names.contains("gamedata") || names.contains(where: { $0.hasPrefix("gamedata.db") })
        return "fsgame.ltx: \(hasConfig ? "yes" : "no") · gamedata: \(hasData ? "yes" : "no")"
    }

    private static func containsXRayResources(in folder: URL) -> Bool {
        let manager = FileManager.default
        let enumerator = manager.enumerator(at: folder, includingPropertiesForKeys: [.isRegularFileKey, .isDirectoryKey])
        var visited = 0
        while let item = enumerator?.nextObject() as? URL {
            visited += 1
            if visited > 200_000 { break }
            let name = item.lastPathComponent.lowercased()
            if name == "fsgame.ltx" || name == "gamedata" || name.hasPrefix("gamedata.db") {
                return true
            }
        }
        return false
    }

    enum ImportError: LocalizedError {
        case invalidEdition, invalidFolder, emptyFolder, missingGameResources, alreadyImporting, insufficientStorage
        var errorDescription: String? {
            switch self {
            case .invalidEdition: return "Unsupported game edition"
            case .alreadyImporting: return "Another import is in progress"
            case .insufficientStorage: return "Not enough free storage to import this game"
            case .invalidFolder: return "Select an extracted game folder, not an archive"
            case .emptyFolder: return "The selected folder contains no files"
            case .missingGameResources: return "No recognizable X-Ray resource files were found (fsgame.ltx, gamedata or gamedata.db*)"
            }
        }
    }
}
