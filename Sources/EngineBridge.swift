import Foundation

/// Boundary between the Swift UI and a future native X-Ray iOS port.
/// This repository does not yet contain the actual X-Ray engine.
protocol GameEngine {
    var isAvailable: Bool { get }
    func start() throws
    func stop()
    func handle(_ input: GameInput, pressed: Bool)
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
    func handle(_ input: GameInput, pressed: Bool) {}
}
