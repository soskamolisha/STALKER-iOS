import SwiftUI
import UIKit

@main
struct LauncherOnlyApp: App {
    var body: some Scene {
        WindowGroup { LauncherOnlyView().preferredColorScheme(.dark) }
    }
}

private enum Edition: String, CaseIterable, Identifiable {
    case shadow, clearSky, pripyat
    var id: String { rawValue }
    var title: String {
        switch self {
        case .shadow: return "Тень Чернобыля"
        case .clearSky: return "Чистое небо"
        case .pripyat: return "Зов Припяти"
        }
    }
}

struct LauncherOnlyView: View {
    @State private var section = "Главная"
    @State private var infoSection: String?
    @State private var settings = false
    @State private var selectedGame: Edition?
    @AppStorage("launcher.showWheel") private var showWheel = true
    @AppStorage("launcher.largeCards") private var largeCards = false
    @AppStorage("launcher.showResourceLabels") private var showResourceLabels = true
    private let menu: [(String, String)] = [
        ("Главная", "house.fill"), ("Игры", "square.grid.2x2"),
        ("Импорт ресурсов", "square.and.arrow.down"), ("Настройки", "gearshape"),
        ("Сохранения", "externaldrive"), ("О проекте", "info.circle")
    ]

    var body: some View {
        GeometryReader { geo in
            ZStack {
                LinearGradient(colors: [.black, Color(red: 0.14, green: 0.16, blue: 0.12)],
                               startPoint: .bottom, endPoint: .top).ignoresSafeArea()
                if showWheel { wheel.opacity(0.30).ignoresSafeArea() }
                HStack(alignment: .top, spacing: 12) {
                    VStack(alignment: .leading, spacing: 8) {
                        ForEach(menu, id: \.0) { item in
                            Button {
                                section = item.0
                                if item.0 == "Настройки" { settings = true }
                                else if item.0 == "Импорт ресурсов" || item.0 == "Сохранения" || item.0 == "О проекте" { infoSection = item.0 }
                            } label: {
                                Label(item.0, systemImage: item.1)
                                    .font(.system(size: 12, weight: .semibold))
                                    .frame(maxWidth: .infinity, alignment: .leading)
                                    .padding(10)
                                    .foregroundStyle(section == item.0 ? .yellow : .white)
                                    .background(.black.opacity(0.7), in: RoundedRectangle(cornerRadius: 8))
                            }.buttonStyle(.plain)
                        }
                        Spacer()
                        Image(systemName: "radiation")
                            .font(.system(size: 70)).foregroundStyle(.yellow.opacity(0.4))
                    }
                    .frame(width: min(165, geo.size.width * 0.22))
                    VStack(alignment: .leading, spacing: 10) {
                        HStack {
                            VStack(alignment: .leading) {
                                Text("S.T.A.L.K.E.R. MOBILE")
                                    .font(.system(size: 23, weight: .black, design: .monospaced))
                                    .foregroundStyle(.yellow)
                                Text("ТРИЛОГИЯ · ЛАУНЧЕР")
                                    .font(.caption).foregroundStyle(.white.opacity(0.8))
                            }
                            Spacer()
                            Text("X-RAY OFF").font(.caption.bold()).foregroundStyle(.orange)
                        }
                        HStack(alignment: .top, spacing: largeCards ? 6 : 10) {
                            ForEach(Edition.allCases) { game in
                                VStack(alignment: .leading, spacing: 9) {
                                    GeometryReader { area in
                                        if let url = Bundle.main.url(forResource: "card_\(game.rawValue)", withExtension: "jpg"),
                                           let image = UIImage(contentsOfFile: url.path) {
                                            Image(uiImage: image).resizable().scaledToFill()
                                                .frame(width: area.size.width, height: area.size.height).clipped()
                                        } else {
                                            ZStack {
                                                Color.black
                                                Image(systemName: "radiation").foregroundStyle(.yellow)
                                            }
                                        }
                                    }
                                    .frame(height: largeCards ? 235 : 205)
                                    .clipShape(RoundedRectangle(cornerRadius: 9))
                                    Text(game.title).font(.headline).foregroundStyle(.yellow)
                                        .lineLimit(1).minimumScaleFactor(0.65)
                                    if showResourceLabels {
                                        Label("Ресурсы не добавлены", systemImage: "folder")
                                            .font(.caption2).foregroundStyle(.gray)
                                    }
                                    Button("Выбрать игру") { selectedGame = game }
                                        .font(.caption.bold()).buttonStyle(.borderedProminent).tint(.yellow)
                                        .frame(maxWidth: .infinity)
                                }
                                .padding(9).frame(maxWidth: .infinity)
                                .background(.black.opacity(0.85), in: RoundedRectangle(cornerRadius: 12))
                                .overlay(RoundedRectangle(cornerRadius: 12).stroke(.yellow.opacity(0.4)))
                            }
                        }
                        Spacer(minLength: 0)
                        Text("Только интерфейс · игровой движок отключён")
                            .font(.caption).foregroundStyle(.white.opacity(0.7))
                    }
                }.padding(16)
            }
        }
        .sheet(isPresented: Binding(
            get: { infoSection != nil },
            set: { if !$0 { infoSection = nil } }
        )) {
            NavigationStack {
                ZStack {
                    Color.black.ignoresSafeArea()
                    VStack(spacing: 18) {
                        Image(systemName: infoSection == "Импорт ресурсов" ? "square.and.arrow.down" :
                              infoSection == "Сохранения" ? "externaldrive" : "radiation")
                            .font(.system(size: 55))
                            .foregroundStyle(.yellow)
                        Text(infoSection ?? "STALKER Mobile")
                            .font(.title2.bold()).foregroundStyle(.yellow)
                        Text(sectionDescription)
                            .multilineTextAlignment(.center)
                            .foregroundStyle(.white.opacity(0.85))
                        if infoSection == "О проекте" {
                            LabeledContent("Версия", value: "Launcher Only")
                                .foregroundStyle(.white)
                            LabeledContent("X-Ray", value: "Не подключён")
                                .foregroundStyle(.white)
                        }
                        Button("Вернуться в меню") { infoSection = nil }
                            .buttonStyle(.borderedProminent).tint(.yellow)
                    }
                    .padding(28)
                }
                .toolbar {
                    ToolbarItem(placement: .confirmationAction) {
                        Button("Закрыть") { infoSection = nil }
                    }
                }
            }
        }
        .sheet(item: $selectedGame) { game in
            NavigationStack {
                ZStack {
                    Color.black.ignoresSafeArea()
                    VStack(spacing: 16) {
                        if let url = Bundle.main.url(forResource: "card_\\(game.rawValue)", withExtension: "jpg"),
                           let image = UIImage(contentsOfFile: url.path) {
                            Image(uiImage: image)
                                .resizable()
                                .scaledToFit()
                                .frame(maxHeight: 260)
                                .clipShape(RoundedRectangle(cornerRadius: 12))
                        }
                        Text(game.title)
                            .font(.title2.bold())
                            .foregroundStyle(.yellow)
                        Label("Файлы игры пока не добавлены", systemImage: "folder.badge.questionmark")
                            .foregroundStyle(.white)
                        Text("Это версия лаунчера без X-Ray. Игру запустить нельзя, но карточки и меню работают.")
                            .multilineTextAlignment(.center)
                            .font(.footnote)
                            .foregroundStyle(.gray)
                        Button("Вернуться к играм") { selectedGame = nil }
                            .buttonStyle(.borderedProminent)
                            .tint(.yellow)
                    }
                    .padding(24)
                }
                .navigationTitle("Информация об игре")
                .navigationBarTitleDisplayMode(.inline)
                .toolbar {
                    ToolbarItem(placement: .confirmationAction) {
                        Button("Закрыть") { selectedGame = nil }
                    }
                }
            }
        }
        .sheet(isPresented: $settings) {
            NavigationStack {
                Form {
                    LabeledContent("Версия", value: "Launcher Only")
                    LabeledContent("Движок X-Ray", value: "Отключён")
                    LabeledContent("Карточки", value: "3")
                    Toggle("Колесо обозрения на фоне", isOn: $showWheel)
                    Toggle("Увеличенные карточки", isOn: $largeCards)
                    Toggle("Показывать подписи ресурсов", isOn: $showResourceLabels)
                }
                .navigationTitle("Настройки")
                .toolbar {
                    ToolbarItem(placement: .confirmationAction) {
                        Button("Готово") { settings = false }
                    }
                }
            }
        }
    }

    private var sectionDescription: String {
        switch infoSection {
        case "Импорт ресурсов":
            return "Импорт игровых файлов будет доступен после проверки безопасного подключения ресурсов. В этой версии импорт отключён."
        case "Сохранения":
            return "Сохранений пока нет. Этот лаунчер не запускает игры и не создаёт игровые сохранения."
        default:
            return "STALKER Mobile — интерфейс выбора трёх игр. Эта сборка предназначена для проверки дизайна и установки на iPhone."
        }
    }

    private var wheel: some View {
        GeometryReader { geo in
            Canvas { ctx, size in
                let center = CGPoint(x: size.width * 0.75, y: size.height * 0.42)
                let radius = min(size.width * 0.19, size.height * 0.34)
                let ink = Color.black.opacity(0.9)
                ctx.stroke(Path(ellipseIn: CGRect(x: center.x-radius, y: center.y-radius,
                                                 width: radius*2, height: radius*2)),
                           with: .color(ink), lineWidth: 5)
                for index in 0..<16 {
                    let angle = Double(index) * .pi / 8
                    var spoke = Path()
                    spoke.move(to: center)
                    spoke.addLine(to: CGPoint(x: center.x + radius*cos(angle),
                                              y: center.y + radius*sin(angle)))
                    ctx.stroke(spoke, with: .color(ink), lineWidth: 3)
                }
                var support = Path()
                support.move(to: center)
                support.addLine(to: CGPoint(x: center.x-radius*0.6, y: size.height))
                support.move(to: center)
                support.addLine(to: CGPoint(x: center.x+radius*0.6, y: size.height))
                ctx.stroke(support, with: .color(ink), lineWidth: 7)
            }
        }
    }
}
