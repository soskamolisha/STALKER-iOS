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
                Text("Выбери папку с оригинальными gamedata.db0, gamedata.db1 и другими архивами. Проверяются имена и размеры, но не содержимое. Движок X-Ray пока не запускается.")
                    .font(.subheadline)
                Text(storedArchiveStatus).font(.footnote).foregroundStyle(.secondary)
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

    private func refreshStoredArchives() {
        do {
            let destination = try STArchiveStore.destinationRoot()
            guard FileManager.default.fileExists(atPath: destination.path) else {
                storedArchiveStatus = "Файлы игры ещё не импортированы"
                return
            }
            let saved = try STArchiveInventory.scan(directory: destination)
            let bytes = saved.archives.reduce(UInt64(0)) { total, archive in
                let (sum, overflow) = total.addingReportingOverflow(archive.bytes)
                return overflow ? UInt64.max : sum
            }
            let size = ByteCountFormatter.string(fromByteCount: Int64(clamping: bytes), countStyle: .file)
            storedArchiveStatus = saved.hasBaseArchive
                ? "Сохранено архивов: \(saved.archives.count), общий размер: \(size). Содержимое не проверено."
                : "В сохранённой папке отсутствует gamedata.db0"
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
