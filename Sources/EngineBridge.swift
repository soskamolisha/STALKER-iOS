import Foundation

/// Contract for a future native X-Ray iOS engine integration.
/// The starter deliberately does not pretend to run Android binaries.
protocol GameEngine {
    var isAvailable: Bool { get }
    func start() throws
    func stop()
    func handle(_ input: GameInput)
}

enum GameInput {
    case moveLeft, moveForward, moveRight, aim, fire
}

enum EngineStartError: LocalizedError {
    case notPorted
    var errorDescription: String? {
        switch self {
        case .notPorted: return "Движок X-Ray пока не портирован на iOS"
        }
    }
}

final class UnavailableEngine: GameEngine {
    let isAvailable = false
    func start() throws { throw EngineStartError.notPorted }
    func stop() {}
    func handle(_ input: GameInput) {}
}
