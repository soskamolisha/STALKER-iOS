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
    case resourcesUnavailable
    case nativeEntryFailed(Int32)
    var errorDescription: String? {
        switch self {
        case .notPorted: return "Исполняемый X-Ray пока не подключён к iOS"
        case .resourcesUnavailable: return "Не удалось открыть игровые ресурсы X-Ray"
        case .nativeEntryFailed(let code):
            switch code {
            case -6: return "Папка игровых ресурсов не существует или недоступна (-6)"
            case -5: return "Движок не поддерживает запуск с папкой игровых ресурсов (-5)"
            case -4: return "Движок вызвал исключение C++ во время запуска (-4)"
            case -3: return "Нативный движок уже выполняется (-3)"
            case -2: return "Неверные параметры запуска движка (-2)"
            case -1: return "Точка входа движка не подключена (-1)"
            default: return "Нативный мост X-Ray завершился с ошибкой \(code)"
            }
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



/// Native resource lifecycle adapter used by the launcher. This connects the
/// Swift game-engine boundary to the X-Ray C bootstrap, but intentionally does
/// not report a playable engine until the upstream runtime is linked.
final class XRayBootstrapEngine: GameEngine {
    // This reports native symbol linkage, not playable iOS compatibility.
    // Gameplay stays disabled until UIKit/Metal lifecycle integration is verified.
    let isAvailable = false
    var isNativeEntryLinked: Bool { st_openxray_runtime_linked() == 1 }
    private let gameID: String
    private var handle: STXRayHandle?
    private let nativeInput = st_input_create()

    init(gameID: String) { self.gameID = gameID }
    deinit {
        stop()
        st_input_destroy(nativeInput)
    }

    func start() throws {
        stop()
        let edition: Int32
        switch gameID {
        case "shadow": edition = 0
        case "clearSky": edition = 1
        case "pripyat": edition = 2
        default: throw EngineStartError.resourcesUnavailable
        }
        guard let core = st_xray_create() else {
            throw EngineStartError.resourcesUnavailable
        }
        handle = core
        // The current bootstrap never starts a playable runtime. Always
        // release the native handle when this preflight attempt exits.
        defer { stop() }
        guard st_xray_initialize(core, edition) == 0 else {
            throw EngineStartError.resourcesUnavailable
        }
        let root = FileManager.default.urls(for: .applicationSupportDirectory,
                                             in: .userDomainMask)[0]
            .appendingPathComponent("ImportedGames", isDirectory: true)
            .appendingPathComponent(gameID, isDirectory: true)
        guard root.path.withCString({ st_xray_attach_resources(core, $0) }) == 0 else {
            throw EngineStartError.resourcesUnavailable
        }
        // A successful attach only means the resource root was recognized.
        // Require at least one actual game-data source before proceeding.
        guard st_xray_has_loose_data(core) == 1 ||
              st_xray_has_packed_data(core) == 1 else {
            throw EngineStartError.resourcesUnavailable
        }
        // Resource bootstrap is successful; the actual render/game loop is
        // not part of this target yet. Never claim a successful game launch.
        throw EngineStartError.notPorted
    }

    /// Experimental native entry invocation. Never run on the UI thread.
    /// This is not a verified gameplay path: SDL/UIKit integration is pending.
    /// Call only after resource preflight and explicit user opt-in.
    func runEmbeddedEntryForDiagnostics() async throws -> Int32 {
        guard st_openxray_runtime_resources_linked() == 1 else { throw EngineStartError.notPorted }
        // Archive probing performs synchronous disk reads; keep it off the
        // SwiftUI main actor just like the blocking native entry point.
        let editionID = gameID
        let root = FileManager.default.urls(for: .applicationSupportDirectory,
                                            in: .userDomainMask)[0]
            .appendingPathComponent("ImportedGames", isDirectory: true)
            .appendingPathComponent(editionID, isDirectory: true)
        let hasData = await Task.detached(priority: .userInitiated) {
            XRayResourcePreflight.inspect(gameID: editionID)?.hasRecognizedGameData == true
        }.value
        guard hasData else { throw EngineStartError.resourcesUnavailable }
        // Keep the resource root explicit for the future embedded-runtime
        // configuration bridge. Do not change the process working directory:
        // it is global to the entire iOS application.
        guard FileManager.default.fileExists(atPath: root.path) else {
            throw EngineStartError.resourcesUnavailable
        }
        // The upstream X-Ray filesystem requires a game-specific fsgame.ltx.
        // Recognized gamedata archives alone are insufficient for mounting.
        let config = root.appendingPathComponent("fsgame.ltx", isDirectory: false)
        guard let configInfo = try? config.resourceValues(forKeys: [.isRegularFileKey, .isSymbolicLinkKey, .fileSizeKey]),
              configInfo.isRegularFile == true,
              configInfo.isSymbolicLink != true,
              (configInfo.fileSize ?? 0) > 0 else {
            throw EngineStartError.resourcesUnavailable
        }
        // The embedded C entry is synchronous and may block indefinitely.
        // Do not capture the Swift engine or any UIKit state on this worker.
        // Do not pass a synthetic -stalker-ios flag: upstream OpenXRay
        // does not define it. The empty command line uses upstream defaults.
        // Requires the engine to export the resource-aware ABI; otherwise
        // the bridge returns -5 rather than launching with the wrong data.
        let result = await Task.detached(priority: .userInitiated) {
            root.path.withCString { rootPointer in
                "".withCString { argsPointer in
                    st_openxray_runtime_run_with_resources(argsPointer, rootPointer)
                }
            }
        }.value
        if (-7 ... -1).contains(result) {
            throw EngineStartError.nativeEntryFailed(result)
        }
        return result
    }

    func stop() {
        if let core = handle {
            st_xray_shutdown(core)
            st_xray_destroy(core)
            handle = nil
        }
        st_input_reset(nativeInput)
    }

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
    let archiveDirectoryStatus: String?
    let archiveDirectoryCount: Int?
    let firstFilePrefix: String?
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
        var firstFilePrefix: String?
        var archiveDirectoryStatus: String?
        var archiveDirectoryCount: Int?
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
            let directoryCountResult = st_xray_archive_directory_count(engine, 0)
            if directoryCountResult >= 0 {
                archiveDirectoryCount = Int(directoryCountResult)
            }
            let directoryResult = st_xray_archive_directory_entry(engine, 0, 0, &name,
                                                                   UInt32(name.count),
                                                                   &realSize, &compressedSize,
                                                                   &crc, &offset)
            switch directoryResult {
            case 0:
                firstArchiveEntry = "\(String(cString: name)) (\(realSize) / \(compressedSize) bytes)"
                archiveDirectoryStatus = "Каталог читается"
                if realSize == compressedSize && realSize > 0 {
                    var fileBytes = [UInt8](repeating: 0, count: min(Int(realSize), 16))
                    let readResult = fileBytes.withUnsafeMutableBytes { buffer in
                        st_xray_archive_file_read(engine, 0, 0, 0, buffer.baseAddress,
                                                  UInt32(buffer.count))
                    }
                    if readResult == 0 {
                        firstFilePrefix = fileBytes.map { String(format: "%02X", $0) }.joined()
                    }
                }
            case -4:
                // Decode compressed/encrypted X-Ray directory into bounded native memory.
                // This is a resource probe, not an engine startup or archive mount.
                let maxCatalogBytes = 8 * 1024 * 1024
                var catalog = [UInt8](repeating: 0, count: maxCatalogBytes)
                var decodedSize: UInt32 = 0
                var detectedKey: Int32 = -2
                let decodeStatus = catalog.withUnsafeMutableBytes { bytes in
                    st_xray_archive_directory_auto_decrypt(
                        engine, 0, bytes.baseAddress, UInt32(bytes.count),
                        &decodedSize, &detectedKey)
                }
                if decodeStatus == 0 {
                    archiveDirectoryCount = catalog.withUnsafeBytes { bytes in
                        let result = st_xray_validate_directory_records(bytes.baseAddress, decodedSize)
                        return result >= 0 ? Int(result) : nil
                    }
                    let cipher = detectedKey == -1 ? "без шифрования" :
                                 detectedKey == 0 ? "международный ключ" : "русский ключ"
                    archiveDirectoryStatus = "Каталог распакован (\(cipher), \(decodedSize) байт)"
                } else {
                    archiveDirectoryStatus = "Сжатый каталог не декодирован (код \(decodeStatus))"
                }
            case -3:
                archiveDirectoryStatus = "Некорректная структура каталога"
            case -2:
                archiveDirectoryStatus = "Каталог файлов не найден"
            default:
                archiveDirectoryStatus = "Каталог недоступен (код \(directoryResult))"
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
            archiveDirectoryStatus: archiveDirectoryStatus,
            archiveDirectoryCount: archiveDirectoryCount,
            firstFilePrefix: firstFilePrefix,
            looseLevelCount: Int(st_xray_level_count(engine)),
            hasLooseData: st_xray_has_loose_data(engine) != 0,
            hasPackedData: st_xray_has_packed_data(engine) != 0
        )
    }
}
