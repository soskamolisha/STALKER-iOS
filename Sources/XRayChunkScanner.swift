import Foundation

/// Safe X-Ray chunk header scanner for in-memory test fixtures and future archive metadata.
/// Chunk headers are 8 bytes: little-endian ID and little-endian size.
/// The high bit of the ID is a compression flag, not an indication that the
/// payload can currently be decompressed by this app.
enum XRayChunkScanner {
    struct Chunk {
        let id: UInt32
        let isCompressed: Bool
        let payloadOffset: Int
        let payloadSize: Int
    }

    enum ScanError: LocalizedError {
        case invalidChunkLength
        case tooManyChunks
        var errorDescription: String? {
            switch self {
            case .invalidChunkLength: return "Invalid X-Ray chunk size"
            case .tooManyChunks: return "Too many X-Ray chunks"
            }
        }
    }

    static func scan(_ data: Data, maximumChunks: Int = 4096) throws -> [Chunk] {
        var reader = XRayBinaryReader(data: data)
        var chunks: [Chunk] = []
        while reader.remaining > 0 {
            guard reader.remaining >= 8 else { throw ScanError.invalidChunkLength }
            guard chunks.count < maximumChunks else { throw ScanError.tooManyChunks }
            let rawID = try reader.readUInt32LE()
            let rawLength = try reader.readUInt32LE()
            guard UInt64(rawLength) <= UInt64(reader.remaining) else {
                throw ScanError.invalidChunkLength
            }
            let length = Int(rawLength)
            chunks.append(Chunk(id: rawID & 0x7fff_ffff,
                                isCompressed: (rawID & 0x8000_0000) != 0,
                                payloadOffset: reader.offset,
                                payloadSize: length))
            try reader.skip(length)
        }
        return chunks
    }
}
