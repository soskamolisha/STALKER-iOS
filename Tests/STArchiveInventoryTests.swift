import Foundation
import XCTest
@testable import STALKERiOSStarter

final class STArchiveInventoryTests: XCTestCase {
    private func withDirectory(_ body: (URL) throws -> Void) throws {
        let directory = FileManager.default.temporaryDirectory.appendingPathComponent(UUID().uuidString, isDirectory: true)
        try FileManager.default.createDirectory(at: directory, withIntermediateDirectories: true)
        defer { try? FileManager.default.removeItem(at: directory) }
        try body(directory)
    }

    func testEmptyDirectoryReportsMissingBase() throws {
        try withDirectory { directory in
            let report = try STArchiveInventory.scan(directory: directory)
            XCTAssertTrue(report.archives.isEmpty)
            XCTAssertFalse(report.hasBaseArchive)
        }
    }

    func testContiguousArchives() throws {
        try withDirectory { directory in
            for name in ["gamedata.db0", "gamedata.db1"] {
                try Data([0x01, 0x02]).write(to: directory.appendingPathComponent(name))
            }
            let report = try STArchiveInventory.scan(directory: directory)
            XCTAssertEqual(report.archives.map(\.number), [0, 1])
            XCTAssertTrue(report.missingNumbers.isEmpty)
            XCTAssertTrue(report.hasBaseArchive)
            XCTAssertFalse(report.isEngineReady)
        }
    }

    func testMissingIndex() throws {
        try withDirectory { directory in
            try Data([1]).write(to: directory.appendingPathComponent("gamedata.db0"))
            try Data([1]).write(to: directory.appendingPathComponent("gamedata.db2"))
            XCTAssertEqual(try STArchiveInventory.scan(directory: directory).missingNumbers, [1])
        }
    }

    func testRejectsSymlink() throws {
        try withDirectory { directory in
            let target = directory.appendingPathComponent("target")
            try Data([1]).write(to: target)
            try FileManager.default.createSymbolicLink(at: directory.appendingPathComponent("gamedata.db0"), withDestinationURL: target)
            XCTAssertFalse(try STArchiveInventory.scan(directory: directory).hasBaseArchive)
        }
    }
    func testZipIsNotCountedAsGameArchive() throws {
        try withDirectory { directory in
            try Data([0x50, 0x4B, 0x03, 0x04]).write(
                to: directory.appendingPathComponent("gamedata.db0.zip"))
            let report = try STArchiveInventory.scan(directory: directory)
            XCTAssertFalse(report.hasBaseArchive)
            XCTAssertTrue(report.warnings.contains { $0.contains("Extract ZIP") })
        }
    }

    func testEmptyArchiveIsNotCountedAsBase() throws {
        try withDirectory { directory in
            try Data().write(to: directory.appendingPathComponent("gamedata.db0"))
            let report = try STArchiveInventory.scan(directory: directory)
            XCTAssertFalse(report.hasBaseArchive)
            XCTAssertTrue(report.warnings.contains { $0.contains("Empty archive") })
        }
    }

    func testEmptyDuplicateDoesNotHideValidBase() throws {
        try withDirectory { directory in
            try Data().write(to: directory.appendingPathComponent("gamedata.db0"))
            try Data([1, 2, 3]).write(to: directory.appendingPathComponent("gamedata.db00"))
            let report = try STArchiveInventory.scan(directory: directory)
            XCTAssertTrue(report.hasBaseArchive)
            XCTAssertEqual(report.archives.count, 1)
        }
    }
    func testRenamedZipIsNotAcceptedAsDb0() throws {
        try withDirectory { directory in
            try Data([0x50, 0x4B, 0x03, 0x04, 0x00]).write(
                to: directory.appendingPathComponent("gamedata.db0"))
            let report = try STArchiveInventory.scan(directory: directory)
            XCTAssertFalse(report.hasBaseArchive)
            XCTAssertTrue(report.warnings.contains { $0.contains("ZIP content") })
        }
    }

    func testZipSignatureDoesNotMaskAnotherValidCandidate() throws {
        try withDirectory { directory in
            try Data([0x50, 0x4B, 0x03, 0x04]).write(
                to: directory.appendingPathComponent("gamedata.db0"))
            try Data([0x01, 0x02, 0x03, 0x04]).write(
                to: directory.appendingPathComponent("gamedata.db00"))
            let report = try STArchiveInventory.scan(directory: directory)
            XCTAssertTrue(report.hasBaseArchive)
            XCTAssertEqual(report.archives.count, 1)
        }
    }
}
