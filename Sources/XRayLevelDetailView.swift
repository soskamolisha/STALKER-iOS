import SwiftUI

struct XRayLevelDetailView: View {
    let gameID: String
    let levelName: String
    @State private var chunks: [XRayLevelDiagnostics.Chunk] = []
    @State private var loading = true
    @State private var collision: XRayLevelDiagnostics.Collision?

    var body: some View {
        List {
            if let collision {
                Section("Collision geometry") {
                    Text("Vertices: \(collision.vertices)")
                    Text("Triangles: \(collision.triangles)")
                    Text("Bounds min: \(collision.minimum.map { String(format: "%.2f", $0) }.joined(separator: ", "))")
                    Text("Bounds max: \(collision.maximum.map { String(format: "%.2f", $0) }.joined(separator: ", "))")
                }
            }
            if loading {
                ProgressView("Reading geometry index…")
            } else if chunks.isEmpty {
                ContentUnavailableView("No readable chunks", systemImage: "doc.questionmark",
                                       description: Text("Only unpacked X-Ray level files are supported."))
            } else {
                Section("First \(chunks.count) geometry chunks") {
                    ForEach(chunks) { chunk in
                        VStack(alignment: .leading, spacing: 4) {
                            HStack {
                                Text("Chunk #\(chunk.chunkID)").font(.headline.monospaced())
                                Spacer()
                                if chunk.compressed { Text("Compressed").font(.caption).foregroundStyle(.orange) }
                            }
                            Text("Offset: \(chunk.offset) · Payload: \(chunk.size) bytes")
                                .font(.caption.monospaced()).foregroundStyle(.secondary)
                        }
                    }
                }
            }
        }
        .navigationTitle(levelName)
        .task {
            loading = true
            chunks = await Task.detached(priority: .utility) {
                XRayLevelDiagnostics.inspectChunks(gameID: gameID, levelName: levelName)
            }.value
            collision = await Task.detached(priority: .utility) {
                XRayLevelDiagnostics.inspectCollision(gameID: gameID, levelName: levelName)
            }.value
            loading = false
        }
    }
}
