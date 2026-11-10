import Foundation

/// Self-contained checks that can run in the starter app without a separate XCTest target.
/// Uses synthetic chunk data; does not claim compatibility with game archives.
enum XRayParserSelfTest {
    struct Result {
        let passed: Int
        let failed: [String]
        var isSuccessful: Bool { failed.isEmpty }
    }

    static func run() -> Result {
        var passed = 0
        var failed: [String] = []
        func check(_ name: String, _ condition: Bool) {
            if condition { passed += 1 } else { failed.append(name) }
        }

        do {
            var reader = XRayBinaryReader(data: Data([0x34, 0x12, 0x78, 0x56, 0x34, 0x12]))
            check("UInt16 little endian", try reader.readUInt16LE() == 0x1234)
            check("UInt32 little endian", try reader.readUInt32LE() == 0x12345678)
            check("Remaining bytes", reader.remaining == 0)
        } catch { failed.append("Basic integer decoding: \(error)") }

        do {
            var reader = XRayBinaryReader(data: Data([0x01]))
            do {
                _ = try reader.readUInt32LE()
                failed.append("Truncated integer was accepted")
            } catch {
                passed += 1
            }
        }

        do {
            let bytes: [UInt8] = [0x02, 0x00, 0x00, 0x80, 0x03, 0x00, 0x00, 0x00,
                                  0xAA, 0xBB, 0xCC]
            let chunks = try XRayChunkScanner.scan(Data(bytes))
            check("Chunk count", chunks.count == 1)
            if let first = chunks.first {
                check("Chunk identifier", first.id == 2)
                check("Compression flag", first.isCompressed)
                check("Chunk bounds", first.payloadOffset == 8 && first.payloadSize == 3)
            }
        } catch { failed.append("Valid chunk scan: \(error)") }

        do {
            _ = try XRayChunkScanner.scan(Data([1, 0, 0, 0, 8, 0, 0, 0, 0xAA]))
            failed.append("Oversized chunk was accepted")
        } catch {
            passed += 1
        }

        do {
            var reader = XRayBinaryReader(data: Data([0x10, 0x20, 0x30]))
            try reader.seek(to: 3)
            check("Seek to end", reader.remaining == 0)
            do {
                try reader.seek(to: 4)
                failed.append("Out-of-range seek was accepted")
            } catch { passed += 1 }
            do {
                try reader.skip(-1)
                failed.append("Negative skip was accepted")
            } catch { passed += 1 }
        } catch { failed.append("Seek validation: \(error)") }

        do {
            _ = try XRayChunkScanner.scan(Data([0x01, 0x00, 0x00]))
            failed.append("Partial chunk header was accepted")
        } catch { passed += 1 }

        do {
            _ = try XRayChunkScanner.scan(Data([0, 0, 0, 0, 0, 0, 0, 0]), maximumChunks: 0)
            failed.append("Chunk count limit was ignored")
        } catch { passed += 1 }

        do {
            let root = FileManager.default.temporaryDirectory
                .appendingPathComponent("xray-inspector-test-" + UUID().uuidString, isDirectory: true)
            try FileManager.default.createDirectory(at: root, withIntermediateDirectories: true)
            defer { try? FileManager.default.removeItem(at: root) }
            try Data([0x01, 0x02, 0x03]).write(to: root.appendingPathComponent("gamedata.db0"))
            try Data().write(to: root.appendingPathComponent("gamedata.db1"))
            try Data([0xFF]).write(to: root.appendingPathComponent("notes.txt"))
            let archives = XRayArchiveInspector.inspect(in: root)
            check("Only game archives selected", archives.count == 2)
            check("Archive byte count", archives.reduce(Int64(0)) { $0 + $1.size } == 3)
            check("Empty archive flagged", archives.filter { $0.appearsEmpty }.count == 1)
            check("Archive header read", archives.first(where: { $0.name == "gamedata.db0" })?.header == Data([1, 2, 3]))
        } catch { failed.append("Archive inspector filesystem test: \\(error)") }

        do {
            let root = FileManager.default.temporaryDirectory
                .appendingPathComponent("xray-scanner-test-" + UUID().uuidString, isDirectory: true)
            try FileManager.default.createDirectory(at: root, withIntermediateDirectories: true)
            defer { try? FileManager.default.removeItem(at: root) }
            try Data("game".utf8).write(to: root.appendingPathComponent("fsgame.ltx"))
            try Data([0x11, 0x22]).write(to: root.appendingPathComponent("gamedata.db0"))
            try Data().write(to: root.appendingPathComponent("gamedata.db1"))
            try Data([0xFF]).write(to: root.appendingPathComponent("notes.txt"))
            if let inventory = GameResourceScanner.scan(root) {
                check("Inventory file count", inventory.count == 4)
                check("Inventory total bytes", inventory.bytes == 7)
                check("Inventory detects resources", inventory.detected)
                check("Inventory archive count", inventory.archiveCount == 2)
                check("Inventory empty archive", inventory.archiveSummary.contains("1 empty"))
                check("Inventory archive header", inventory.firstArchiveHeader.contains("11 22"))
            } else {
                failed.append("Scanner rejected existing folder")
            }
            check("Scanner rejects missing folder", GameResourceScanner.scan(root.appendingPathComponent("missing")) == nil)
        } catch { failed.append("Resource scanner fixture test: \\(error)") }

        do {
            let root = FileManager.default.temporaryDirectory
                .appendingPathComponent("xray-nested-test-" + UUID().uuidString, isDirectory: true)
            let nested = root.appendingPathComponent("gamedata/config", isDirectory: true)
            try FileManager.default.createDirectory(at: nested, withIntermediateDirectories: true)
            defer { try? FileManager.default.removeItem(at: root) }
            try Data([1, 2, 3, 4, 5]).write(to: nested.appendingPathComponent("system.ltx"))
            if let inventory = GameResourceScanner.scan(root) {
                check("Nested folder file count", inventory.count == 1)
                check("Nested folder bytes", inventory.bytes == 5)
                check("Nested gamedata marker", inventory.detected)
                check("Nested folder has no archives", inventory.archiveCount == 0)
            } else {
                failed.append("Nested resource folder not scanned")
            }
            let empty = root.appendingPathComponent("empty", isDirectory: true)
            try FileManager.default.createDirectory(at: empty, withIntermediateDirectories: true)
            if let inventory = GameResourceScanner.scan(empty) {
                check("Empty folder zero files", inventory.count == 0)
                check("Empty folder no resources", !inventory.detected)
            } else {
                failed.append("Empty folder not scanned")
            }
        } catch { failed.append("Nested folder fixture test: \\(error)") }

        return Result(passed: passed, failed: failed)
    }
}
