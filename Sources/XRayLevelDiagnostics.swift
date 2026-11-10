import Foundation

/// Read-only native diagnostics for unpacked game levels.
/// This does not start or render the game.
enum XRayLevelDiagnostics {
    struct Level: Identifiable {
        let id: Int
        let name: String
        let chunks: Int32
    }

    static func inspect(gameID: String) -> [Level] {
        let editions: [String: Int32] = ["shadow": 0, "clearSky": 1, "pripyat": 2]
        guard let edition = editions[gameID],
              let handle = st_xray_create() else { return [] }
        defer { st_xray_destroy(handle) }
        guard st_xray_initialize(handle, edition) == 0 else { return [] }
        defer { st_xray_shutdown(handle) }
        guard let support = FileManager.default.urls(for: .applicationSupportDirectory,
                                                      in: .userDomainMask).first else { return [] }
        let root = support.appendingPathComponent("ImportedGames", isDirectory: true)
            .appendingPathComponent(gameID, isDirectory: true)
        guard root.path.withCString({ st_xray_attach_resources(handle, $0) }) == 0 else { return [] }
        let count = st_xray_level_count(handle)
        guard count > 0 else { return [] }
        var result: [Level] = []
        for index in 0..<min(Int(count), 4096) {
            var buffer = [CChar](repeating: 0, count: 1024)
            let code = buffer.withUnsafeMutableBufferPointer {
                st_xray_level_name(handle, UInt32(index), $0.baseAddress, UInt32($0.count))
            }
            guard code == 0 else { continue }
            let name = String(cString: buffer)
            result.append(Level(id: index, name: name,
                                chunks: st_xray_level_chunk_count(handle, UInt32(index))))
        }
        return result
    }
}
