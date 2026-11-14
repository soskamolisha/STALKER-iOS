import Foundation
import simd

struct CollisionPreviewMesh {
    let vertices: [SIMD3<Float>]
    let indices: [UInt32]

    static func load(gameID: String, levelName: String) -> CollisionPreviewMesh? {
        guard ["shadow", "clearSky", "pripyat"].contains(gameID),
              !levelName.isEmpty,
              !levelName.contains("/"), !levelName.contains("\\"),
              levelName != ".", levelName != ".." else { return nil }
        let root = FileManager.default.urls(for: .applicationSupportDirectory, in: .userDomainMask)[0]
            .appendingPathComponent("ImportedGames", isDirectory: true)
            .appendingPathComponent(gameID, isDirectory: true)
        let path = root.appendingPathComponent("gamedata/levels", isDirectory: true)
            .appendingPathComponent(levelName, isDirectory: true)
            .appendingPathComponent("level.cform", isDirectory: false)
        let cap = 50_000
        var xyz = [Float](repeating: 0, count: cap * 3)
        var indices = [UInt32](repeating: 0, count: cap * 3)
        var vertexCount: UInt32 = 0
        var triangleCount: UInt32 = 0
        let status = path.path.withCString { cpath in
            xyz.withUnsafeMutableBufferPointer { positions in
                indices.withUnsafeMutableBufferPointer { faces in
                    st_xray_read_collision_sample(cpath, positions.baseAddress, UInt32(cap),
                                                faces.baseAddress, UInt32(cap),
                                                &vertexCount, &triangleCount)
                }
            }
        }
        guard status == 0 else { return nil }
        let points = (0..<Int(vertexCount)).map { i in
            SIMD3<Float>(xyz[i * 3], xyz[i * 3 + 1], xyz[i * 3 + 2])
        }
        return CollisionPreviewMesh(vertices: points, indices: Array(indices.prefix(Int(triangleCount) * 3)))
    }
}
