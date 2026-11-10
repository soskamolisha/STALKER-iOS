import SwiftUI
import UniformTypeIdentifiers

/// Trilogy selection screen. Game launching remains disabled until a compatible iOS engine exists.
struct TrilogyLauncherView: View {
    @AppStorage("stalker.language") private var language = "ru"
    @State private var selectedGame: GameEdition?
    @State private var showImporter = false
    @StateObject private var resources = GameResourceStore()
    @State private var importError: String?
    @State private var isCopying = false
    @State private var engineDiagnostics: [String: String] = [:]

    enum GameEdition: String, CaseIterable, Identifiable {
        case shadow = "shadow"
        case clearSky = "clearSky"
        case pripyat = "pripyat"
        var id: String { rawValue }
        var artworkURL: URL? {
            let url: String
            switch self {
            case .shadow: url = "https://images.unsplash.com/photo-1511497584788-876760111969?w=1200&q=85"
            case .clearSky: url = "https://images.unsplash.com/photo-1470252649378-9c29740c9fa8?w=1200&q=85"
            case .pripyat: url = "https://images.unsplash.com/photo-1519608487953-e999c86e7455?w=1200&q=85"
            }
            return URL(string: url)
        }
        var symbol: String {
            switch self {
            case .shadow: return "radiation"
            case .clearSky: return "cloud.sun.fill"
            case .pripyat: return "bolt.shield.fill"
            }
        }
        var russianName: String {
            switch self {
            case .shadow: return "Тень Чернобыля"
            case .clearSky: return "Чистое небо"
            case .pripyat: return "Зов Припяти"
            }
        }
        var englishName: String {
            switch self {
            case .shadow: return "Shadow of Chernobyl"
            case .clearSky: return "Clear Sky"
            case .pripyat: return "Call of Pripyat"
            }
        }
    }

    private var russian: Bool { language == "ru" }

    private func checkEngineResources(for game: GameEdition) {
        guard let handle = st_xray_create() else {
            engineDiagnostics[game.rawValue] = russian ? "Ошибка создания ядра" : "Engine initialization failed"
            return
        }
        defer { st_xray_destroy(handle) }
        let edition: Int32
        switch game {
        case .shadow: edition = 0
        case .clearSky: edition = 1
        case .pripyat: edition = 2
        }
        guard st_xray_initialize(handle, edition) == 0 else {
            engineDiagnostics[game.rawValue] = russian ? "Ошибка инициализации" : "Initialization failed"
            return
        }
        defer { st_xray_shutdown(handle) }
        guard let support = FileManager.default.urls(for: .applicationSupportDirectory, in: .userDomainMask).first else {
            engineDiagnostics[game.rawValue] = russian ? "Хранилище недоступно" : "Storage unavailable"
            return
        }
        let folder = support.appendingPathComponent("ImportedGames", isDirectory: true)
            .appendingPathComponent(game.rawValue, isDirectory: true)
        let result = folder.path.withCString { st_xray_attach_resources(handle, $0) }
        if result == 0 {
            let loose = st_xray_has_loose_data(handle) == 1
            let packed = st_xray_has_packed_data(handle) == 1
            let levels = st_xray_level_count(handle)
            var levelDetails: [String] = []
            if levels > 0 {
                for index in 0..<min(levels, 3) {
                    var buffer = [CChar](repeating: 0, count: 256)
                    let nameResult = buffer.withUnsafeMutableBufferPointer { pointer in
                        st_xray_level_name(handle, UInt32(index), pointer.baseAddress, UInt32(pointer.count))
                    }
                    if nameResult == 0 {
                        let name = String(cString: buffer)
                        let chunks = st_xray_level_chunk_count(handle, UInt32(index))
                        levelDetails.append("\(name): \(chunks >= 0 ? String(chunks) : "—")")
                    }
                }
            }
            let levelSummary = levelDetails.isEmpty ? "" : " [\(levelDetails.joined(separator: ", "))]"
            engineDiagnostics[game.rawValue] = russian
                ? "Ядро готово: папка \\(loose ? "✓" : "—"), архивы \\(packed ? "✓" : "—"), уровней \\(levels)"
                : "Core ready: folder \\(loose ? "✓" : "—"), archives \\(packed ? "✓" : "—"), levels \\(levels)"
        } else {
            engineDiagnostics[game.rawValue] = russian
                ? "Ядро: ресурсы не найдены или не распознаны"
                : "Core: no recognized resources"
        }
    }

    var body: some View {
        NavigationStack {
            ZStack {
                LinearGradient(colors: [Color(red: 0.035, green: 0.055, blue: 0.055),
                                         Color(red: 0.11, green: 0.13, blue: 0.10)],
                               startPoint: .topLeading, endPoint: .bottomTrailing).ignoresSafeArea()
                ScrollView {
                    VStack(alignment: .leading, spacing: 18) {
                        HStack {
                            HStack(spacing: 9) {
                                Image(systemName: "scope").font(.title.bold())
                                Text("STALKER").font(.largeTitle.weight(.black)).tracking(3)
                                Text("MOBILE").font(.caption.bold()).tracking(2)
                                    .padding(.horizontal, 9).padding(.vertical, 5)
                                    .background(.yellow.opacity(0.2), in: Capsule())
                            }.foregroundStyle(.yellow)
                            Spacer()
                            Picker("Language", selection: $language) {
                                Text("РУС").tag("ru")
                                Text("ENG").tag("en")
                            }
                            .pickerStyle(.segmented)
                            .frame(width: 150)
                        }
                        if isCopying {
                            ProgressView(russian ? "Копирование файлов игры…" : "Copying game files…")
                                .tint(.yellow)
                        }
                        HStack {
                            VStack(alignment: .leading, spacing: 5) {
                                Text(russian ? "ДОБРО ПОЖАЛОВАТЬ В ЗОНУ" : "WELCOME TO THE ZONE")
                                    .font(.caption.bold()).tracking(3).foregroundStyle(.yellow)
                                Text(russian ? "Выберите свою историю" : "Choose your story")
                                    .font(.title2.bold()).foregroundStyle(.white)
                            }
                            Spacer()
                            Image(systemName: "waveform.path.ecg.rectangle")
                                .font(.title).foregroundStyle(.yellow.opacity(0.7))
                        }
                        ForEach(GameEdition.allCases) { game in
                            VStack(alignment: .leading, spacing: 10) {
                                ZStack(alignment: .bottomLeading) {
                                    AsyncImage(url: game.artworkURL) { phase in
                                        if let image = phase.image {
                                            image.resizable().scaledToFill()
                                        } else {
                                            ZStack {
                                                LinearGradient(colors: [.black, Color(red: 0.22, green: 0.30, blue: 0.19), .black],
                                                               startPoint: .topLeading, endPoint: .bottomTrailing)
                                                Image(systemName: game.symbol)
                                                    .font(.system(size: 90, weight: .ultraLight))
                                                    .foregroundStyle(.yellow.opacity(0.3))
                                            }
                                        }
                                    }
                                    .frame(maxWidth: .infinity).frame(height: 160).clipped()
                                    LinearGradient(colors: [.clear, .black.opacity(0.9)],
                                                   startPoint: .top, endPoint: .bottom)
                                    HStack {
                                        Image(systemName: game.symbol)
                                        Text(russian ? game.russianName.uppercased() : game.englishName.uppercased())
                                            .font(.title3.bold()).tracking(1)
                                    }
                                    .foregroundStyle(.white)
                                    .padding(16)
                                }
                                .frame(height: 160)
                                .clipShape(RoundedRectangle(cornerRadius: 10))
                                Text(russian ? game.russianName : game.englishName)
                                    .font(.headline).foregroundStyle(.yellow)
                                Text(russian ? "Игровые ресурсы добавляются отдельно" : "Import your own game resources")
                                    .foregroundStyle(.gray)
                                if let status = resources.status[game.rawValue] {
                                    Text(status).font(.caption).foregroundStyle(.green)
                                }
                                if let archiveCount = resources.archiveCounts[game.rawValue] {
                                    Text(russian ? "Архивов X-Ray: \(archiveCount)" : "X-Ray archives: \(archiveCount)")
                                        .font(.caption).foregroundStyle(.secondary)
                                }
                                if let summary = resources.archiveSummaries[game.rawValue] {
                                    Text(summary).font(.caption2).foregroundStyle(.secondary)
                                }
                                if let header = resources.archiveHeaders[game.rawValue] {
                                    Text(header).font(.caption2.monospaced()).foregroundStyle(.gray)
                                        .lineLimit(1)
                                }
                                if let bytes = resources.storageBytes[game.rawValue] {
                                    Text(ByteCountFormatter.string(fromByteCount: bytes, countStyle: .file))
                                        .font(.caption).foregroundStyle(.secondary)
                                }
                                if let diagnostic = resources.diagnostics[game.rawValue] {
                                    Text(diagnostic).font(.caption2.monospaced()).foregroundStyle(.gray)
                                }
                                if let engineResult = engineDiagnostics[game.rawValue] {
                                    Label(engineResult, systemImage: "cpu")
                                        .font(.caption)
                                        .foregroundStyle(.yellow)
                                }
                                HStack {
                                    Button(russian ? "Проверить ядро" : "Check engine") {
                                        checkEngineResources(for: game)
                                    }
                                    .buttonStyle(.bordered)
                                    .tint(.green)
                                    .disabled(isCopying)
                                    Button(russian ? "Импортировать файлы" : "Import files") {
                                        selectedGame = game
                                        showImporter = true
                                    }
                                    .buttonStyle(.borderedProminent).tint(.yellow)
                                    .disabled(resources.importInProgress || isCopying)
                                    Button(russian ? "Запустить" : "Launch") {}
                                        .buttonStyle(.bordered)
                                        .disabled(true)
                                }
                            }
                            .frame(maxWidth: .infinity, alignment: .leading)
                            .padding()
                            .background(Color.white.opacity(0.075), in: RoundedRectangle(cornerRadius: 16))
                            .overlay(RoundedRectangle(cornerRadius: 16).stroke(.yellow.opacity(0.18), lineWidth: 1))
                        }
                        Divider().overlay(.yellow.opacity(0.4))
                        HStack(spacing: 10) {
                            Image(systemName: "externaldrive.fill").foregroundStyle(.yellow)
                            Text(russian ? "РАЗДЕЛ: ДИАГНОСТИКА И ТЕСТЫ" : "SECTION: DIAGNOSTICS & TESTS")
                                .font(.caption.bold()).tracking(1).foregroundStyle(.white)
                        }
                        NavigationLink(russian ? "Открыть технический прототип" : "Open technical prototype") {
                            GameShellView()
                        }
                        .buttonStyle(.bordered)
                        .tint(.orange)
                        Text(russian ? "Запуск оригинальных игр пока недоступен: требуется порт движка X-Ray." : "Original game launching is not yet available: an iOS X-Ray engine port is required.")
                            .font(.footnote).foregroundStyle(.gray)
                    }
                    .padding()
                }
            }
            .task {
                for game in GameEdition.allCases { await resources.refresh(gameID: game.rawValue) }
            }
            .alert("Import error", isPresented: Binding(get: { importError != nil }, set: { if !$0 { importError = nil } })) {
                Button("OK", role: .cancel) { importError = nil }
            } message: { Text(importError ?? "") }
            .fileImporter(isPresented: $showImporter, allowedContentTypes: [.folder]) { result in
                guard let selectedGame else { return }
                do {
                    let folder = try result.get()
                    isCopying = true
                    let edition = selectedGame.rawValue
                    Task {
                        defer { isCopying = false; self.selectedGame = nil }
                        do {
                            try await Task.detached(priority: .userInitiated) {
                                try GameResourceImporter.copy(folder, gameID: edition)
                            }.value
                            await resources.refresh(gameID: edition)
                        } catch {
                            importError = error.localizedDescription
                        }
                    }
                } catch {
                    importError = error.localizedDescription
                    self.selectedGame = nil
                }
            }
        }
    }
}
