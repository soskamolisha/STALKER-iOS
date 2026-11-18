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
            Text(message).font(.caption).textSelection(.enabled)
            ShareLink(item: message) {
                Label("Share diagnostic report", systemImage: "square.and.arrow.up")
            }
            .disabled(message == "Choose a small candidate chunk-stream file (up to 4 MB).")
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
                let lowerName = url.lastPathComponent.lowercased()
                let handle = try FileHandle(forReadingFrom: url)
                defer { try? handle.close() }
                // All numbered .db segments use the same bounded inspection path.
                let dbExtension = url.pathExtension.lowercased()
                let isDbSegment = dbExtension.count >= 3 && dbExtension.hasPrefix("db") &&
                    dbExtension.dropFirst(2).allSatisfy { $0.isNumber }
                if isDbSegment {
                    // Read a bounded sample, never load the complete game archive.
                    let fileSize = try handle.seekToEnd()
                    try handle.seek(toOffset: 0)
                    let prefix = try handle.read(upToCount: 32) ?? Data()
                    let tailOffset = fileSize > 32 ? fileSize - 32 : 0
                    try handle.seek(toOffset: tailOffset)
                    let suffix = try handle.read(upToCount: 32) ?? Data()
                    let firstWord: String
                    if prefix.count >= 4 {
                        let word = UInt32(prefix[0]) |
                            (UInt32(prefix[1]) << 8) |
                            (UInt32(prefix[2]) << 16) |
                            (UInt32(prefix[3]) << 24)
                        firstWord = String(format: "0x%08X", word)
                    } else {
                        firstWord = "unavailable (fewer than 4 bytes)"
                    }
                    let firstChunkHeader: String
                    if prefix.count >= 8 {
                        let rawID = UInt32(prefix[0]) |
                            (UInt32(prefix[1]) << 8) |
                            (UInt32(prefix[2]) << 16) |
                            (UInt32(prefix[3]) << 24)
                        let payloadSize = UInt32(prefix[4]) |
                            (UInt32(prefix[5]) << 8) |
                            (UInt32(prefix[6]) << 16) |
                            (UInt32(prefix[7]) << 24)
                        let fits = UInt64(payloadSize) <= fileSize - 8
                        firstChunkHeader = "Candidate chunk ID: \(rawID & 0x7FFFFFFF), compressed flag: \((rawID & 0x80000000) != 0), payload bytes: \(payloadSize), within file bounds: \(fits)"
                    } else {
                        firstChunkHeader = "Candidate chunk header unavailable (fewer than 8 bytes)"
                    }
                    let headHex = prefix.map { String(format: "%02X", $0) }.joined(separator: " ")
                    let tailHex = suffix.map { String(format: "%02X", $0) }.joined(separator: " ")
                    let signature = prefix.starts(with: [0x50, 0x4B, 0x03, 0x04])
                        ? "ZIP signature detected: this is not a raw .db segment."
                        : (fileSize == 0 ? "Empty archive candidate." : "Raw .db segment candidate; format not verified.")
                    message = "File: \(url.lastPathComponent)\nSize: \(fileSize) bytes\nFirst UInt32 (little-endian): \(firstWord)\nHead (offset 0): \(headHex)\n\(firstChunkHeader)\nTail (offset \(tailOffset)): \(tailHex)\n\(signature)\nX-Ray .db directory decoding is not implemented."
                    return
                }
                // ZIP archives are containers, not generic X-Ray chunks.
                if lowerName.hasSuffix(".zip") {
                    message = "ZIP archive selected. Extract the target file before diagnostics."
                    return
                }
                // Check actual file length before allocating a diagnostic buffer.
                // This avoids reading several megabytes from unsupported large inputs.
                let candidateSize = try handle.seekToEnd()
                try handle.seek(toOffset: 0)
                if candidateSize > 4 * 1024 * 1024 {
                    message = "File is \(candidateSize) bytes; generic chunk diagnostics accept at most 4 MB. No content was loaded."
                    return
                }
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
                // ZIP files and .db archives are not generic X-Ray chunk streams.
                if lowerName.hasSuffix(".zip") || data.starts(with: [0x50, 0x4B, 0x03, 0x04]) {
                    message = "ZIP archive detected. This diagnostic does not unpack ZIP files."
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
