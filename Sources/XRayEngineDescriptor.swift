import Foundation

/// A truthful boundary between the launcher and a future native X-Ray engine.
/// This adapter never reports a game as playable without an actual linked runtime.
enum XRayEngineAvailability: Equatable {
    case unavailable(reason: String)
    case ready
}

struct XRayEngineDescriptor {
    let gameID: String
    let upstreamURL: URL
    let verifiedArguments: [String]?
    let availability: XRayEngineAvailability

    static func forGame(_ id: String) -> XRayEngineDescriptor? {
        let upstream: String
        let flags: [String]?
        switch id {
        case "shadow":
            upstream = "https://github.com/OpenXRay/xray"
            flags = ["-soc", "-nointro", "-nosplash"]
        case "clearSky":
            upstream = "https://github.com/OpenXRay/xray-15"
            flags = nil
        case "pripyat":
            upstream = "https://github.com/OpenXRay/xray-16"
            flags = nil
        default:
            return nil
        }
        return XRayEngineDescriptor(
            gameID: id,
            upstreamURL: URL(string: upstream)!,
            verifiedArguments: flags,
            availability: .unavailable(reason: "Native X-Ray runtime is not linked to this iOS build")
        )
    }
}
