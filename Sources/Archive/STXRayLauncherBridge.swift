import Foundation
import UIKit

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
        let directoryRecordCounts: [Int32]
        let looseDataPresent: Bool
        let nativeEngineLinked: Bool
        let resourceEngineLinked: Bool
        let preflightStatus: Int32
        /// A successful preflight only verifies prerequisites, not gameplay.
        var prerequisitesPresent: Bool {
            preflightStatus == 0 &&
                (looseDataPresent || allArchiveDirectoriesReadable) &&
                (archiveNames.isEmpty || allArchiveDirectoriesReadable)
        }
        var unreadableArchives: [String] {
            archiveNames.enumerated().compactMap { index, name in
                guard index < directoryRecordCounts.count else { return name }
                return directoryRecordCounts[index] < 0 ? name : nil
            }
        }
        var allArchiveDirectoriesReadable: Bool {
            !archiveNames.isEmpty && archiveNames.count == directoryRecordCounts.count && directoryRecordCounts.allSatisfy { $0 >= 0 }
        }
        var statusText: String {
            switch preflightStatus {
            case 0:
                if !unreadableArchives.isEmpty {
                    return "X-Ray не смог прочитать каталоги игровых архивов"
                }
                return "X-Ray: предварительная проверка пройдена; запуск игры не подтверждён"
            case -5:
                if !unreadableArchives.isEmpty {
                    return "X-Ray: ошибки каталогов архивов (\(unreadableArchives.count)); движок не подключён"
                }
                return "X-Ray: игровой движок ещё не подключён к лаунчеру"
            case -7: return "X-Ray: отсутствует или повреждён fsgame.ltx"
            case -8: return "X-Ray: не найдены игровые данные (gamedata.db0 или gamedata)"
            case -6: return "X-Ray: папка игры недоступна"
            case -9: return "X-Ray: нативный каталог не смог подключить игровые ресурсы"
            default: return "X-Ray: ошибка проверки ресурсов (\(preflightStatus))"
            }
        }
    }

    // The legacy GLES shader in some imported ports multiplies an integer
    // offset by a float. Apple GLSL ES requires an explicit conversion.
    private static func updateLegacyGatherShader(in root: URL) throws {
        guard let bundled = Bundle.main.url(forResource: "gather", withExtension: "ps") else {
            throw CocoaError(.fileNoSuchFile)
        }
        let files = FileManager.default
        var directory = root
        for component in ["gamedata", "shaders", "gl"] {
            directory.appendPathComponent(component, isDirectory: true)
            if files.fileExists(atPath: directory.path) {
                let values = try directory.resourceValues(forKeys: [.isDirectoryKey, .isSymbolicLinkKey])
                guard values.isDirectory == true, values.isSymbolicLink == false else {
                    throw CocoaError(.fileReadUnsupportedScheme)
                }
            } else {
                try files.createDirectory(at: directory, withIntermediateDirectories: false)
            }
        }
        let shader = directory.appendingPathComponent("gather.ps")
        guard files.fileExists(atPath: shader.path) else {
            try files.copyItem(at: bundled, to: shader)
            return
        }
        let values = try shader.resourceValues(forKeys: [.isRegularFileKey, .isSymbolicLinkKey, .fileSizeKey])
        guard values.isRegularFile == true, values.isSymbolicLink == false,
              let size = values.fileSize, size <= 128 * 1024 else {
            throw CocoaError(.fileReadCorruptFile)
        }
        let original = try String(contentsOf: shader, encoding: .utf8)
        guard original.contains("float4 sm_gather"), original.contains("float4 sm_minmax_gather") else { return }
        let corrected = original.replacingOccurrences(of: "rcp_scale * offset", with: "rcp_scale * float2(offset)")
        if corrected != original {
            try corrected.write(to: shader, atomically: true, encoding: .utf8)
        }
    }

    /// Invoke the real embedded engine only after it has been linked.
    /// SDL on iOS owns the game window and pumps its events on the main thread.
    @MainActor
    static func launchImportedShadowOfChernobyl(root: URL) async -> Int32 {
        let readiness: Int32 = await Task.detached(priority: .userInitiated) {
            do {
                let snapshot = try inspect(root: root)
                if snapshot.preflightStatus != 0 { return snapshot.preflightStatus }
                if !snapshot.prerequisitesPresent { return -11 }
            } catch {
                return -11
            }
            do {
                try updateLegacyGatherShader(in: root)
                return 0
            } catch {
                return -12
            }
        }.value
        guard readiness == 0 else { return readiness }
        guard let defaults = XRayLaunchProfile.arguments(for: "shadow") else { return -2 }
        let argsFile = root.appendingPathComponent("ios_args.txt")
        let arguments: String
        let argsAttributes = try? argsFile.resourceValues(forKeys: [
            .isSymbolicLinkKey, .isRegularFileKey, .fileSizeKey
        ])
        if argsAttributes?.isSymbolicLink == false,
           argsAttributes?.isRegularFile == true,
           let fileSize = argsAttributes?.fileSize,
           fileSize > 0, fileSize <= 4096,
           let bytes = try? Data(contentsOf: argsFile),
           bytes.count <= 4096, !bytes.contains(0),
           let text = String(data: bytes, encoding: .utf8),
           !text.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty {
            arguments = text.trimmingCharacters(in: .whitespacesAndNewlines)
        } else {
            arguments = defaults.joined(separator: " ")
        }
        let launcherWindow = UIApplication.shared.connectedScenes
            .compactMap { $0 as? UIWindowScene }
            .first { $0.activationState == .foregroundActive }?.keyWindow
        defer { launcherWindow?.makeKey() }
        return arguments.withCString { command in
            root.path.withCString { path in
                st_openxray_runtime_run_with_resources(command, path)
            }
        }
    }

    static func inspect(root: URL, edition: Int32 = 0) throws -> Snapshot {
        guard (0...2).contains(edition) else { throw BridgeError.initializationFailed }
        guard let handle = st_xray_create() else { throw BridgeError.allocationFailed }
        defer { st_xray_destroy(handle) }

        guard st_xray_initialize(handle, edition) == 0 else {
            throw BridgeError.initializationFailed
        }
        let preflight = root.path.withCString { st_openxray_runtime_preflight($0) }
        guard root.path.withCString({ st_xray_attach_resources(handle, $0) }) == 0 else {
            return Snapshot(archiveNames: [], archiveBytes: 0, directoryRecordCounts: [],
                            looseDataPresent: false,
                            nativeEngineLinked: st_openxray_runtime_linked() == 1,
                            resourceEngineLinked: st_openxray_runtime_resources_linked() == 1,
                            preflightStatus: preflight == 0 ? -9 : preflight)
        }

        let count = st_xray_archive_count(handle)
        guard count >= 0 && count <= 10_000 else {
            throw BridgeError.nativeFailure(count)
        }
        var names: [String] = []
        var recordCounts: [Int32] = []
        for index in 0..<Int(count) {
            var buffer = [CChar](repeating: 0, count: 256)
            let result = buffer.withUnsafeMutableBufferPointer { pointer in
                st_xray_archive_name(handle, UInt32(index), pointer.baseAddress, UInt32(pointer.count))
            }
            guard result == 0 else { throw BridgeError.nativeFailure(result) }
            names.append(String(cString: buffer))
            // Negative results are retained for diagnostics: a filename alone
            // never proves that an X-Ray archive directory is readable.
            recordCounts.append(st_xray_archive_directory_count(handle, UInt32(index)))
        }

        return Snapshot(
            archiveNames: names,
            archiveBytes: st_xray_archive_bytes(handle),
            directoryRecordCounts: recordCounts,
            looseDataPresent: st_xray_has_loose_data(handle) == 1,
            nativeEngineLinked: st_openxray_runtime_linked() == 1,
            resourceEngineLinked: st_openxray_runtime_resources_linked() == 1,
            preflightStatus: preflight
        )
    }
}
