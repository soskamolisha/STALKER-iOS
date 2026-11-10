import SwiftUI
import UniformTypeIdentifiers

@main
struct StalkerStarterApp: App {
    var body: some Scene {
        WindowGroup {
            GameShellView()
                .preferredColorScheme(.dark)
        }
    }
}

/// Prototype shell. No game rendering or native X-Ray runtime is bundled.
struct GameShellView: View {
    @State private var engine = UnavailableEngine()
    @Environment(\.scenePhase) private var scenePhase
    @State private var status = "Движок X-Ray не подключён"
    @State private var showArchivePicker = false
    @State private var archiveStatus = "Можно проверить структуру своего файла .db"

    var body: some View {
        GeometryReader { _ in
            ZStack {
                MetalSurface().ignoresSafeArea()
                VStack(spacing: 9) {
                    Text("S.T.A.L.K.E.R. MOBILE")
                        .font(.system(size: 27, weight: .bold, design: .monospaced))
                        .foregroundStyle(.orange)
                    Text("iOS — прототип интерфейса")
                        .foregroundStyle(.white)
                    Text(status)
                        .foregroundStyle(.gray)
                        .font(.footnote)
                    Button("Проверить движок") {
                        do {
                            try engine.start()
                            status = "Движок запущен"
                        } catch {
                            status = error.localizedDescription
                        }
                    }
                    .buttonStyle(.borderedProminent)
                    .tint(.orange)
                    Button("Архив .db") { showArchivePicker = true }
                        .buttonStyle(.bordered)
                        .tint(.orange)
                    Text(archiveStatus)
                        .foregroundStyle(.gray)
                        .font(.caption2)
                        .lineLimit(2)
                        .accessibilityIdentifier("archiveStatus")
                    Spacer(minLength: 2)
                    HStack {
                        control("◀", input: .moveLeft)
                        control("▲", input: .moveForward)
                        control("▶", input: .moveRight)
                        Spacer()
                        control("ПРИЦЕЛ", input: .aim)
                        control("ОГОНЬ", input: .fire)
                    }
                }
                .padding(24)
            }
        }
        .fileImporter(isPresented: $showArchivePicker, allowedContentTypes: [.item]) { result in
            switch result {
            case .success(let url): archiveStatus = ArchiveProbe.inspect(url)
            case .failure: archiveStatus = "Выбор файла отменён или недоступен."
            }
        }
        .onChange(of: scenePhase) { _, phase in
            if phase != .active { engine.stop() }
        }
        .onDisappear { engine.stop() }
    }

    private func control(_ title: String, input: GameInput) -> some View {
        Text(title)
            .font(.system(size: 12, weight: .bold))
            .foregroundStyle(.white)
            .padding(12)
            .background(.gray.opacity(0.45), in: RoundedRectangle(cornerRadius: 10))
            .contentShape(Rectangle())
            .gesture(
                DragGesture(minimumDistance: 0)
                    .onChanged { _ in
                        engine.handle(input, pressed: true)
                    }
                    .onEnded { _ in
                        engine.handle(input, pressed: false)
                    }
            )
            .accessibilityLabel(title)
    }
}
