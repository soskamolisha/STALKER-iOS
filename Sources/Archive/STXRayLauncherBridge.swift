import Foundation

/// Connects the Swift launcher to the existing native X-Ray resource catalog.
/// This is resource inspection, NOT a playable engine implementation.
final class STXRayLauncherBridge {
    enum BridgeError: LocalizedError {
        case allocationFailed
        case initializationFailed
        case resourcesUnavailable
        case nativeFailure(Int32)

        var errorDescription: String? {
            switch self {
            case .allocationFailed: return "Не удалось создать контекст X-Ray"
            case .initializationFailed: return "Не удалось инициализировать X-Ray"
            case .resourcesUnavailable: return "Игровые ресурсы не обнаружены"
            case .nativeFailure(let code): return "Ошибка чтения X-Ray: \(code)"
            }
        }
    }

    struct Snapshot {
        let archiveNames: [String]
        let archiveBytes: UInt64
        let looseDataPresent: Bool
        let nativeEngineLinked: Bool
        let resourceEngineLinked: Bool
        /// A resource catalog is not proof of successful game startup.
        var canLaunchGame: Bool { nativeEngineLinked && resourceEngineLinked }
    }

    static func inspect(root: URL, edition: Int32 = 0) throws -> Snapshot {
        guard (0...2).contains(edition) else { throw BridgeError.initializationFailed }
        guard let handle = st_xray_create() else { throw BridgeError.allocationFailed }
        defer { st_xray_destroy(handle) }

        guard st_xray_initialize(handle, edition) == 0 else {
            throw BridgeError.initializationFailed
        }
        guard root.path.withCString({ st_xray_attach_resources(handle, $0) }) == 0 else {
            throw BridgeError.resourcesUnavailable
        }

        let count = st_xray_archive_count(handle)
        guard count >= 0 && count <= 10_000 else {
            throw BridgeError.nativeFailure(count)
        }
        var names: [String] = []
        for index in 0..<Int(count) {
            var buffer = [CChar](repeating: 0, count: 256)
            let result = buffer.withUnsafeMutableBufferPointer { pointer in
                st_xray_archive_name(handle, UInt32(index), pointer.baseAddress, UInt32(pointer.count))
            }
            guard result == 0 else { throw BridgeError.nativeFailure(result) }
            names.append(String(cString: buffer))
        }

        return Snapshot(
            archiveNames: names,
            archiveBytes: st_xray_archive_bytes(handle),
            looseDataPresent: st_xray_has_loose_data(handle) == 1,
            nativeEngineLinked: st_openxray_runtime_linked() == 1,
            resourceEngineLinked: st_openxray_runtime_resources_linked() == 1
        )
    }
}
