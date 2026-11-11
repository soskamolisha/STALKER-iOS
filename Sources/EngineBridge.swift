import Foundation

/// Boundary between the Swift UI and a future native X-Ray iOS port.
/// This repository does not yet contain the actual X-Ray engine.
protocol GameEngine {
    var isAvailable: Bool { get }
    func start() throws
    func stop()
    func handle(_ input: GameInput, pressed: Bool)
}

enum GameInput: Int32 {
    case moveLeft, moveForward, moveRight, aim, fire
}

enum EngineStartError: LocalizedError {
    case notPorted
    var errorDescription: String? {
        switch self {
        case .notPorted: return "Движок X-Ray пока не портирован на iOS"
        }
    }
}

final class UnavailableEngine: GameEngine {
    let isAvailable = false
    func start() throws { throw EngineStartError.notPorted }
    private let nativeInput = st_input_create()
    deinit { st_input_destroy(nativeInput) }
    func stop() { st_input_reset(nativeInput) }
    func handle(_ input: GameInput, pressed: Bool) {
        st_input_set(nativeInput, input.rawValue, pressed ? 1 : 0)
    }
}


/// Native X-Ray data probe. This is a resource preflight, NOT a playable engine.
struct XRayResourcePreflight {
    let archiveCount: Int
    let archiveBytes: UInt64
    let archiveNames: [String]
    let firstArchiveSignature: String?
    let firstArchiveChunk: String?
    let firstChunkPayloadPrefix: String?
    let firstArchiveEntry: String?
    let looseLevelCount: Int
    let hasLooseData: Bool
    let hasPackedData: Bool

    /// A resource check is not proof that the X-Ray runtime can start.
    var hasRecognizedGameData: Bool { hasLooseData || hasPackedData }
    var hasRenderableLooseLevels: Bool { looseLevelCount > 0 }
    var needsPackedArchiveMounting: Bool { hasPackedData && !hasRenderableLooseLevels }

    static func inspect(gameID: String) -> XRayResourcePreflight? {
        let edition: Int32
        switch gameID {
        case "shadow": edition = 0
        case "clearSky": edition = 1
        case "pripyat": edition = 2
        default: return nil
        }
        let root = FileManager.default.urls(for: .applicationSupportDirectory,
                                             in: .userDomainMask)[0]
            .appendingPathComponent("ImportedGames", isDirectory: true)
            .appendingPathComponent(gameID, isDirectory: true)
        guard FileManager.default.fileExists(atPath: root.path),
              let engine = st_xray_create() else { return nil }
        defer {
            st_xray_shutdown(engine)
            st_xray_destroy(engine)
        }
        guard st_xray_initialize(engine, edition) == 0,
              root.path.withCString({ st_xray_attach_resources(engine, $0) }) == 0 else {
            return nil
        }
        let count = max(0, Int(st_xray_archive_count(engine)))
        var archiveNames: [String] = []
        for index in 0..<min(count, 256) {
            var buffer = [CChar](repeating: 0, count: 128)
            let status = buffer.withUnsafeMutableBufferPointer { pointer in
                st_xray_archive_name(engine, UInt32(index), pointer.baseAddress, UInt32(pointer.count))
            }
            if status == 0 { archiveNames.append(String(cString: buffer)) }
        }
        var firstArchiveSignature: String?
        if count > 0 {
            // Probe a short prefix only; tiny or truncated archives must not crash diagnostics.
            for length in [16, 8, 4, 1] {
                var bytes = [UInt8](repeating: 0, count: length)
                let status = bytes.withUnsafeMutableBytes { buffer in
                    st_xray_archive_read(engine, 0, 0, buffer.baseAddress, UInt32(buffer.count))
                }
                if status == 0 {
                    firstArchiveSignature = bytes.map { String(format: "%02X", $0) }.joined()
                    break
                }
            }
        }
        var firstArchiveChunk: String?
        var firstArchiveEntry: String?
        var firstChunkPayloadPrefix: String?
        if count > 0 {
            var chunkID: UInt32 = 0
            var payloadOffset: UInt64 = 0
            var payloadSize: UInt32 = 0
            var compressed: Int32 = 0
            if st_xray_archive_chunk_info(engine, 0, 0, &chunkID, &payloadOffset,
                                          &payloadSize, &compressed) == 0 {
                firstArchiveChunk = "id=\(chunkID) offset=\(payloadOffset) size=\(payloadSize) compressed=\(compressed)"
                let prefixLength = min(Int(payloadSize), 16)
                if prefixLength > 0 {
                    var prefix = [UInt8](repeating: 0, count: prefixLength)
                    let result = prefix.withUnsafeMutableBytes { buffer in
                        st_xray_archive_chunk_read(engine, 0, 0, 0, buffer.baseAddress,
                                                   UInt32(buffer.count))
                    }
                    if result == 0 {
                        firstChunkPayloadPrefix = prefix.map { String(format: "%02X", $0) }.joined()
                    }
                }
            }
        }
        if count > 0 {
            var name = [CChar](repeating: 0, count: 512)
            var realSize: UInt32 = 0
            var compressedSize: UInt32 = 0
            var crc: UInt32 = 0
            var offset: UInt32 = 0
            if st_xray_archive_directory_entry(engine, 0, 0, &name, UInt32(name.count),
                                                &realSize, &compressedSize, &crc, &offset) == 0 {
                firstArchiveEntry = "\(String(cString: name)) (\(realSize) / \(compressedSize) bytes)"
            }
        }
        return XRayResourcePreflight(
            archiveCount: Int(st_xray_archive_count(engine)),
            archiveBytes: st_xray_archive_bytes(engine),
            archiveNames: archiveNames,
            firstArchiveSignature: firstArchiveSignature,
            firstArchiveChunk: firstArchiveChunk,
            firstChunkPayloadPrefix: firstChunkPayloadPrefix,
            firstArchiveEntry: firstArchiveEntry,
            looseLevelCount: Int(st_xray_level_count(engine)),
            hasLooseData: st_xray_has_loose_data(engine) != 0,
            hasPackedData: st_xray_has_packed_data(engine) != 0
        )
    }
}
