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
    @State private var engine = UnavailableEngine()
    @Environment(\.scenePhase) private var scenePhase
    @State private var status = "Движок X-Ray не подключён"
    @State private var coreCheck = "xrCore CRC32 не проверен"
    @State private var chunkCheck = "Чтение чанков не проверено"

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
                    Text(coreCheck)
                        .foregroundStyle(.green)
                        .font(.footnote)
                    Button("Проверить xrCore CRC32") {
                        let bytes = Array("123456789".utf8)
                        let checksum = bytes.withUnsafeBytes { raw in
                            st_openxray_crc32(raw.baseAddress, UInt32(raw.count))
                        }
                        coreCheck = checksum == 0xCBF43926 ? "xrCore CRC32 работает" : "Ошибка xrCore CRC32"
                    }
                    .buttonStyle(.bordered)
                    .tint(.green)
                    Text(chunkCheck)
                        .foregroundStyle(.green)
                        .font(.footnote)
                    Button("Проверить структуру чанков") {
                        // Synthetic test data only: chunk ID 42, size 3, payload ABC.
                        let bytes: [UInt8] = [42, 0, 0, 0, 3, 0, 0, 0, 65, 66, 67]
                        var count: UInt32 = 0
                        var firstID: UInt32 = 0
                        var firstSize: UInt32 = 0
                        let result = bytes.withUnsafeBytes { raw in
                            st_chunk_scan(raw.bindMemory(to: UInt8.self).baseAddress,
                                          UInt32(raw.count), &count, &firstID, &firstSize)
                        }
                        chunkCheck = result == 0 && count == 1 && firstID == 42 && firstSize == 3
                            ? "Структура чанков: тест пройден" : "Ошибка чтения чанков"
                    }
                    .buttonStyle(.bordered)
                    .tint(.green)
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
