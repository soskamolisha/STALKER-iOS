import Foundation
import XCTest
@testable import STALKERiOSStarter

final class GameResourceIntegrationTests: XCTestCase {
    private let fm = FileManager.default

    private func withFolders(_ body: (URL, URL, URL) throws -> Void) throws {
        let root = fm.temporaryDirectory.appendingPathComponent(UUID().uuidString)
        let destination = root.appendingPathComponent("Documents/ImportedGame")
        let legacy = root.appendingPathComponent("Support/ImportedGames/shadow")
        try fm.createDirectory(at: destination, withIntermediateDirectories: true)
        defer { try? fm.removeItem(at: root) }
        try body(root, destination, legacy)
    }

    func testImportedPortIsVisibleToLauncherScanner() throws {
        try withFolders { root, destination, legacy in
            let app = root.appendingPathComponent("StalkerMobile.app")
            let data = app.appendingPathComponent("Data")
            try fm.createDirectory(at: data, withIntermediateDirectories: true)
            try Data([1, 2, 3, 4]).write(to: data.appendingPathComponent("gamedata.db0"))
            try Data("$app_data_root$ = true|false| C:\\Old Saves\\\r\n".utf8)
                .write(to: app.appendingPathComponent("fsgame.ltx"))
            try Data("-soc -nointro -nosplash".utf8).write(to: app.appendingPathComponent("ios_args.txt"))
            try Data([9]).write(to: app.appendingPathComponent("StalkerMobile"))
            for source in [app, data] {
                try GameResourceImporter.copy(source, gameID: "shadow", destinationOverride: destination)
                XCTAssertEqual(try String(contentsOf: destination.appendingPathComponent("fsgame.ltx"), encoding: .utf8),
                               "$app_data_root$ = true| false| $fs_root$| appdata\\\r\n")
                XCTAssertTrue(fm.fileExists(atPath: destination.appendingPathComponent("ios_args.txt").path))
                XCTAssertFalse(fm.fileExists(atPath: destination.appendingPathComponent("StalkerMobile").path))
            }

            let active = XRayGameResourcePaths.preferredRoot(destination: destination, legacy: legacy)
            XCTAssertEqual(active, destination)
            let inventory = try XCTUnwrap(GameResourceScanner.scan(active))
            XCTAssertTrue(inventory.detected)
            XCTAssertEqual(inventory.archiveCount, 1)
            XCTAssertEqual(inventory.count, 3)
        }
    }

    func testImportFromFilesInboxKeepsExistingSaves() throws {
        try withFolders { root, destination, _ in
            let inbox = root.appendingPathComponent("Documents/Import")
            let configs = inbox.appendingPathComponent("My Game/gamedata/config")
            let saves = destination.appendingPathComponent("appdata/savedgames")
            try fm.createDirectory(at: configs, withIntermediateDirectories: true)
            try fm.createDirectory(at: saves, withIntermediateDirectories: true)
            try Data([1]).write(to: configs.appendingPathComponent("system.ltx"))
            try Data([2]).write(to: saves.appendingPathComponent("save.sav"))
            let imported = try GameResourceImporter.copy(inbox, gameID: "shadow", destinationOverride: destination)
            XCTAssertTrue(imported.detected)
            XCTAssertEqual(try Data(contentsOf: saves.appendingPathComponent("save.sav")), Data([2]))
            XCTAssertTrue(fm.fileExists(atPath: configs.appendingPathComponent("system.ltx").path))
        }
    }

    func testMultipleFoldersAndRenamedZIPDoNotReplaceAnExistingImport() throws {
        try withFolders { root, destination, _ in
            let inbox = root.appendingPathComponent("Documents/Import")
            let existing = destination.appendingPathComponent("gamedata.db0")
            try Data([9]).write(to: existing)
            for name in ["first", "second"] {
                let game = inbox.appendingPathComponent(name)
                try fm.createDirectory(at: game, withIntermediateDirectories: true)
                try Data([0x50, 0x4b, 0x03, 0x04]).write(to: game.appendingPathComponent("gamedata.db0"))
            }
            XCTAssertThrowsError(try GameResourceImporter.copy(inbox, gameID: "shadow", destinationOverride: destination)) {
                XCTAssertEqual($0 as? GameResourceImporter.ImportFailure, .multipleGameFolders)
            }
            let first = inbox.appendingPathComponent("first")
            XCTAssertFalse(try XCTUnwrap(GameResourceScanner.scan(first)).detected)
            XCTAssertThrowsError(try GameResourceImporter.copy(first, gameID: "shadow", destinationOverride: destination)) {
                XCTAssertEqual($0 as? GameResourceImporter.ImportFailure, .compressedArchive)
            }
            XCTAssertEqual(try Data(contentsOf: existing), Data([9]))
        }
    }

    func testEmptyFilesFolderDoesNotHideLegacyImport() throws {
        try withFolders { _, destination, legacy in
            try fm.createDirectory(at: legacy, withIntermediateDirectories: true)
            try Data([1]).write(to: legacy.appendingPathComponent("gamedata.db0"))
            let active = XRayGameResourcePaths.preferredRoot(destination: destination, legacy: legacy)
            XCTAssertEqual(active, legacy)
            XCTAssertTrue(try XCTUnwrap(GameResourceScanner.scan(active)).detected)
        }
    }

    func testNewImportTakesPrecedenceWithoutDeletingLegacyFiles() throws {
        try withFolders { root, destination, legacy in
            try fm.createDirectory(at: legacy, withIntermediateDirectories: true)
            try Data([9]).write(to: legacy.appendingPathComponent("gamedata.db0"))
            let source = root.appendingPathComponent("source")
            try fm.createDirectory(at: source, withIntermediateDirectories: true)
            try Data([1, 2]).write(to: source.appendingPathComponent("gamedata.db0"))
            _ = try STArchiveStore.importArchives(from: source, destinationOverride: destination)
            XCTAssertEqual(XRayGameResourcePaths.preferredRoot(destination: destination, legacy: legacy), destination)
            XCTAssertEqual(try Data(contentsOf: legacy.appendingPathComponent("gamedata.db0")), Data([9]))
        }
    }

    func testEmptyAndConfigOnlyFoldersDoNotAppearImported() throws {
        try withFolders { _, destination, _ in
            XCTAssertFalse(try XCTUnwrap(GameResourceScanner.scan(destination)).detected)
            try Data("config".utf8).write(to: destination.appendingPathComponent("fsgame.ltx"))
            try Data().write(to: destination.appendingPathComponent("gamedata.db0"))
            try fm.createDirectory(at: destination.appendingPathComponent("gamedata"), withIntermediateDirectories: true)
            XCTAssertFalse(try XCTUnwrap(GameResourceScanner.scan(destination)).detected)
        }
    }

    func testRefreshAfterManualFileCopyAndRemoval() throws {
        try withFolders { _, destination, legacy in
            let archive = destination.appendingPathComponent("gamedata.db0")
            let active = { XRayGameResourcePaths.preferredRoot(destination: destination, legacy: legacy) }
            XCTAssertFalse(try XCTUnwrap(GameResourceScanner.scan(active())).detected)
            try Data([1, 2, 3]).write(to: archive)
            XCTAssertTrue(try XCTUnwrap(GameResourceScanner.scan(active())).detected)
            try fm.removeItem(at: archive)
            XCTAssertFalse(try XCTUnwrap(GameResourceScanner.scan(active())).detected)
        }
    }

    func testLooseResourcesWithoutPackedArchivesAreDetected() throws {
        try withFolders { _, destination, legacy in
            let configs = destination.appendingPathComponent("gamedata/config")
            try fm.createDirectory(at: configs, withIntermediateDirectories: true)
            try Data("settings".utf8).write(to: configs.appendingPathComponent("system.ltx"))
            let active = XRayGameResourcePaths.preferredRoot(destination: destination, legacy: legacy)
            let inventory = try XCTUnwrap(GameResourceScanner.scan(active))
            XCTAssertEqual(active, destination)
            XCTAssertTrue(inventory.detected)
            XCTAssertEqual(inventory.archiveCount, 0)
        }
    }

    func testDirectoryURLHintsDoNotHideRootArchivesOrCountNestedBackups() throws {
        try withFolders { _, destination, _ in
            let backup = destination.appendingPathComponent("backup", isDirectory: true)
            try fm.createDirectory(at: backup, withIntermediateDirectories: true)
            try Data([1]).write(to: destination.appendingPathComponent("gamedata.db0"))
            try Data([9]).write(to: backup.appendingPathComponent("gamedata.db1"))
            for hint in [true, false] {
                let url = URL(fileURLWithPath: destination.path, isDirectory: hint)
                let inventory = try XCTUnwrap(GameResourceScanner.scan(url))
                XCTAssertTrue(inventory.detected)
                XCTAssertEqual(inventory.archiveCount, 1)
            }
        }
    }

    func testSymlinkedArchiveDoesNotProduceImportedStatus() throws {
        try withFolders { root, destination, _ in
            let outside = root.appendingPathComponent("outside.db0")
            try Data([1, 2]).write(to: outside)
            try fm.createSymbolicLink(at: destination.appendingPathComponent("gamedata.db0"), withDestinationURL: outside)
            XCTAssertFalse(try XCTUnwrap(GameResourceScanner.scan(destination)).detected)
        }
    }

    func testArchiveAndFolderImportsShareShadowDestination() throws {
        XCTAssertEqual(try STArchiveStore.destinationRoot(),
                       try XRayGameResourcePaths.destination(for: "shadow"))
        for game in ["clearSky", "pripyat"] {
            XCTAssertEqual(try XRayGameResourcePaths.destination(for: game),
                           try XRayGameResourcePaths.legacyRoot(for: game))
            XCTAssertNotEqual(try XRayGameResourcePaths.destination(for: game),
                              try XRayGameResourcePaths.destination(for: "shadow"))
        }
        XCTAssertThrowsError(try XRayGameResourcePaths.destination(for: "../shadow"))
    }
}
