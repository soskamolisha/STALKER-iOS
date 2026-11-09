import SwiftUI

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
    private let engine: any GameEngine = UnavailableEngine()
    @State private var status = "Движок X-Ray не подключён"

    var body: some View {
        GeometryReader { _ in
            ZStack {
                MetalSurface().ignoresSafeArea()
                VStack(spacing: 18) {
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
                    Spacer()
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
                        if engine.isAvailable {
                            engine.handle(input, pressed: true)
                        } else {
                            status = "Управление недоступно: движок не подключён"
                        }
                    }
                    .onEnded { _ in
                        if engine.isAvailable {
                            engine.handle(input, pressed: false)
                        }
                    }
            )
            .accessibilityLabel(title)
    }
}
