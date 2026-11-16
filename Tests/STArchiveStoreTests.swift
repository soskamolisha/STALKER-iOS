import Foundation
import XCTest
@testable import STALKERiOSStarter

final class STArchiveStoreTests: XCTestCase {
    func testImportsArchivesAndConfigIntoIsolatedDestination() throws {
        let fm = FileManager.default
        let root = fm.temporaryDirectory.appendingPathComponent(UUID().uuidString, isDirectory: true)
        let source = root.appendingPathComponent("source", isDirectory: true)
        let destination = root.appendingPathComponent("ImportedGame", isDirectory: true)
        try fm.createDirectory(at: source, withIntermediateDirectories: true)
        defer { try? fm.removeItem(at: root) }
        let archive = Data([1, 2, 3, 4])
        let config = Data("[filesystem]".utf8)
        try archive.write(to: source.appendingPathComponent("gamedata.db0"))
        try config.write(to: source.appendingPathComponent("fsgame.ltx"))
        let result = try STArchiveStore.importArchives(from: source, destinationOverride: destination)
        XCTAssertEqual(result.archives.count, 1)
        XCTAssertTrue(result.hasBaseArchive)
        XCTAssertEqual(try Data(contentsOf: destination.appendingPathComponent("gamedata.db0")), archive)
        XCTAssertEqual(try Data(contentsOf: destination.appendingPathComponent("fsgame.ltx")), config)
    }

    func testFailedImportKeepsExistingGameFiles() throws {
        let fm = FileManager.default
        let root = fm.temporaryDirectory.appendingPathComponent(UUID().uuidString, isDirectory: true)
        let source = root.appendingPathComponent("source", isDirectory: true)
        let destination = root.appendingPathComponent("ImportedGame", isDirectory: true)
        try fm.createDirectory(at: source, withIntermediateDirectories: true)
        try fm.createDirectory(at: destination, withIntermediateDirectories: true)
        defer { try? fm.removeItem(at: root) }
        let previous = Data([9, 8, 7])
        try previous.write(to: destination.appendingPathComponent("gamedata.db0"))
        try Data([1, 2]).write(to: source.appendingPathComponent("gamedata.db0"))
        try Data().write(to: source.appendingPathComponent("fsgame.ltx"))
        XCTAssertThrowsError(try STArchiveStore.importArchives(from: source, destinationOverride: destination))
        XCTAssertEqual(try Data(contentsOf: destination.appendingPathComponent("gamedata.db0")), previous)
        let leftovers = try fm.contentsOfDirectory(atPath: root.path)
        XCTAssertFalse(leftovers.contains { $0.hasPrefix(".stalker-import-") })
    }

    func testImportedFilesReachNativeXRayPreflight() throws {
        let fm = FileManager.default
        let root = fm.temporaryDirectory.appendingPathComponent(UUID().uuidString, isDirectory: true)
        let source = root.appendingPathComponent("source", isDirectory: true)
        let destination = root.appendingPathComponent("ImportedGame", isDirectory: true)
        try fm.createDirectory(at: source, withIntermediateDirectories: true)
        defer { try? fm.removeItem(at: root) }
        try Data([1, 2, 3]).write(to: source.appendingPathComponent("gamedata.db0"))
        try Data("[filesystem]".utf8).write(to: source.appendingPathComponent("fsgame.ltx"))
        _ = try STArchiveStore.importArchives(from: source, destinationOverride: destination)
        let status = destination.path.withCString { st_openxray_runtime_preflight($0) }
        // -5 means the real resource-aware engine is not linked. Zero would
        // mean the native resource preflight succeeded with an engine present.
        XCTAssertTrue(status == -5 || status == 0, "Unexpected native preflight code: \\(status)")
    }

    func testNativePreflightIdentifiesMissingConfig() throws {
        let fm = FileManager.default
        let root = fm.temporaryDirectory.appendingPathComponent(UUID().uuidString, isDirectory: true)
        try fm.createDirectory(at: root, withIntermediateDirectories: true)
        defer { try? fm.removeItem(at: root) }
        try Data([1]).write(to: root.appendingPathComponent("gamedata.db0"))
        XCTAssertEqual(root.path.withCString { st_openxray_runtime_preflight($0) }, -7)
    }

    func testNativePreflightIdentifiesMissingBaseArchive() throws {
        let fm = FileManager.default
        let root = fm.temporaryDirectory.appendingPathComponent(UUID().uuidString, isDirectory: true)
        try fm.createDirectory(at: root, withIntermediateDirectories: true)
        defer { try? fm.removeItem(at: root) }
        try Data("[filesystem]".utf8).write(to: root.appendingPathComponent("fsgame.ltx"))
        XCTAssertEqual(root.path.withCString { st_openxray_runtime_preflight($0) }, -8)
    }

    func testArchiveOnlyReimportPreservesExistingXRayConfig() throws {
        let fm = FileManager.default
        let root = fm.temporaryDirectory.appendingPathComponent(UUID().uuidString, isDirectory: true)
        let source = root.appendingPathComponent("source", isDirectory: true)
        let destination = root.appendingPathComponent("ImportedGame", isDirectory: true)
        try fm.createDirectory(at: source, withIntermediateDirectories: true)
        try fm.createDirectory(at: destination, withIntermediateDirectories: true)
        defer { try? fm.removeItem(at: root) }
        let config = Data("[filesystem]".utf8)
        try config.write(to: destination.appendingPathComponent("fsgame.ltx"))
        try Data([9]).write(to: destination.appendingPathComponent("gamedata.db0"))
        try Data([1, 2, 3]).write(to: source.appendingPathComponent("gamedata.db0"))
        _ = try STArchiveStore.importArchives(from: source, destinationOverride: destination)
        XCTAssertEqual(try Data(contentsOf: destination.appendingPathComponent("fsgame.ltx")), config)
        XCTAssertEqual(try Data(contentsOf: destination.appendingPathComponent("gamedata.db0")), Data([1, 2, 3]))
    }

    func testArchiveOnlyReimportRejectsUnsafeExistingConfig() throws {
        let fm = FileManager.default
        let root = fm.temporaryDirectory.appendingPathComponent(UUID().uuidString, isDirectory: true)
        let source = root.appendingPathComponent("source", isDirectory: true)
        let destination = root.appendingPathComponent("ImportedGame", isDirectory: true)
        try fm.createDirectory(at: source, withIntermediateDirectories: true)
        try fm.createDirectory(at: destination, withIntermediateDirectories: true)
        defer { try? fm.removeItem(at: root) }
        try Data([9]).write(to: destination.appendingPathComponent("gamedata.db0"))
        try Data().write(to: destination.appendingPathComponent("fsgame.ltx"))
        try Data([1, 2]).write(to: source.appendingPathComponent("gamedata.db0"))
        XCTAssertThrowsError(try STArchiveStore.importArchives(from: source, destinationOverride: destination))
        XCTAssertEqual(try Data(contentsOf: destination.appendingPathComponent("gamedata.db0")), Data([9]))
    }

    func testExplainsZipWrappedBaseArchive() throws {
        let manager = FileManager.default
        let folder = manager.temporaryDirectory.appendingPathComponent(UUID().uuidString)
        try manager.createDirectory(at: folder, withIntermediateDirectories: true)
        defer { try? manager.removeItem(at: folder) }
        try Data([0x50, 0x4B, 0x03, 0x04]).write(to: folder.appendingPathComponent("gamedata.db0.zip"))
        XCTAssertThrowsError(try STArchiveStore.importArchives(from: folder)) { error in
            guard case STArchiveStore.ImportError.compressedArchive = error else {
                return XCTFail("Unexpected error: \(error)")
            }
        }
    }

    func testRejectsFolderWithoutBaseArchive() throws {
        let folder = FileManager.default.temporaryDirectory.appendingPathComponent(UUID().uuidString)
        try FileManager.default.createDirectory(at: folder, withIntermediateDirectories: true)
        defer { try? FileManager.default.removeItem(at: folder) }
        try Data([1]).write(to: folder.appendingPathComponent("gamedata.db1"))
        XCTAssertThrowsError(try STArchiveStore.importArchives(from: folder)) { error in
            guard case STArchiveStore.ImportError.noBaseArchive = error else {
                return XCTFail("Unexpected error: \(error)")
            }
        }
    }

    func testDestinationLivesInDocuments() throws {
        let root = try STArchiveStore.destinationRoot()
        XCTAssertEqual(root.lastPathComponent, "ImportedGame")
        XCTAssertEqual(root.deletingLastPathComponent().lastPathComponent, "Documents")
    }
    func testRejectsMissingMiddleArchive() throws {
        let folder = FileManager.default.temporaryDirectory.appendingPathComponent(UUID().uuidString)
        try FileManager.default.createDirectory(at: folder, withIntermediateDirectories: true)
        defer { try? FileManager.default.removeItem(at: folder) }
        try Data([1]).write(to: folder.appendingPathComponent("gamedata.db0"))
        try Data([1]).write(to: folder.appendingPathComponent("gamedata.db2"))
        XCTAssertThrowsError(try STArchiveStore.importArchives(from: folder)) { error in
            guard case STArchiveStore.ImportError.incompleteArchiveSet = error else {
                return XCTFail("Unexpected error: \(error)")
            }
        }
    }

    func testRejectsEmptyBaseArchive() throws {
        let folder = FileManager.default.temporaryDirectory.appendingPathComponent(UUID().uuidString)
        try FileManager.default.createDirectory(at: folder, withIntermediateDirectories: true)
        defer { try? FileManager.default.removeItem(at: folder) }
        try Data().write(to: folder.appendingPathComponent("gamedata.db0"))
        XCTAssertThrowsError(try STArchiveStore.importArchives(from: folder)) { error in
            guard case STArchiveStore.ImportError.incompleteArchiveSet = error else {
                return XCTFail("Unexpected error: \(error)")
            }
        }
    }

    func testRejectsSymlinkedBaseArchive() throws {
        let manager = FileManager.default
        let folder = manager.temporaryDirectory.appendingPathComponent(UUID().uuidString)
        try manager.createDirectory(at: folder, withIntermediateDirectories: true)
        defer { try? manager.removeItem(at: folder) }
        let target = folder.appendingPathComponent("actual.db")
        try Data([1, 2, 3]).write(to: target)
        try manager.createSymbolicLink(at: folder.appendingPathComponent("gamedata.db0"),
                                       withDestinationURL: target)
        XCTAssertThrowsError(try STArchiveStore.importArchives(from: folder))
        XCTAssertEqual(try Data(contentsOf: target), Data([1, 2, 3]))
    }

    func testRejectsEmptyXRayConfig() throws {
        let manager = FileManager.default
        let folder = manager.temporaryDirectory.appendingPathComponent(UUID().uuidString)
        try manager.createDirectory(at: folder, withIntermediateDirectories: true)
        defer { try? manager.removeItem(at: folder) }
        try Data([1]).write(to: folder.appendingPathComponent("gamedata.db0"))
        try Data().write(to: folder.appendingPathComponent("fsgame.ltx"))
        XCTAssertThrowsError(try STArchiveStore.importArchives(from: folder)) { error in
            guard case STArchiveStore.ImportError.invalidDestination = error else {
                return XCTFail("Unexpected error: \(error)")
            }
        }
    }

    func testRejectsSymlinkedXRayConfig() throws {
        let manager = FileManager.default
        let folder = manager.temporaryDirectory.appendingPathComponent(UUID().uuidString)
        try manager.createDirectory(at: folder, withIntermediateDirectories: true)
        defer { try? manager.removeItem(at: folder) }
        try Data([1]).write(to: folder.appendingPathComponent("gamedata.db0"))
        let target = folder.appendingPathComponent("real-config.ltx")
        try Data("config".utf8).write(to: target)
        try manager.createSymbolicLink(at: folder.appendingPathComponent("fsgame.ltx"),
                                       withDestinationURL: target)
        XCTAssertThrowsError(try STArchiveStore.importArchives(from: folder))
        XCTAssertEqual(try Data(contentsOf: target), Data("config".utf8))
    }

    func testRejectsUnrecognizedArchiveSuffix() throws {
        let manager = FileManager.default
        let folder = manager.temporaryDirectory.appendingPathComponent(UUID().uuidString)
        try manager.createDirectory(at: folder, withIntermediateDirectories: true)
        defer { try? manager.removeItem(at: folder) }
        try Data([1]).write(to: folder.appendingPathComponent("gamedata.db0"))
        try Data([2]).write(to: folder.appendingPathComponent("gamedata.dbBAD"))
        XCTAssertThrowsError(try STArchiveStore.importArchives(from: folder))
    }


    func testMissingMiddleArchiveDoesNotReplaceExistingInstallation() throws {
        let fm = FileManager.default
        let root = fm.temporaryDirectory.appendingPathComponent(UUID().uuidString, isDirectory: true)
        let source = root.appendingPathComponent("source", isDirectory: true)
        let destination = root.appendingPathComponent("ImportedGame", isDirectory: true)
        try fm.createDirectory(at: source, withIntermediateDirectories: true)
        try fm.createDirectory(at: destination, withIntermediateDirectories: true)
        defer { try? fm.removeItem(at: root) }

        let original = Data([0xAA, 0xBB, 0xCC])
        try original.write(to: destination.appendingPathComponent("gamedata.db0"))
        try Data("existing config".utf8).write(to: destination.appendingPathComponent("fsgame.ltx"))
        try Data([1]).write(to: source.appendingPathComponent("gamedata.db0"))
        try Data([2]).write(to: source.appendingPathComponent("gamedata.db2"))

        XCTAssertThrowsError(try STArchiveStore.importArchives(from: source, destinationOverride: destination))
        XCTAssertEqual(try Data(contentsOf: destination.appendingPathComponent("gamedata.db0")), original)
        XCTAssertFalse(fm.fileExists(atPath: destination.appendingPathComponent("gamedata.db2").path))
    }

    func testZipWrappedArchiveDoesNotOverwriteExistingInstallation() throws {
        let fm = FileManager.default
        let root = fm.temporaryDirectory.appendingPathComponent(UUID().uuidString, isDirectory: true)
        let source = root.appendingPathComponent("source", isDirectory: true)
        let destination = root.appendingPathComponent("ImportedGame", isDirectory: true)
        try fm.createDirectory(at: source, withIntermediateDirectories: true)
        try fm.createDirectory(at: destination, withIntermediateDirectories: true)
        defer { try? fm.removeItem(at: root) }

        let original = Data([0x10, 0x20])
        try original.write(to: destination.appendingPathComponent("gamedata.db0"))
        try Data([0x50, 0x4B, 0x03, 0x04]).write(to: source.appendingPathComponent("gamedata.db0.zip"))

        XCTAssertThrowsError(try STArchiveStore.importArchives(from: source, destinationOverride: destination))
        XCTAssertEqual(try Data(contentsOf: destination.appendingPathComponent("gamedata.db0")), original)
    }


    func testNativePreflightRejectsZipInsteadOfBaseArchive() throws {
        let fm = FileManager.default
        let root = fm.temporaryDirectory.appendingPathComponent(UUID().uuidString, isDirectory: true)
        try fm.createDirectory(at: root, withIntermediateDirectories: true)
        defer { try? fm.removeItem(at: root) }
        try Data("[filesystem]".utf8).write(to: root.appendingPathComponent("fsgame.ltx"))
        try Data([0x50, 0x4B, 0x03, 0x04]).write(to: root.appendingPathComponent("gamedata.db0.zip"))
        XCTAssertEqual(root.path.withCString { st_openxray_runtime_preflight($0) }, -8)
    }

    func testNativePreflightRejectsSymlinkedBaseArchive() throws {
        let fm = FileManager.default
        let root = fm.temporaryDirectory.appendingPathComponent(UUID().uuidString, isDirectory: true)
        try fm.createDirectory(at: root, withIntermediateDirectories: true)
        defer { try? fm.removeItem(at: root) }
        try Data("[filesystem]".utf8).write(to: root.appendingPathComponent("fsgame.ltx"))
        let target = root.appendingPathComponent("real.db")
        try Data([1, 2, 3]).write(to: target)
        try fm.createSymbolicLink(at: root.appendingPathComponent("gamedata.db0"),
                                  withDestinationURL: target)
        XCTAssertEqual(root.path.withCString { st_openxray_runtime_preflight($0) }, -8)
    }

    func testNativePreflightRejectsEmptyBaseArchive() throws {
        let fm = FileManager.default
        let root = fm.temporaryDirectory.appendingPathComponent(UUID().uuidString, isDirectory: true)
        try fm.createDirectory(at: root, withIntermediateDirectories: true)
        defer { try? fm.removeItem(at: root) }
        try Data("[filesystem]".utf8).write(to: root.appendingPathComponent("fsgame.ltx"))
        try Data().write(to: root.appendingPathComponent("gamedata.db0"))
        XCTAssertEqual(root.path.withCString { st_openxray_runtime_preflight($0) }, -8)
    }

    func testRenamedZipDoesNotOverwriteInstalledGame() throws {
        let fm = FileManager.default
        let root = fm.temporaryDirectory.appendingPathComponent(UUID().uuidString)
        let source = root.appendingPathComponent("source")
        let destination = root.appendingPathComponent("ImportedGame")
        try fm.createDirectory(at: source, withIntermediateDirectories: true)
        try fm.createDirectory(at: destination, withIntermediateDirectories: true)
        defer { try? fm.removeItem(at: root) }
        let original = Data([9, 8, 7])
        try original.write(to: destination.appendingPathComponent("gamedata.db0"))
        try Data([0x50, 0x4B, 0x03, 0x04]).write(
            to: source.appendingPathComponent("gamedata.db0"))
        XCTAssertThrowsError(try STArchiveStore.importArchives(
            from: source, destinationOverride: destination)) { error in
            guard case STArchiveStore.ImportError.compressedArchive = error else {
                return XCTFail("Expected compressedArchive, got \(error)")
            }
        }
        XCTAssertEqual(try Data(contentsOf: destination.appendingPathComponent("gamedata.db0")), original)
    }

    func testMixedRealAndZipArchiveSetIsRejected() throws {
        let fm = FileManager.default
        let folder = fm.temporaryDirectory.appendingPathComponent(UUID().uuidString)
        try fm.createDirectory(at: folder, withIntermediateDirectories: true)
        defer { try? fm.removeItem(at: folder) }
        try Data([1, 2, 3]).write(to: folder.appendingPathComponent("gamedata.db0"))
        try Data([0x50, 0x4B, 0x03, 0x04]).write(to: folder.appendingPathComponent("gamedata.db1"))
        XCTAssertThrowsError(try STArchiveStore.importArchives(from: folder)) { error in
            guard case STArchiveStore.ImportError.incompleteArchiveSet = error else {
                return XCTFail("Expected incompleteArchiveSet, got \(error)")
            }
        }
    }

    func testNativePreflightRejectsRenamedZipBase() throws {
        let fm = FileManager.default
        let root = fm.temporaryDirectory.appendingPathComponent(UUID().uuidString)
        try fm.createDirectory(at: root, withIntermediateDirectories: true)
        defer { try? fm.removeItem(at: root) }
        try Data("[filesystem]".utf8).write(to: root.appendingPathComponent("fsgame.ltx"))
        try Data([0x50, 0x4B, 0x03, 0x04]).write(to: root.appendingPathComponent("gamedata.db0"))
        XCTAssertEqual(root.path.withCString { st_openxray_runtime_preflight($0) }, -8)
    }

}
