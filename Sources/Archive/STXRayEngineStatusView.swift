import SwiftUI

/// Drop-in diagnostics panel for the launcher game-detail screen.
/// Call refresh after importing resources; does not attempt to start gameplay.
struct STXRayEngineStatusView: View {
    let gameRoot: URL
    let edition: Int32

    @State private var isChecking = false
    @State private var summary = "Проверка X-Ray ещё не выполнена"
    @State private var archiveDetails = ""

    var body: some View {
        VStack(alignment: .leading, spacing: 10) {
            Text("Движок X-Ray").font(.headline)
            Text(summary).font(.subheadline)
            if !archiveDetails.isEmpty {
                Text(archiveDetails).font(.caption).foregroundStyle(.secondary)
            }
            Button(isChecking ? "Проверка…" : "Проверить движок и файлы игры") {
                refresh()
            }
            .disabled(isChecking)
            .buttonStyle(.borderedProminent)
        }
        .padding()
        .task { refresh() }
    }

    private func refresh() {
        guard !isChecking else { return }
        isChecking = true
        let root = gameRoot
        let gameEdition = edition
        DispatchQueue.global(qos: .userInitiated).async {
            let result: Result<STXRayLauncherBridge.Snapshot, Error> =
                Result { try STXRayLauncherBridge.inspect(root: root, edition: gameEdition) }
            DispatchQueue.main.async {
                isChecking = false
                switch result {
                case .success(let snapshot):
                    summary = snapshot.statusText
                    archiveDetails = "Архивов: \(snapshot.archiveNames.count), байт: \(snapshot.archiveBytes). " +
                        (snapshot.archiveNames.isEmpty ? "Архивы не найдены." :
                            snapshot.archiveNames.prefix(5).joined(separator: ", "))
                case .failure(let error):
                    summary = error.localizedDescription
                    archiveDetails = "Проверь папку ImportedGame и импортированные ресурсы."
                }
            }
        }
    }
}
