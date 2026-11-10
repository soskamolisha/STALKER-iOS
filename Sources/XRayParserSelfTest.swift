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

        return Result(passed: passed, failed: failed)
    }
}
