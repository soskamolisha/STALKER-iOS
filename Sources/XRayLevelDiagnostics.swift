import Foundation

/// Read-only native diagnostics for unpacked game levels.
/// This does not start or render the game.
enum XRayLevelDiagnostics {
    struct Level: Identifiable {
        let id: Int
        let name: String
        let chunks: Int32
    }

    struct Collision {
        let vertices: UInt32
        let triangles: UInt32
        let minimum: [Float]
        let maximum: [Float]
    }

    struct ResourceSummary {
        let loose: Bool
        let archiveCount: Int32
        let archiveBytes: UInt64
    }

    static func resourceSummary(gameID: String) -> ResourceSummary? {
        let editions: [String: Int32] = ["shadow": 0, "clearSky": 1, "pripyat": 2]
        guard let edition = editions[gameID], let handle = st_xray_create() else { return nil }
        defer { st_xray_destroy(handle) }
        guard st_xray_initialize(handle, edition) == 0 else { return nil }
        defer { st_xray_shutdown(handle) }
        guard let support = FileManager.default.urls(for: .applicationSupportDirectory,
                                                      in: .userDomainMask).first else { return nil }
        let root = support.appendingPathComponent("ImportedGames", isDirectory: true)
            .appendingPathComponent(gameID, isDirectory: true)
        guard root.path.withCString({ st_xray_attach_resources(handle, $0) }) == 0 else { return nil }
        return ResourceSummary(loose: st_xray_has_loose_data(handle) != 0,
                               archiveCount: st_xray_archive_count(handle),
                               archiveBytes: st_xray_archive_bytes(handle))
    }

    static func inspectCollision(gameID: String, levelName: String) -> Collision? {
        let editions: [String: Int32] = ["shadow": 0, "clearSky": 1, "pripyat": 2]
        guard let edition = editions[gameID], let handle = st_xray_create() else { return nil }
        defer { st_xray_destroy(handle) }
        guard st_xray_initialize(handle, edition) == 0 else { return nil }
        defer { st_xray_shutdown(handle) }
        let root = FileManager.default.urls(for: .applicationSupportDirectory, in: .userDomainMask)[0]
            .appendingPathComponent("ImportedGames", isDirectory: true)
            .appendingPathComponent(gameID, isDirectory: true)
        guard root.path.withCString({ st_xray_attach_resources(handle, $0) }) == 0 else { return nil }
        let count = st_xray_level_count(handle)
        guard count > 0 else { return nil }
        for index in 0..<min(Int(count), 4096) {
            var buffer = [CChar](repeating: 0, count: 1024)
            let status = buffer.withUnsafeMutableBufferPointer {
                st_xray_level_name(handle, UInt32(index), $0.baseAddress, UInt32($0.count))
            }
            guard status == 0, String(cString: buffer) == levelName else { continue }
            var vertices: UInt32 = 0
            var triangles: UInt32 = 0
            var minimum = [Float](repeating: 0, count: 3)
            var maximum = [Float](repeating: 0, count: 3)
            let result = minimum.withUnsafeMutableBufferPointer { minBuffer in
                maximum.withUnsafeMutableBufferPointer { maxBuffer in
                    st_xray_level_collision_info(handle, UInt32(index), &vertices, &triangles,
                                                 minBuffer.baseAddress, maxBuffer.baseAddress)
                }
            }
            guard result == 0 else { return nil }
            return Collision(vertices: vertices, triangles: triangles,
                             minimum: minimum, maximum: maximum)
        }
        return nil
    }

    struct Chunk: Identifiable {
        let id: Int
        let chunkID: UInt32
        let offset: UInt64
        let size: UInt32
        let compressed: Bool
    }

    static func inspectChunks(gameID: String, levelName: String) -> [Chunk] {
        let editions: [String: Int32] = ["shadow": 0, "clearSky": 1, "pripyat": 2]
        guard let edition = editions[gameID], let handle = st_xray_create() else { return [] }
        defer { st_xray_destroy(handle) }
        guard st_xray_initialize(handle, edition) == 0 else { return [] }
        defer { st_xray_shutdown(handle) }
        let root = FileManager.default.urls(for: .applicationSupportDirectory, in: .userDomainMask)[0]
            .appendingPathComponent("ImportedGames", isDirectory: true)
            .appendingPathComponent(gameID, isDirectory: true)
        guard root.path.withCString({ st_xray_attach_resources(handle, $0) }) == 0 else { return [] }
        let count = st_xray_level_count(handle)
        guard count > 0 else { return [] }
        for index in 0..<min(Int(count), 4096) {
            var buffer = [CChar](repeating: 0, count: 1024)
            let status = buffer.withUnsafeMutableBufferPointer {
                st_xray_level_name(handle, UInt32(index), $0.baseAddress, UInt32($0.count))
            }
            guard status == 0, String(cString: buffer) == levelName else { continue }
            let chunkCount = st_xray_level_chunk_count(handle, UInt32(index))
            guard chunkCount > 0 else { return [] }
            var chunks: [Chunk] = []
            for chunkIndex in 0..<min(Int(chunkCount), 256) {
                var chunkID: UInt32 = 0
                var offset: UInt64 = 0
                var size: UInt32 = 0
                var compressed: Int32 = 0
                guard st_xray_level_chunk_info(handle, UInt32(index), UInt32(chunkIndex),
                                               &chunkID, &offset, &size, &compressed) == 0 else { break }
                chunks.append(Chunk(id: chunkIndex, chunkID: chunkID, offset: offset,
                                    size: size, compressed: compressed != 0))
            }
            return chunks
        }
        return []
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
