import SwiftUI
import UniformTypeIdentifiers

@main
struct StalkerStarterApp: App {
    var body: some Scene {
        WindowGroup {
            TrilogyLauncherView()
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
    @State private var isImportingFile = false
    @State private var importedFileStatus = "Файл ещё не выбран"
    @State private var chunkDetails: [String] = []

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
                    Button("Проверить свой файл") {
                        isImportingFile = true
                    }
                    .buttonStyle(.bordered)
                    .tint(.orange)
                    Text(importedFileStatus)
                        .font(.footnote)
                        .foregroundStyle(.white)
                        .lineLimit(4)
                    ForEach(chunkDetails, id: \.self) { detail in
                        Text(detail)
                            .font(.system(size: 11, design: .monospaced))
                            .foregroundStyle(.white)
                            .lineLimit(2)
                    }
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
        .fileImporter(isPresented: $isImportingFile,
                      allowedContentTypes: [.data, .item]) { result in
            switch result {
            case .success(let url):
                inspectSelectedFile(url)
            case .failure(let error):
                importedFileStatus = "Не удалось выбрать файл: \(error.localizedDescription)"
            }
        }
        .onChange(of: scenePhase) { _, phase in
            if phase != .active { engine.stop() }
        }
        .onDisappear { engine.stop() }
    }

    private func inspectSelectedFile(_ url: URL) {
        chunkDetails = []
        let access = url.startAccessingSecurityScopedResource()
        defer { if access { url.stopAccessingSecurityScopedResource() } }
        do {
            // Limit memory usage: this is a small structural probe, not an archive loader.
            let maxSize = 32 * 1024 * 1024
            let size = try url.resourceValues(forKeys: [.fileSizeKey]).fileSize
            guard let size, size > 0, size <= maxSize else {
                importedFileStatus = "Нужен файл размером до 32 МБ"
                return
            }
            let data = try Data(contentsOf: url)
            guard !data.isEmpty, data.count <= maxSize else {
                importedFileStatus = "Файл пустой или превышает 32 МБ"
                return
            }
            var count: UInt32 = 0
            var firstID: UInt32 = 0
            var firstSize: UInt32 = 0
            let (result, checksum) = data.withUnsafeBytes { raw -> (Int32, UInt32) in
                let pointer = raw.bindMemory(to: UInt8.self).baseAddress
                let parsed = st_chunk_scan(pointer, UInt32(raw.count),
                                           &count, &firstID, &firstSize)
                let crc = st_openxray_crc32(raw.baseAddress, UInt32(raw.count))
                return (parsed, crc)
            }
            let hexCRC = String(format: "%08X", checksum)
            if data.starts(with: [0x50, 0x4B, 0x03, 0x04]) {
                importedFileStatus = "\(url.lastPathComponent): ZIP/APK, CRC32 \(hexCRC). Это не простой контейнер чанков X-Ray."
            } else if result == 0 {
                importedFileStatus = "\(url.lastPathComponent): \(count) чанков, CRC32 \(hexCRC). Формат игры не подтверждён."
                let shown = min(Int(count), 5)
                for index in 0..<shown {
                    var id: UInt32 = 0
                    var offset: UInt32 = 0
                    var size: UInt32 = 0
                    var compressed: UInt32 = 0
                    let code = data.withUnsafeBytes { raw in
                        st_chunk_at(raw.bindMemory(to: UInt8.self).baseAddress,
                                    UInt32(raw.count), UInt32(index),
                                    &id, &offset, &size, &compressed)
                    }
                    if code != 0 {
                        chunkDetails = ["Ошибка чтения списка чанков"]
                        break
                    }
                    chunkDetails.append("#\(index + 1) ID=\(id) @\(offset) size=\(size) compressed=\(compressed == 1 ? "yes" : "no")")
                    if compressed == 0 && size >= 8 {
                        var nestedCount: UInt32 = 0
                        var nestedFirstID: UInt32 = 0
                        let nestedResult = data.withUnsafeBytes { raw in
                            st_chunk_scan_children(raw.bindMemory(to: UInt8.self).baseAddress,
                                                   UInt32(raw.count), UInt32(index),
                                                   &nestedCount, &nestedFirstID)
                        }
                        if nestedResult == 0 && nestedCount > 0 {
                            chunkDetails.append("  nested: \(nestedCount), first ID=\(nestedFirstID)")
                        }
                    }
                    // Only show a small preview of uncompressed payloads.
                    // The native parser has already validated these bounds.
                    if compressed == 0 && size > 0 {
                        let begin = Int(offset)
                        let previewSize = min(Int(size), 16)
                        if begin <= data.count && previewSize <= data.count - begin {
                            let preview = data[begin..<(begin + previewSize)]
                                .map { String(format: "%02X", $0) }
                                .joined(separator: " ")
                            chunkDetails.append("  HEX: \(preview)")
                        }
                    }
                }
                if count > 5 {
                    chunkDetails.append("Показаны первые 5 из \(count)")
                }
            } else {
                importedFileStatus = "\(url.lastPathComponent): CRC32 \(hexCRC). Не распознан как простой контейнер чанков X-Ray."
            }
        } catch {
            importedFileStatus = "Ошибка чтения файла: \(error.localizedDescription)"
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
                        engine.handle(input, pressed: true)
                    }
                    .onEnded { _ in
                        engine.handle(input, pressed: false)
                    }
            )
            .accessibilityLabel(title)
    }
}
