import Foundation

/// One resource location shared by import, cards, diagnostics and engine startup.
enum XRayGameResourcePaths {
    enum PathError: Error { case invalidEdition }
    // Both import screens may be open in the same app session.
    static let importLock = NSLock()

    /// New Shadow of Chernobyl imports are visible in the iOS Files app.
    static func destination(for gameID: String, fileManager: FileManager = .default) throws -> URL {
        guard ["shadow", "clearSky", "pripyat"].contains(gameID) else {
            throw PathError.invalidEdition
        }
        if gameID == "shadow" {
            let documents = try fileManager.url(for: .documentDirectory, in: .userDomainMask,
                                               appropriateFor: nil, create: true)
            return documents.appendingPathComponent("ImportedGame", isDirectory: true)
        }
        return try legacyRoot(for: gameID, fileManager: fileManager)
    }

    static func legacyRoot(for gameID: String, fileManager: FileManager = .default) throws -> URL {
        guard ["shadow", "clearSky", "pripyat"].contains(gameID) else {
            throw PathError.invalidEdition
        }
        let support = try fileManager.url(for: .applicationSupportDirectory, in: .userDomainMask,
                                         appropriateFor: nil, create: true)
        return support.appendingPathComponent("ImportedGames", isDirectory: true)
            .appendingPathComponent(gameID, isDirectory: true)
    }

    static func root(for gameID: String, fileManager: FileManager = .default) throws -> URL {
        let destination = try destination(for: gameID, fileManager: fileManager)
        guard gameID == "shadow" else { return destination }
        return preferredRoot(destination: destination,
                             legacy: try legacyRoot(for: gameID, fileManager: fileManager))
    }

    /// Keep older imports available without copying gigabytes on app launch.
    /// An empty Files placeholder must not hide a previous installation.
    static func preferredRoot(destination: URL, legacy: URL) -> URL {
        let entries = (try? FileManager.default.contentsOfDirectory(atPath: destination.path)) ?? []
        let hasResourceEntry = entries.contains {
            let name = $0.lowercased()
            return name == "gamedata" || name.hasPrefix("gamedata.db")
        }
        if hasResourceEntry { return destination }
        let values = try? legacy.resourceValues(forKeys: [.isDirectoryKey, .isSymbolicLinkKey])
        return values?.isDirectory == true && values?.isSymbolicLink != true ? legacy : destination
    }
}
