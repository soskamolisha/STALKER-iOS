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

struct GameShellView: View {
    @State private var status = "Движок iOS ещё не подключён"
    var body: some View {
        GeometryReader { geometry in
            ZStack {
                Color.black.ignoresSafeArea()
                VStack(spacing: 18) {
                    Text("S.T.A.L.K.E.R. MOBILE")
                        .font(.system(size: 27, weight: .bold, design: .monospaced))
                        .foregroundStyle(.orange)
                    Text("iOS — подготовительный проект")
                        .foregroundStyle(.white)
                    Text(status)
                        .foregroundStyle(.gray)
                        .font(.footnote)
                    Spacer()
                    HStack {
                        control("◀")
                        control("▲")
                        control("▶")
                        Spacer()
                        control("ПРИЦЕЛ")
                        control("ОГОНЬ")
                    }
                }
                .padding(24)
            }
        }
    }
    private func control(_ title: String) -> some View {
        Button(title) { status = "Управление: ожидает подключения движка" }
            .font(.system(size: 12, weight: .bold))
            .foregroundStyle(.white)
            .padding(12)
            .background(.gray.opacity(0.45), in: RoundedRectangle(cornerRadius: 10))
    }
}
