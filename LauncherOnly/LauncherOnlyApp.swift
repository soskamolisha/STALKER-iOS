import SwiftUI
import UIKit

@main
struct LauncherOnlyApp: App {
    var body: some Scene {
        WindowGroup { LauncherOnlyView().preferredColorScheme(.dark) }
    }
}

private enum Edition: String, CaseIterable, Identifiable {
    case shadow, pripyat, clearSky
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
    private let menu: [(String, String)] = [
        ("Главная", "house.fill"), ("Игры", "square.grid.2x2"),
        ("Импорт ресурсов", "square.and.arrow.down"), ("Настройки", "gearshape"),
        ("Сохранения", "externaldrive"), ("О проекте", "info.circle")
    ]

    var body: some View {
        GeometryReader { geo in
            ZStack {
                LinearGradient(stops: [
                    .init(color: Color(red: 0.09, green: 0.10, blue: 0.11), location: 0),
                    .init(color: Color(red: 0.37, green: 0.28, blue: 0.22), location: 0.40),
                    .init(color: Color(red: 0.55, green: 0.32, blue: 0.20), location: 0.64),
                    .init(color: Color(red: 0.11, green: 0.12, blue: 0.14), location: 1)
                ], startPoint: .bottom, endPoint: .top).ignoresSafeArea()
                if showWheel {
                    if let url = Bundle.main.url(forResource: "panorama_pripyat", withExtension: "png"),
                       let background = UIImage(contentsOfFile: url.path) {
                        GeometryReader { backgroundArea in
                            Image(uiImage: background)
                                .resizable()
                                .aspectRatio(contentMode: .fill)
                                .frame(width: backgroundArea.size.width, height: backgroundArea.size.height)
                                .clipped()
                        }
                        .ignoresSafeArea()
                        .allowsHitTesting(false)
                    } else {
                        wheel.ignoresSafeArea()
                    }
                }
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
                                    .font(.system(size: 29, weight: .black, design: .monospaced).width(.condensed))
                                    .tracking(-1.1)
                                    .foregroundStyle(.white)
                                    .lineLimit(1)
                                    .minimumScaleFactor(0.6)
                                    .overlay {
                                        GeometryReader { titleArea in
                                            Canvas { context, size in
                                                // Weathered stencil cuts inspired by the reference lettering.
                                                let cuts: [(CGFloat, CGFloat, CGFloat)] = [
                                                    (0.12, 0.29, 0.23), (0.27, 0.04, 0.38),
                                                    (0.45, 0.52, 0.17), (0.64, 0.15, 0.28),
                                                    (0.83, 0.43, 0.18), (0.94, 0.10, 0.22)
                                                ]
                                                for (x, y, width) in cuts {
                                                    let rect = CGRect(x: size.width * x, y: size.height * y,
                                                                      width: size.width * width, height: 1.7)
                                                    context.fill(Path(rect), with: .color(.black.opacity(0.88)))
                                                }
                                            }
                                            .allowsHitTesting(false)
                                        }
                                    }
                                    .shadow(color: .black, radius: 1, x: 2, y: 3)
                                Text("ТРИЛОГИЯ · ЛАУНЧЕР")
                                    .font(.caption).foregroundStyle(.white.opacity(0.8))
                            }
                            Spacer()
                            Text("X-RAY OFF").font(.caption.bold()).foregroundStyle(.orange)
                        }
                        HStack(alignment: .top, spacing: 5) {
                            ForEach(Edition.allCases) { game in
                                VStack(spacing: 6) {
                                    if let url = Bundle.main.url(forResource: "card_\(game.rawValue)", withExtension: "jpg"),
                                       let image = UIImage(contentsOfFile: url.path) {
                                        Image(uiImage: image)
                                            .resizable()
                                            .aspectRatio(contentMode: .fit)
                                            .frame(maxWidth: .infinity)
                                            .clipShape(RoundedRectangle(cornerRadius: 7))
                                    } else {
                                        Image(systemName: "radiation")
                                            .resizable()
                                            .scaledToFit()
                                            .frame(height: 180)
                                            .foregroundStyle(.yellow)
                                    }
                                    Button("Выбрать игру") { selectedGame = game }
                                        .font(.caption.bold())
                                        .buttonStyle(.borderedProminent)
                                        .tint(.yellow)
                                }
                                .frame(maxWidth: .infinity, alignment: .top)
                                .padding(5)
                                .background(.black.opacity(0.82), in: RoundedRectangle(cornerRadius: 9))
                                .overlay(RoundedRectangle(cornerRadius: 9).stroke(.yellow.opacity(0.4), lineWidth: 0.8))
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
                        if let url = Bundle.main.url(forResource: "card_\(game.rawValue)", withExtension: "jpg"),
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
                // Pripyat wheel: oversized silhouette with suspended passenger cabins.
                let center = CGPoint(x: size.width * 0.72, y: size.height * 0.40)
                let radius = min(size.width * 0.31, size.height * 0.48)
                let steel = Color(red: 0.055, green: 0.057, blue: 0.052).opacity(0.91)
                let frame = CGRect(x: center.x - radius, y: center.y - radius,
                                   width: radius * 2, height: radius * 2)
                ctx.stroke(Path(ellipseIn: frame), with: .color(steel), lineWidth: 5)
                let inner = radius * 0.78
                ctx.stroke(Path(ellipseIn: CGRect(x: center.x-inner, y: center.y-inner,
                                                   width: inner*2, height: inner*2)),
                           with: .color(steel), lineWidth: 2)
                for index in 0..<20 {
                    let angle = Double(index) * .pi / 10
                    let outer = CGPoint(x: center.x + radius*cos(angle),
                                        y: center.y + radius*sin(angle))
                    var spoke = Path()
                    spoke.move(to: center)
                    spoke.addLine(to: outer)
                    ctx.stroke(spoke, with: .color(steel), lineWidth: 2.5)
                    let cabinWidth = max(12, radius * 0.16)
                    let cabinHeight = cabinWidth * 0.68
                    let cabin = CGRect(x: outer.x - cabinWidth/2,
                                       y: outer.y + 3, width: cabinWidth, height: cabinHeight)
                    var suspension = Path()
                    suspension.move(to: outer)
                    suspension.addLine(to: CGPoint(x: outer.x, y: cabin.minY))
                    ctx.stroke(suspension, with: .color(steel), lineWidth: 2)
                    ctx.fill(Path(roundedRect: cabin, cornerRadius: 2),
                             with: .color(steel))
                    let window = CGRect(x: cabin.minX + cabinWidth*0.18,
                                        y: cabin.minY + cabinHeight*0.18,
                                        width: cabinWidth*0.64, height: cabinHeight*0.32)
                    ctx.fill(Path(window), with: .color(Color(red: 0.34, green: 0.25, blue: 0.20).opacity(0.55)))
                }
                var support = Path()
                support.move(to: center)
                support.addLine(to: CGPoint(x: center.x-radius*0.52, y: size.height))
                support.move(to: center)
                support.addLine(to: CGPoint(x: center.x+radius*0.52, y: size.height))
                ctx.stroke(support, with: .color(steel), lineWidth: 9)
                var horizon = Path()
                horizon.move(to: CGPoint(x: 0, y: size.height * 0.88))
                for i in 0...28 {
                    let x = size.width * CGFloat(i) / 28
                    let h = CGFloat([0.05,0.10,0.08,0.14,0.06,0.12,0.07][i % 7])
                    horizon.addLine(to: CGPoint(x: x, y: size.height*(0.88-h)))
                    horizon.addLine(to: CGPoint(x: x, y: size.height*0.88))
                }
                horizon.addLine(to: CGPoint(x: size.width, y: size.height))
                horizon.addLine(to: CGPoint(x: 0, y: size.height))
                horizon.closeSubpath()
                ctx.fill(horizon, with: .color(steel.opacity(0.75)))
            }
            .overlay(Color.black.opacity(0.13))
            .allowsHitTesting(false)
        }
    }
}
