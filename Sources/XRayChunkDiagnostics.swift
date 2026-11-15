import Foundation

/// Diagnostic only: scans an in-memory candidate chunk stream.
/// This does not parse X-Ray gamedata.db0 archives.
enum XRayChunkDiagnostics {
    struct Chunk {
        let id: UInt32
        let payloadSize: UInt32
        let payloadOffset: Int
        let compressed: Bool
    }

    enum ScanError: Error {
        case tooLarge
        case malformed(Int)
        case tooManyChunks
    }

    static func scan(_ data: Data, maxChunks: Int = 10_000) throws -> [Chunk] {
        guard data.count <= 4 * 1024 * 1024 else { throw ScanError.tooLarge }
        guard maxChunks > 0 else { throw ScanError.tooManyChunks }
        var chunks: [Chunk] = []
        try data.withUnsafeBytes { bytes in
            let ptr = bytes.bindMemory(to: UInt8.self).baseAddress
            var offset = 0
            while true {
                var info = StalkerChunkInfo()
                let status = stalker_next_chunk(ptr, data.count, &offset, &info)
                if status == STALKER_CHUNK_END { return }
                guard status == STALKER_CHUNK_OK else {
                    throw ScanError.malformed(Int(status.rawValue))
                }
                guard chunks.count < maxChunks else { throw ScanError.tooManyChunks }
                chunks.append(Chunk(id: info.id,
                                    payloadSize: info.payload_size,
                                    payloadOffset: Int(info.payload_offset),
                                    compressed: info.compressed != 0))
            }
        }
        return chunks
    }
}
