import SwiftUI
import UniformTypeIdentifiers

/// Trilogy selection screen. Game launching remains disabled until a compatible iOS engine exists.
struct TrilogyLauncherView: View {
    @AppStorage("stalker.language") private var language = "ru"
    @State private var selectedGame: GameEdition?
    @State private var showImporter = false
    @StateObject private var resources = GameResourceStore()
    @State private var importError: String?

    enum GameEdition: String, CaseIterable, Identifiable {
        case shadow = "shadow"
        case clearSky = "clearSky"
        case pripyat = "pripyat"
        var id: String { rawValue }
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

    var body: some View {
        NavigationStack {
            ZStack {
                Color(red: 0.075, green: 0.085, blue: 0.075).ignoresSafeArea()
                ScrollView {
                    VStack(alignment: .leading, spacing: 18) {
                        HStack {
                            Text("STALKER MOBILE").font(.largeTitle.bold()).foregroundStyle(.yellow)
                            Spacer()
                            Picker("Language", selection: $language) {
                                Text("РУС").tag("ru")
                                Text("ENG").tag("en")
                            }
                            .pickerStyle(.segmented)
                            .frame(width: 150)
                        }
                        Text(russian ? "Выберите игру" : "Choose a game")
                            .font(.title2).foregroundStyle(.white)
                        ForEach(GameEdition.allCases) { game in
                            VStack(alignment: .leading, spacing: 10) {
                                Text(russian ? game.russianName : game.englishName)
                                    .font(.title3.bold()).foregroundStyle(.white)
                                Text(russian ? "Игровые ресурсы добавляются отдельно" : "Import your own game resources")
                                    .foregroundStyle(.gray)
                                if let status = resources.status[game.rawValue] {
                                    Text(status).font(.caption).foregroundStyle(.green)
                                }
                                if let bytes = resources.storageBytes[game.rawValue] {
                                    Text(ByteCountFormatter.string(fromByteCount: bytes, countStyle: .file))
                                        .font(.caption).foregroundStyle(.secondary)
                                }
                                if let diagnostic = resources.diagnostics[game.rawValue] {
                                    Text(diagnostic).font(.caption2.monospaced()).foregroundStyle(.gray)
                                }
                                HStack {
                                    Button(russian ? "Импортировать файлы" : "Import files") {
                                        selectedGame = game
                                        showImporter = true
                                    }
                                    .buttonStyle(.borderedProminent).tint(.yellow)
                                    .disabled(resources.importInProgress)
                                    Button(russian ? "Запустить" : "Launch") {}
                                        .buttonStyle(.bordered)
                                        .disabled(true)
                                }
                            }
                            .frame(maxWidth: .infinity, alignment: .leading)
                            .padding()
                            .background(Color.white.opacity(0.08), in: RoundedRectangle(cornerRadius: 12))
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
                for game in GameEdition.allCases { resources.refresh(gameID: game.rawValue) }
            }
            .alert("Import error", isPresented: Binding(get: { importError != nil }, set: { if !$0 { importError = nil } })) {
                Button("OK", role: .cancel) { importError = nil }
            } message: { Text(importError ?? "") }
            .fileImporter(isPresented: $showImporter, allowedContentTypes: [.folder]) { result in
                guard let selectedGame else { return }
                do {
                    let folder = try result.get()
                    try resources.importFolder(folder, gameID: selectedGame.rawValue)
                    self.selectedGame = nil
                } catch {
                    importError = error.localizedDescription
                    self.selectedGame = nil
                }
            }
        }
    }
}
