import Foundation
import XCTest
@testable import STALKERiOSStarter

final class STArchiveStoreTests: XCTestCase {
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
                return XCTFail("Unexpected error: \\(error)")
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

}
