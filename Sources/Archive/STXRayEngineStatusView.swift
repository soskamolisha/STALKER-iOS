import SwiftUI

/// Drop-in diagnostics panel for the launcher game-detail screen.
/// Diagnostics and guarded launch when a real embedded engine is linked.
struct STXRayEngineStatusView: View {
    let gameRoot: URL
    let edition: Int32

    init(edition: Int32, gameRoot: URL? = nil) {
        self.edition = edition
        self.gameRoot = gameRoot ??
            FileManager.default.urls(for: .documentDirectory, in: .userDomainMask)[0]
                .appendingPathComponent("ImportedGame", isDirectory: true)
    }

    @Environment(\.scenePhase) private var scenePhase
    @State private var isChecking = false
    @State private var isLaunching = false
    @State private var launchResult = ""
    @State private var canLaunch = false
    @State private var needsRecheck = false
    @State private var scanGeneration = 0
    @State private var summary = "Проверка X-Ray ещё не выполнена"
    @State private var archiveDetails = ""
    @State private var engineDetails = ""
    @State private var failedArchives = ""

    var body: some View {
        VStack(alignment: .leading, spacing: 10) {
            Text("Движок X-Ray").font(.headline)
            Text("Папка ресурсов: \(gameRoot.lastPathComponent)")
                .font(.caption).foregroundStyle(.secondary)
            if !FileManager.default.fileExists(atPath: gameRoot.path) {
                Text("Папка игры ещё не создана. Импортируй файлы и повтори проверку.")
                    .font(.caption).foregroundStyle(.orange)
            }
            Text(summary).font(.subheadline)
            if !engineDetails.isEmpty { Text(engineDetails).font(.caption).foregroundStyle(.secondary) }
            if !failedArchives.isEmpty {
                Text(failedArchives).font(.caption).foregroundStyle(.orange)
            }
            if !archiveDetails.isEmpty {
                Text(archiveDetails).font(.caption).foregroundStyle(.secondary)
            }
            if canLaunch || isLaunching {
                Button(isLaunching ? "X-Ray запускается…" : "Запустить Shadow of Chernobyl") {
                    launchGame()
                }
                .disabled(isLaunching || isChecking)
                .buttonStyle(.borderedProminent)
            }
            if !launchResult.isEmpty {
                Text(launchResult).font(.caption).foregroundStyle(.secondary)
            }
            Button(isChecking ? "Проверка…" : "Проверить движок и файлы игры") {
                refresh()
            }
            .disabled(isChecking || isLaunching)
            .buttonStyle(.borderedProminent)
        }
        .padding()
        .task { refresh() }
        .onChange(of: scenePhase) { phase in
            if phase == .active { refresh() }
        }
        .onChange(of: gameRoot) { _ in refresh() }
        .onChange(of: edition) { _ in refresh() }
    }

    private func launchGame() {
        guard edition == 0, canLaunch, !isLaunching, !isChecking else { return }
        isLaunching = true
        needsRecheck = false
        canLaunch = false
        launchResult = "Передача управления встроенному X-Ray…"
        let root = gameRoot
        DispatchQueue.global(qos: .userInitiated).async {
            let result = STXRayLauncherBridge.launchImportedShadowOfChernobyl(root: root)
            DispatchQueue.main.async {
                isLaunching = false
                launchResult = result == 0
                    ? "X-Ray завершил работу (код 0). Успешный запуск игры не подтверждён."
                    : "X-Ray вернул код \(result): " +
                      String(cString: st_openxray_runtime_status_message(result))
                needsRecheck = false
                refresh()
            }
        }
    }

    private func refresh() {
        guard !isLaunching else {
            // A scene-phase refresh may arrive while native X-Ray owns the
            // resources. Run one fresh scan only after the engine returns.
            needsRecheck = true
            return
        }
        guard !isChecking else {
            needsRecheck = true
            return
        }
        isChecking = true
        let root = gameRoot
        let gameEdition = edition
        scanGeneration += 1
        let generation = scanGeneration
        DispatchQueue.global(qos: .userInitiated).async {
            let result: Result<STXRayLauncherBridge.Snapshot, Error> =
                Result { try STXRayLauncherBridge.inspect(root: root, edition: gameEdition) }
            DispatchQueue.main.async {
                guard generation == scanGeneration else { return }
                isChecking = false
                if needsRecheck {
                    needsRecheck = false
                    refresh()
                    return
                }
                switch result {
                case .success(let snapshot):
                    summary = snapshot.statusText
                    canLaunch = edition == 0 && snapshot.prerequisitesPresent && snapshot.resourceEngineLinked && !isLaunching
                    failedArchives = snapshot.unreadableArchives.isEmpty ? "" : "Не удалось прочитать: " + snapshot.unreadableArchives.prefix(5).joined(separator: ", ")
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
                    canLaunch = false
                    engineDetails = ""
                    failedArchives = ""
                    archiveDetails = "Проверь папку ImportedGame и импортированные ресурсы."
                }
            }
        }
    }
}
