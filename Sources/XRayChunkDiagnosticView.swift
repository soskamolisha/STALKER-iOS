import SwiftUI
import UniformTypeIdentifiers

/// Standalone developer diagnostic. It does not decode gamedata.db0.
struct XRayChunkDiagnosticView: View {
    @State private var importing = false
    @State private var message = "Choose a small candidate chunk-stream file (up to 4 MB)."
    @State private var chunks: [XRayChunkDiagnostics.Chunk] = []

    var body: some View {
        VStack(alignment: .leading, spacing: 12) {
            Text("X-Ray chunk diagnostics").font(.headline)
            Text("Read-only developer tool. Not a game archive importer.")
                .font(.caption)
            Button("Inspect file") { importing = true }
            Text(message).font(.caption)
            List(chunks.indices, id: \.self) { index in
                let chunk = chunks[index]
                VStack(alignment: .leading) {
                    Text("Chunk \(chunk.id) · \(chunk.payloadSize) bytes")
                    Text("Offset \(chunk.payloadOffset) · compressed flag: \(chunk.compressed ? "yes" : "no")")
                        .font(.caption)
                }
            }
        }
        .padding()
        .fileImporter(isPresented: $importing, allowedContentTypes: [.data, .item]) { result in
            chunks = []
            do {
                let url = try result.get()
                let securityScoped = url.startAccessingSecurityScopedResource()
                defer { if securityScoped { url.stopAccessingSecurityScopedResource() } }
                let handle = try FileHandle(forReadingFrom: url)
                defer { try? handle.close() }
                // FileHandle may return fewer bytes than requested before EOF.
                // Read until EOF or the hard limit, whichever comes first.
                let limit = 4 * 1024 * 1024
                var data = Data()
                while data.count <= limit {
                    let requested = min(64 * 1024, limit + 1 - data.count)
                    guard let block = try handle.read(upToCount: requested), !block.isEmpty else {
                        break
                    }
                    data.append(block)
                }
                guard data.count <= limit else {
                    message = "File exceeds 4 MB diagnostic limit."
                    return
                }
                chunks = try XRayChunkDiagnostics.scan(data)
                message = "Found \(chunks.count) candidate chunks. No archive format validation performed."
            } catch {
                message = "Could not inspect file: \(error.localizedDescription)"
            }
        }
    }
}
