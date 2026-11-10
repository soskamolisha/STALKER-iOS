import SwiftUI

struct XRayLevelListView: View {
    let gameID: String
    let gameName: String
    @State private var levels: [XRayLevelDiagnostics.Level] = []
    @State private var loading = true

    var body: some View {
        List {
            if loading {
                ProgressView("Scanning unpacked levels…")
            } else if levels.isEmpty {
                ContentUnavailableView("No unpacked levels found",
                                       systemImage: "square.stack.3d.up.slash",
                                       description: Text("Import extracted gamedata/levels folders. Packed game archives are not decoded yet."))
            } else {
                Section("Unpacked locations (\(levels.count))") {
                    ForEach(levels) { level in
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
        .navigationTitle(gameName)
        .task {
            loading = true
            levels = await Task.detached(priority: .utility) {
                XRayLevelDiagnostics.inspect(gameID: gameID)
            }.value
            loading = false
        }
    }
}
