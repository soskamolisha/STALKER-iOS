import Foundation

/// Bounds-checked primitive reader for future X-Ray archive index decoding.
/// All offsets are relative to the supplied data buffer; no unsafe pointers.
struct XRayBinaryReader {
    enum ReadError: LocalizedError {
        case outOfBounds
        var errorDescription: String? { "Archive data is truncated or has an invalid offset" }
    }

    private let bytes: [UInt8]
    private(set) var offset: Int = 0

    init(data: Data) { bytes = Array(data) }

    var remaining: Int { bytes.count - offset }

    mutating func seek(to newOffset: Int) throws {
        guard newOffset >= 0, newOffset <= bytes.count else { throw ReadError.outOfBounds }
        offset = newOffset
    }

    mutating func readUInt16LE() throws -> UInt16 {
        let value = try readBytes(count: 2)
        return UInt16(value[0]) | (UInt16(value[1]) << 8)
    }

    mutating func readUInt32LE() throws -> UInt32 {
        let value = try readBytes(count: 4)
        return UInt32(value[0]) | (UInt32(value[1]) << 8) |
            (UInt32(value[2]) << 16) | (UInt32(value[3]) << 24)
    }

    mutating func readBytes(count: Int) throws -> [UInt8] {
        guard count >= 0, count <= remaining else { throw ReadError.outOfBounds }
        let result = Array(bytes[offset..<(offset + count)])
        offset += count
        return result
    }

    mutating func skip(_ count: Int) throws {
        guard count >= 0, count <= remaining else { throw ReadError.outOfBounds }
        offset += count
    }
}
