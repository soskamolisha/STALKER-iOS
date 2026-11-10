import Foundation

/// Performs an on-device, read-only structural scan of a file selected in Files.
/// Does not extract, decompress, or validate copyrighted game content.
enum ArchiveProbe {
    static func inspect(_ url: URL) -> String {
        let securityScoped = url.startAccessingSecurityScopedResource()
        defer {
            if securityScoped { url.stopAccessingSecurityScopedResource() }
        }
        var summary = STArchiveSummary()
        let result = url.path.withCString { path in
            st_archive_inspect(path, &summary)
        }
        switch result {
        case 0:
            return "Заголовки: \(summary.chunk_count), сжатых: \(summary.compressed_count). Это проверка структуры, не запуск игры."
        case 1:
            return "Не удалось проверить выбранный файл."
        case 2:
            return "Нет доступа к файлу. Скачайте архив в «Файлы» и повторите."
        default:
            return "Структура некорректна или файл другого формата."
        }
    }
}
