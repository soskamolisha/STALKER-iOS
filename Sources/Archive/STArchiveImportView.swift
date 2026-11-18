import SwiftUI

/// Folder import and X-Ray diagnostics for Shadow of Chernobyl.
struct STArchiveImportView: View {
    @Environment(\.dismiss) private var dismiss
    @Environment(\.scenePhase) private var scenePhase
    @EnvironmentObject private var resources: GameResourceStore
    @State private var showingImporter = false
    @State private var isImporting = false
    @State private var report: STArchiveInventory.Report?
    @State private var errorMessage: String?
    @State private var storedArchiveStatus = "Проверка ранее загруженных файлов…"
    @State private var nativeArchiveStatus: String?
    @State private var engineReadinessStatus: String?
    @State private var importedRevision = 0

    private var appFolderName: String {
        let bundle = Bundle.main
        return (bundle.object(forInfoDictionaryKey: "CFBundleDisplayName") as? String)
            ?? (bundle.object(forInfoDictionaryKey: "CFBundleName") as? String)
            ?? "STALKERiOSStarter"
    }

    var body: some View {
        ScrollView {
            VStack(alignment: .leading, spacing: 14) {
                HStack {
                    Text("Импорт «Тени Чернобыля»").font(.title2.bold())
                    Spacer()
                    Button("Закрыть") { dismiss() }
                        .disabled(isImporting)
                        .buttonStyle(.bordered)
                }
                Text("Выбери папку игры целиком. Подойдёт и папка Data из iOS-порта.")
                    .font(.subheadline)
                Button("Выбрать папку игры") { showingImporter = true }
                    .disabled(isImporting)
                    .buttonStyle(.borderedProminent)
                if isImporting {
                    ProgressView("Копирование файлов игры…")
                } else if resources.detectedResources["shadow"] == true {
                    Label("Ресурсы добавлены", systemImage: "checkmark.circle.fill")
                        .foregroundStyle(.green)
                }
                if let errorMessage {
                    Text(errorMessage).foregroundStyle(.red)
                }
                DisclosureGroup("Если «Открыть» неактивно") {
                    Text("Скопируй папку игры в «Файлы» → «На iPhone» → \(appFolderName) → Import.")
                        .font(.footnote)
                    Button("Импортировать из Import") {
                        do {
                            beginImport(try GameResourceImporter.filesInbox())
                        } catch {
                            errorMessage = error.localizedDescription
                        }
                    }
                    .disabled(isImporting)
                    .buttonStyle(.bordered)
                }
                Text(storedArchiveStatus).font(.footnote).foregroundStyle(.secondary)
                Button("Проверить файлы") { refreshStoredArchives() }
                    .disabled(isImporting)
                    .buttonStyle(.bordered)
                Button("Проверить архивы через X-Ray") { inspectNativeArchives() }
                    .disabled(isImporting)
                    .buttonStyle(.bordered)
                if let nativeArchiveStatus {
                    Text(nativeArchiveStatus).font(.footnote).foregroundStyle(.secondary)
                }
                if let importedRoot = try? XRayGameResourcePaths.root(for: "shadow") {
                    STXRayEngineStatusView(edition: 0, gameRoot: importedRoot)
                        .id(importedRevision)
                        .disabled(isImporting)
                }
                Button("Проверить готовность X-Ray") { inspectEngineReadiness() }
                    .disabled(isImporting)
                    .buttonStyle(.bordered)
                if let engineReadinessStatus {
                    Text(engineReadinessStatus).font(.footnote).foregroundStyle(.secondary)
                }
                if let report, !report.archives.isEmpty {
                    Text("Архивов найдено: \(report.archives.count)")
                    Text(report.hasBaseArchive ? "gamedata.db0 найден (содержимое не проверено)" : "gamedata.db0 отсутствует")
                    ForEach(report.archives, id: \.number) { archive in
                        Text("\(archive.filename) — \(archive.bytes) bytes")
                            .font(.system(.caption, design: .monospaced))
                    }
                    if !report.missingNumbers.isEmpty {
                        Text("Отсутствуют номера: \(report.missingNumbers.map(String.init).joined(separator: ", "))")
                    }
                }
            }
            .frame(maxWidth: .infinity, alignment: .leading)
            .padding()
        }
        .onAppear { refreshStoredArchives() }
        .onChange(of: scenePhase) { _, phase in
            if phase == .active && !isImporting { refreshStoredArchives() }
        }
        .interactiveDismissDisabled(isImporting)
        .sheet(isPresented: $showingImporter) {
            GameFolderPicker { folder in
                showingImporter = false
                if let folder { beginImport(folder) }
            }
        }
    }

    private func inspectEngineReadiness() {
        do {
            let root = try XRayGameResourcePaths.root(for: "shadow")
            let result = root.path.withCString { st_openxray_runtime_preflight($0) }
            let nativeMessage = String(cString: st_openxray_runtime_status_message(result))
            let linked = st_openxray_runtime_resources_linked() == 1
            engineReadinessStatus = "Код проверки: \(result). \(nativeMessage). Ресурсный игровой движок: \(linked ? "подключён" : "не подключён"). Это не запуск игры."
        } catch {
            engineReadinessStatus = "Ошибка проверки движка: \(error.localizedDescription)"
        }
    }

    private func inspectNativeArchives() {
        do {
            let root = try XRayGameResourcePaths.root(for: "shadow")
            guard let handle = st_xray_create() else {
                nativeArchiveStatus = "Не удалось создать диагностический экземпляр X-Ray"
                return
            }
            defer { st_xray_destroy(handle) }
            guard st_xray_initialize(handle, 0) == 0 else {
                nativeArchiveStatus = "Ошибка инициализации X-Ray"
                return
            }
            let attached = root.path.withCString { st_xray_attach_resources(handle, $0) }
            guard attached == 0 else {
                nativeArchiveStatus = "X-Ray не смог открыть ImportedGame (код \(attached))"
                return
            }
            let count = st_xray_archive_count(handle)
            let bytes = st_xray_archive_bytes(handle)
            nativeArchiveStatus = "X-Ray обнаружил \(count) архивов, \(ByteCountFormatter.string(fromByteCount: Int64(clamping: bytes), countStyle: .file)). Это проверка чтения файлов, не запуск игры."
        } catch {
            nativeArchiveStatus = "Ошибка проверки X-Ray: \(error.localizedDescription)"
        }
    }

    private func refreshStoredArchives() {
        Task { await resources.refresh(gameID: "shadow") }
        do {
            let destination = try XRayGameResourcePaths.root(for: "shadow")
            report = try STArchiveInventory.scan(directory: destination)
            let config = destination.appendingPathComponent("fsgame.ltx", isDirectory: false)
            let values = try? config.resourceValues(forKeys: [.isSymbolicLinkKey, .isRegularFileKey, .fileSizeKey])
            let valid = values?.isSymbolicLink == false
                && values?.isRegularFile == true
                && (values?.fileSize ?? 0) > 0
            storedArchiveStatus = valid ? "fsgame.ltx найден." : "Для запуска X-Ray нужен файл fsgame.ltx."
        } catch {
            report = nil
            storedArchiveStatus = "Не удалось проверить сохранённые файлы: \(error.localizedDescription)"
        }
    }

    private func beginImport(_ folder: URL) {
        guard !isImporting else { return }
        errorMessage = nil
        isImporting = true
        Task {
            defer { isImporting = false }
            do {
                try await resources.importFolder(folder, gameID: "shadow")
                refreshStoredArchives()
                importedRevision += 1
                nativeArchiveStatus = nil
                engineReadinessStatus = nil
            } catch {
                errorMessage = GameResourceImporter.ImportFailure.message(for: error, russian: true)
            }
        }
    }
}
