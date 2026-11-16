import SwiftUI

/// Drop-in diagnostics panel for the launcher game-detail screen.
/// Call refresh after importing resources; does not attempt to start gameplay.
struct STXRayEngineStatusView: View {
    let gameRoot: URL
    let edition: Int32

    init(edition: Int32, gameRoot: URL? = nil) {
        self.edition = edition
        self.gameRoot = gameRoot ??
            FileManager.default.urls(for: .documentDirectory, in: .userDomainMask)[0]
                .appendingPathComponent("ImportedGame", isDirectory: true)
    }

    @State private var isChecking = false
    @State private var summary = "Проверка X-Ray ещё не выполнена"
    @State private var archiveDetails = ""
    @State private var engineDetails = ""

    var body: some View {
        VStack(alignment: .leading, spacing: 10) {
            Text("Движок X-Ray").font(.headline)
            Text(summary).font(.subheadline)
            if !engineDetails.isEmpty { Text(engineDetails).font(.caption).foregroundStyle(.secondary) }
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
                    engineDetails = snapshot.resourceEngineLinked ? "Ресурсный интерфейс X-Ray обнаружен; запуск не проверен" : "Исполняемый движок X-Ray не подключён"
                    let checked = zip(snapshot.archiveNames, snapshot.directoryRecordCounts)
                        .prefix(5)
                        .map { name, count in
                            count >= 0 ? "\(name): \(count) записей" :
                                "\(name): ошибка каталога \(count)"
                        }
                    archiveDetails = "Архивов: \(snapshot.archiveNames.count), байт: \(snapshot.archiveBytes). " +
                        (checked.isEmpty ? "Архивы не найдены." : checked.joined(separator: "; "))
                case .failure(let error):
                    summary = error.localizedDescription
                    engineDetails = ""
                    archiveDetails = "Проверь папку ImportedGame и импортированные ресурсы."
                }
            }
        }
    }
}
