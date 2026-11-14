import SwiftUI

struct XRayLevelListView: View {
    let gameID: String
    let gameName: String
    @State private var levels: [XRayLevelDiagnostics.Level] = []
    @State private var loading = true
    @State private var resourceSummary: XRayLevelDiagnostics.ResourceSummary?

    var body: some View {
        List {
            if let summary = resourceSummary {
                Section("Native resource scan") {
                    LabeledContent("Loose game files", value: summary.loose ? "Found" : "Not found")
                    LabeledContent("Packed archives", value: "\(summary.archiveCount)")
                    LabeledContent("Archive size", value: ByteCountFormatter.string(
                        fromByteCount: Int64(clamping: summary.archiveBytes), countStyle: .file))
                }
            }
            if loading {
                ProgressView("Scanning unpacked levels…")
            } else if levels.isEmpty {
                ContentUnavailableView("No unpacked levels found",
                                       systemImage: "square.stack.3d.up.slash",
                                       description: Text("Import extracted gamedata/levels folders. Packed game archives are not decoded yet."))
            } else {
                Section("Unpacked locations (\(levels.count))") {
                    ForEach(levels) { level in
                        NavigationLink {
                            XRayLevelDetailView(gameID: gameID, levelName: level.name)
                        } label: {
                        HStack {
                            VStack(alignment: .leading, spacing: 4) {
                                Text(level.name).font(.headline)
                                Text(level.chunks >= 0
                                     ? "Geometry chunks: \(level.chunks)"
                                     : "Geometry unavailable or invalid")
                                    .font(.caption).foregroundStyle(.secondary)
                            }
                            Spacer()
                            Image(systemName: level.chunks >= 0 ? "checkmark.circle" : "exclamationmark.triangle")
                                .foregroundStyle(level.chunks >= 0 ? .green : .orange)
                        }
                        }
                    }
                }
            }
        }
        .navigationTitle(gameName)
        .task {
            loading = true
            let result = await Task.detached(priority: .utility) {
                (XRayLevelDiagnostics.inspect(gameID: gameID),
                 XRayLevelDiagnostics.resourceSummary(gameID: gameID))
            }.value
            levels = result.0
            resourceSummary = result.1
            loading = false
        }
    }
}
