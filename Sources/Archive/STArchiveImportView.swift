import SwiftUI
import UniformTypeIdentifiers

/// Standalone import diagnostics screen. Embed in the launcher navigation
/// after verifying the existing screen structure.
struct STArchiveImportView: View {
    @Environment(\.dismiss) private var dismiss
    @State private var showingImporter = false
    @State private var isImporting = false
    @State private var pendingFolder: URL?
    @State private var showingReplaceConfirmation = false
    @State private var importStatus: String?
    @State private var report: STArchiveInventory.Report?
    @State private var selectedFolder: String?
    @State private var errorMessage: String?
    @State private var storedArchiveStatus: String = "Проверка ранее загруженных файлов…"
    @State private var nativeArchiveStatus: String?
    @State private var engineReadinessStatus: String?

    var body: some View {
        ScrollView {
            VStack(alignment: .leading, spacing: 14) {
                HStack {
                    Text("Импорт файлов игры").font(.title2.bold())
                    Spacer()
                    Button("Закрыть") { dismiss() }
                        .disabled(isImporting)
                        .buttonStyle(.bordered)
                }
                Text("Выбери папку с оригинальными gamedata.db0, gamedata.db1 и другими архивами. Для будущего запуска X-Ray также нужен непустой fsgame.ltx в корне ImportedGame. Проверяются имена и размеры архивов, но не содержимое. Движок X-Ray пока не запускается.")
                    .font(.subheadline)
                Text(storedArchiveStatus).font(.footnote).foregroundStyle(.secondary)
                Text("Если кнопка «Открыть» при выборе папки неактивна: открой «Файлы» → «На iPhone» → STALKERiOSStarter → ImportedGame и скопируй туда оригинальные gamedata.db0 и fsgame.ltx (если они есть). Затем вернись сюда и нажми «Проверить файлы», после чего «Проверить готовность X-Ray».")
                    .font(.footnote)
                Button("Проверить файлы") { refreshStoredArchives() }
                    .disabled(isImporting)
                    .buttonStyle(.bordered)
                Button("Проверить архивы через X-Ray") { inspectNativeArchives() }
                    .disabled(isImporting)
                    .buttonStyle(.bordered)
                if let nativeArchiveStatus {
                    Text(nativeArchiveStatus).font(.footnote).foregroundStyle(.secondary)
                }
                Button("Проверить готовность X-Ray") { inspectEngineReadiness() }
                    .disabled(isImporting)
                    .buttonStyle(.bordered)
                if let engineReadinessStatus {
                    Text(engineReadinessStatus).font(.footnote).foregroundStyle(.secondary)
                }
                Button("Выбрать папку игры") { showingImporter = true }
                    .disabled(isImporting)
                    .buttonStyle(.borderedProminent)
                if isImporting { ProgressView("Копирование архивов… Не закрывай экран") }
                if let importStatus { Text(importStatus).font(.subheadline) }
                if let selectedFolder {
                    Text("Выбрана папка: \(selectedFolder)").font(.caption)
                }
                if let report {
                    Text("Архивов найдено: \(report.archives.count)")
                    Text(report.hasBaseArchive ? "gamedata.db0 найден (содержимое не проверено)" : "gamedata.db0 отсутствует")
                    ForEach(report.archives, id: \.number) { archive in
                        Text("\(archive.filename) — \(archive.bytes) bytes")
                            .font(.system(.caption, design: .monospaced))
                    }
                    if !report.missingNumbers.isEmpty {
                        Text("Отсутствуют номера: \(report.missingNumbers.map(String.init).joined(separator: ", "))")
                    }
                    ForEach(Array(report.warnings.enumerated()), id: \.offset) { entry in
                        Text(entry.element).font(.caption).foregroundStyle(.orange)
                    }
                }
                if let errorMessage {
                    Text(errorMessage).foregroundStyle(.red)
                }
            }
            .frame(maxWidth: .infinity, alignment: .leading)
            .padding()
        }
        .onAppear { refreshStoredArchives() }
        .interactiveDismissDisabled(isImporting)
        .fileImporter(isPresented: $showingImporter, allowedContentTypes: [.folder], allowsMultipleSelection: false) { result in
            switch result {
            case .failure(let error):
                errorMessage = "Не удалось открыть выбор папки: \(error.localizedDescription)"
            case .success(let urls):
                guard let folder = urls.first else {
                    errorMessage = "Папка не выбрана"
                    return
                }
                pendingFolder = folder
                showingReplaceConfirmation = true
            }
        }
        .confirmationDialog("Импортировать архивы игры?", isPresented: $showingReplaceConfirmation, titleVisibility: .visible) {
            Button("Скопировать архивы") {
                guard let folder = pendingFolder else { return }
                pendingFolder = nil
                beginImport(folder)
            }
            Button("Отмена", role: .cancel) { pendingFolder = nil }
        } message: {
            Text("Ранее загруженные архивы будут заменены только после успешного копирования новых. Для больших файлов понадобится свободное место.")
        }
    }

    private func inspectEngineReadiness() {
        do {
            let root = try STArchiveStore.destinationRoot()
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
            let root = try STArchiveStore.destinationRoot()
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
        do {
            let destination = try STArchiveStore.destinationRoot()
            // Create the Files-visible destination before the first import.
            // Folder picking can be disabled by some iOS document providers;
            // users can then copy original game files into this folder manually.
            if !FileManager.default.fileExists(atPath: destination.path) {
                try FileManager.default.createDirectory(at: destination, withIntermediateDirectories: true)
                storedArchiveStatus = "Папка ImportedGame создана. Скопируй сюда игровые файлы через «Файлы» или выбери папку для импорта."
                return
            }
            let saved = try STArchiveInventory.scan(directory: destination)
            let config = destination.appendingPathComponent("fsgame.ltx", isDirectory: false)
            // Match the native lstat-based preflight: a symlinked config
            // must never appear valid in the import screen.
            let configValues = try? config.resourceValues(forKeys: [
                .isSymbolicLinkKey, .isRegularFileKey, .fileSizeKey
            ])
            let configValid = configValues?.isSymbolicLink == false
                && configValues?.isRegularFile == true
                && (configValues?.fileSize ?? 0) > 0

            let bytes = saved.archives.reduce(UInt64(0)) { total, archive in
                let (sum, overflow) = total.addingReportingOverflow(archive.bytes)
                return overflow ? UInt64.max : sum
            }
            let size = ByteCountFormatter.string(fromByteCount: Int64(clamping: bytes), countStyle: .file)
            let valid = saved.hasBaseArchive
                && saved.missingNumbers.isEmpty
                && !saved.archives.contains(where: { $0.bytes == 0 })
                && saved.warnings.isEmpty
            let configMessage = configValid ? "fsgame.ltx найден." : "fsgame.ltx отсутствует или пустой — проверка запуска X-Ray не пройдёт."
            storedArchiveStatus = (valid
                ? "Найдено архивов: \(saved.archives.count), общий размер: \(size). Имена и размеры проверены; содержимое не проверено."
                : "Файлы требуют проверки: архивов \(saved.archives.count), размер \(size). Убедись, что есть gamedata.db0, нет пропусков и ошибок.") + " " + configMessage
        } catch {
            storedArchiveStatus = "Не удалось проверить сохранённые файлы: \(error.localizedDescription)"
        }
    }

    private func beginImport(_ folder: URL) {
                selectedFolder = folder.lastPathComponent
                report = nil
                errorMessage = nil
                isImporting = true
                importStatus = "Копирование файлов в память приложения…"
                DispatchQueue.global(qos: .userInitiated).async {
                    // STArchiveStore owns security-scoped access for the entire copy.
                    let result = Result { try STArchiveStore.importArchives(from: folder) }
                    DispatchQueue.main.async {
                        isImporting = false
                        switch result {
                        case .success(let imported):
                            report = imported
                            importStatus = "Сохранено архивов: \(imported.archives.count). Содержимое ещё не проверено."
                            errorMessage = nil
                            refreshStoredArchives()
                        case .failure(let error):
                            refreshStoredArchives()
                            report = nil
                            importStatus = nil
                            errorMessage = "Ошибка импорта: \(error.localizedDescription)"
                        }
                    }
                }
    }
}
