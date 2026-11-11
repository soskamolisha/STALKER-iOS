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
        case pripyat = "pripyat"
        case clearSky = "clearSky"
        var id: String { rawValue }
        var bundledArtworkName: String { "card_\(rawValue)" }
        var artworkURL: URL? {
            let url: String
            switch self {
            case .shadow: url = "https://unsplash.com/photos/dF7LP4m_0Vg/download?w=900"
            case .clearSky: url = "https://unsplash.com/photos/y6Tr4hsOD5w/download?w=900"
            case .pripyat: url = "https://unsplash.com/photos/TuhOWnyKiKk/download?w=900"
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
                ? "Ядро: папка \(loose ? "✓" : "—"), архивы \(packed ? "✓" : "—"), уровней \(levels)\(levelSummary)"
                : "Core: folder \(loose ? "✓" : "—"), archives \(packed ? "✓" : "—"), levels \(levels)\(levelSummary)"
        } else {
            engineDiagnostics[game.rawValue] = russian
                ? "Ядро: ресурсы не найдены или не распознаны"
                : "Core: no recognized resources"
        }
    }

    @State private var selectedSection = "home"
    @State private var showAbout = false
    @State private var showDiagnostics = false
    @State private var showEngineNotice = false
    @Environment(\.horizontalSizeClass) private var horizontalSizeClass

    private var sidebar: some View {
        VStack(alignment: .leading, spacing: 12) {
            ForEach(["home", "games", "import", "settings", "saves", "about"], id: \.self) { section in
                let icon: String = {
                    switch section {
                    case "home": return "house.fill"
                    case "games": return "folder"
                    case "import": return "square.and.arrow.down"
                    case "settings": return "slider.horizontal.3"
                    case "saves": return "square.and.arrow.down.on.square"
                    default: return "info.circle"
                    }
                }()
                let label: String = {
                    switch section {
                    case "home": return russian ? "Главная" : "Home"
                    case "games": return russian ? "Игры" : "Games"
                    case "import": return russian ? "Импорт ресурсов" : "Import resources"
                    case "settings": return russian ? "Настройки" : "Settings"
                    case "saves": return russian ? "Сохранения" : "Saves"
                    default: return russian ? "О проекте" : "About"
                    }
                }()
                Button {
                    selectedSection = section
                    if section == "about" { showAbout = true }
                    if section == "import" {
                        selectedGame = .shadow
                        showImporter = true
                    }
                } label: {
                    Label(label, systemImage: icon)
                        .font(.subheadline.weight(selectedSection == section ? .bold : .regular))
                        .frame(maxWidth: .infinity, alignment: .leading)
                        .padding(12)
                        .foregroundStyle(selectedSection == section ? .yellow : .white.opacity(0.85))
                        .background(selectedSection == section ? Color.yellow.opacity(0.16) : .clear,
                                    in: RoundedRectangle(cornerRadius: 8))
                }
                .buttonStyle(.plain)
            }
            Spacer(minLength: 0)
            Image(systemName: "radiation")
                .font(.system(size: 80))
                .foregroundStyle(.yellow.opacity(0.22))
        }
        .frame(width: horizontalSizeClass == .compact ? 116 : 170)
        .padding(.top, 8)
    }

    private var launcherContent: some View {
        HStack(alignment: .top, spacing: 12) {
            sidebar
            HStack(alignment: .top, spacing: 12) {
                        ForEach(GameEdition.allCases) { game in
                            VStack(alignment: .leading, spacing: 10) {
                                ZStack(alignment: .bottomLeading) {
                                    Group {
                                        if let localURL = (Bundle.main.url(forResource: game.bundledArtworkName, withExtension: "jpg", subdirectory: "Resources")
                                            ?? Bundle.main.url(forResource: game.bundledArtworkName, withExtension: "jpg")),
                                           let data = try? Data(contentsOf: localURL),
                                           let image = UIImage(data: data) {
                                            Image(uiImage: image).resizable().scaledToFill()
                                        } else {
                                            AsyncImage(url: game.artworkURL) { phase in
                                                if let image = phase.image {
                                                    image.resizable().scaledToFill()
                                                } else {
                                                    ZStack {
                                                        Color.black
                                                        Image(systemName: game.symbol)
                                                            .font(.system(size: 90, weight: .ultraLight))
                                                            .foregroundStyle(.yellow.opacity(0.3))
                                                    }
                                                }
                                            }
                                        }
                                    }
                                    .frame(maxWidth: .infinity).frame(height: 245).clipped()
                                    LinearGradient(colors: [.clear, .black.opacity(0.9)],
                                                   startPoint: .top, endPoint: .bottom)
                                    // Artwork contains its own visual treatment; keep text readable
                                    // without placing a second oversized title over the illustration.
                                    EmptyView() // Poster already includes its own game title
                                }
                                .frame(height: 245)
                                .clipShape(RoundedRectangle(cornerRadius: 10))
                                Text(russian ? game.russianName : game.englishName)
                                    .font(.headline).foregroundStyle(.yellow)
                                Label(resources.status[game.rawValue] == nil ? (russian ? "Ресурсы не найдены" : "Resources not found") : (russian ? "Ресурсы добавлены" : "Resources imported"), systemImage: resources.status[game.rawValue] == nil ? "arrow.down.circle" : "checkmark.circle.fill")
                                    .font(.caption).foregroundStyle(resources.status[game.rawValue] == nil ? .gray : .green)
                                if showDiagnostics, let status = resources.status[game.rawValue] {
                                    Text(status).font(.caption).foregroundStyle(.green)
                                }
                                if showDiagnostics, let archiveCount = resources.archiveCounts[game.rawValue] {
                                    Text(russian ? "Архивов X-Ray: \(archiveCount)" : "X-Ray archives: \(archiveCount)")
                                        .font(.caption).foregroundStyle(.secondary)
                                }
                                if showDiagnostics, let summary = resources.archiveSummaries[game.rawValue] {
                                    Text(summary).font(.caption2).foregroundStyle(.secondary)
                                }
                                if showDiagnostics, let header = resources.archiveHeaders[game.rawValue] {
                                    Text(header).font(.caption2.monospaced()).foregroundStyle(.gray)
                                        .lineLimit(1)
                                }
                                if showDiagnostics, let bytes = resources.storageBytes[game.rawValue] {
                                    Text(ByteCountFormatter.string(fromByteCount: bytes, countStyle: .file))
                                        .font(.caption).foregroundStyle(.secondary)
                                }
                                if showDiagnostics, let diagnostic = resources.diagnostics[game.rawValue] {
                                    Text(diagnostic).font(.caption2.monospaced()).foregroundStyle(.gray)
                                }
                                if showDiagnostics, let engineResult = engineDiagnostics[game.rawValue] {
                                    Label(engineResult, systemImage: "cpu")
                                        .font(.caption)
                                        .foregroundStyle(.yellow)
                                }
                                if showDiagnostics { NavigationLink {
                                    XRayLevelListView(gameID: game.rawValue,
                                                      gameName: russian ? game.russianName : game.englishName)
                                } label: {
                                    Label(russian ? "Локации" : "Locations", systemImage: "map")
                                }
                                .buttonStyle(.bordered)
                                .tint(.yellow)
                                }
                                HStack {
                                    if showDiagnostics { Button(russian ? "Проверить ядро" : "Check engine") {
                                        checkEngineResources(for: game)
                                    }
                                    .buttonStyle(.bordered)
                                    .tint(.green)
                                    .disabled(isCopying)
                                    }
                                    Button {
                                        if resources.status[game.rawValue] == nil {
                                            selectedGame = game
                                            showImporter = true
                                        } else {
                                            showEngineNotice = true
                                        }
                                    } label: {
                                        Label(resources.status[game.rawValue] == nil
                                              ? (russian ? "Добавить файлы игры" : "Add game files")
                                              : (russian ? "Проверить запуск" : "Check launch"),
                                              systemImage: resources.status[game.rawValue] == nil ? "folder" : "play.fill")
                                            .frame(maxWidth: .infinity)
                                            .lineLimit(1)
                                            .minimumScaleFactor(0.8)
                                    }
                                    .buttonStyle(.borderedProminent).tint(.yellow)
                                    .disabled(resources.importInProgress || isCopying)

                                }
                            }
                            .frame(maxWidth: .infinity, alignment: .leading)
                            .padding(10)
                            .background(Color.black.opacity(0.72), in: RoundedRectangle(cornerRadius: 16))
                            .overlay(RoundedRectangle(cornerRadius: 16).stroke(.yellow.opacity(0.18), lineWidth: 1))
                        }

            }
        }
    }

    var body: some View {
        NavigationStack {
            ZStack {
                ZStack {
                    LinearGradient(colors: [Color(red: 0.035, green: 0.055, blue: 0.055),
                                             Color(red: 0.11, green: 0.13, blue: 0.10)],
                                   startPoint: .topLeading, endPoint: .bottomTrailing)
                    AsyncImage(url: Bundle.main.url(forResource: "pripyat_header", withExtension: "jpg") ?? URL(string: "https://unsplash.com/photos/IM4b4ytWetg/download?w=1600")) { phase in
                        if let image = phase.image {
                            image.resizable().scaledToFill()
                        } else {
                            Color(red: 0.17, green: 0.13, blue: 0.11)
                        }
                    }
                    .frame(maxWidth: .infinity, maxHeight: .infinity)
                    .clipped()
                    GeometryReader { geometry in
                        Canvas { context, size in
                            let radius = min(size.width * 0.13, size.height * 0.27)
                            let center = CGPoint(x: size.width * 0.57, y: size.height * 0.29)
                            let iron = Color.black.opacity(0.82)
                            let circle = CGRect(x: center.x - radius, y: center.y - radius,
                                                width: radius * 2, height: radius * 2)
                            context.stroke(Path(ellipseIn: circle), with: .color(iron), lineWidth: 4)
                            for index in 0..<16 {
                                let angle = Double(index) * .pi / 8
                                let end = CGPoint(x: center.x + radius * cos(angle),
                                                  y: center.y + radius * sin(angle))
                                var spoke = Path()
                                spoke.move(to: center)
                                spoke.addLine(to: end)
                                context.stroke(spoke, with: .color(iron), lineWidth: 2)
                                let cabin = CGRect(x: end.x - 6, y: end.y - 3, width: 12, height: 10)
                                context.fill(Path(roundedRect: cabin, cornerRadius: 2), with: .color(iron))
                            }
                            let baseY = center.y + radius * 1.65
                            for x in [center.x - radius * 0.38, center.x + radius * 0.38] {
                                var leg = Path()
                                leg.move(to: center)
                                leg.addLine(to: CGPoint(x: x, y: baseY))
                                context.stroke(leg, with: .color(iron), lineWidth: 5)
                            }
                        }
                        .accessibilityHidden(true)
                    }
                    .allowsHitTesting(false)
                    LinearGradient(colors: [.black.opacity(0.65), .black.opacity(0.86)],
                                   startPoint: .top, endPoint: .bottom)
                }.ignoresSafeArea()
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
                            Menu {
                                Button("Русский") { language = "ru" }
                                Button("English") { language = "en" }
                            } label: {
                                Label(russian ? "Русский" : "English", systemImage: "globe")
                                    .foregroundStyle(.white)
                                    .padding(.horizontal, 12)
                                    .padding(.vertical, 8)
                                    .background(.black.opacity(0.65), in: RoundedRectangle(cornerRadius: 9))
                            }
                            Button {
                                selectedSection = "settings"
                                showDiagnostics.toggle()
                            } label: {
                                Image(systemName: "gearshape")
                                    .font(.title3).foregroundStyle(.white)
                                    .padding(10)
                                    .background(.black.opacity(0.65), in: RoundedRectangle(cornerRadius: 9))
                            }
                            .accessibilityLabel(russian ? "Настройки диагностики" : "Diagnostic settings")
                            Button { showAbout = true } label: {
                                Image(systemName: "info.circle")
                                    .font(.title3).foregroundStyle(.white)
                                    .padding(10)
                                    .background(.black.opacity(0.65), in: RoundedRectangle(cornerRadius: 9))
                            }
                            .accessibilityLabel(russian ? "О проекте" : "About")
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
                        ViewThatFits(in: .horizontal) {
                            launcherContent
                            ScrollView(.horizontal, showsIndicators: false) {
                                launcherContent.frame(minWidth: 900)
                            }
                        }
                        HStack(spacing: 14) {
                            Image(systemName: "newspaper.fill")
                                .font(.title2)
                                .foregroundStyle(.yellow)
                            VStack(alignment: .leading, spacing: 5) {
                                Text(russian ? "Новости проекта" : "Project news")
                                    .font(.headline).foregroundStyle(.yellow)
                                Text(russian ? "STALKER Mobile — лаунчер трилогии для iOS. Игровой движок пока в разработке." : "STALKER Mobile — iOS trilogy launcher. Game engine integration is in progress.")
                                    .font(.subheadline).foregroundStyle(.white.opacity(0.75))
                            }
                            Spacer()
                        }
                        .padding(16)
                        .background(.black.opacity(0.55), in: RoundedRectangle(cornerRadius: 12))
                        if showDiagnostics {
                            Text(russian ? "Режим диагностики включён" : "Diagnostics enabled")
                                .font(.caption).foregroundStyle(.yellow)
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
            .alert(russian ? "Запуск пока недоступен" : "Launch not available", isPresented: $showEngineNotice) {
                Button("OK", role: .cancel) {}
            } message: {
                Text(russian ? "Ресурсы импортированы, но порт игрового движка X-Ray для iOS ещё не готов." : "Resources are imported, but the X-Ray game engine port for iOS is not ready.")
            }
            .alert(russian ? "О проекте" : "About", isPresented: $showAbout) {
                Button("OK", role: .cancel) {}
            } message: {
                Text(russian ? "STALKER Mobile — тестовый лаунчер. Порт X-Ray для iOS ещё не готов." : "STALKER Mobile is a prototype launcher. The iOS X-Ray port is not ready yet.")
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
