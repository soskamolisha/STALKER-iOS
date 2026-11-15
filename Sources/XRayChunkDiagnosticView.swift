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
                guard url.startAccessingSecurityScopedResource() else {
                    message = "Unable to access selected file."
                    return
                }
                defer { url.stopAccessingSecurityScopedResource() }
                let handle = try FileHandle(forReadingFrom: url)
                defer { try? handle.close() }
                let data = try handle.read(upToCount: 4 * 1024 * 1024 + 1) ?? Data()
                guard data.count <= 4 * 1024 * 1024 else {
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
