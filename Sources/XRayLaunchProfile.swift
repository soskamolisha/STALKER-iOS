import Foundation

/// Observed command-line configuration from an iOS X-Ray port.
/// These are data-only launch profiles; they do not start a game engine.
enum XRayLaunchProfile {
    static func arguments(for gameID: String) -> [String]? {
        switch gameID {
        case "shadow": return ["-soc", "-nointro", "-nosplash"]
        case "clearSky": return nil // Unverified for selected engine source
        case "pripyat": return nil // Unverified for selected engine source
        default: return nil
        }
    }

    /// Matches the conventional fsgame.ltx directory layout.
    /// Never allow an untrusted profile to escape its imported game root.
    static func resourceURLs(for root: URL) -> [String: URL] {
        let base = root.standardizedFileURL
        let gameData = base.appendingPathComponent("gamedata", isDirectory: true)
        let appData = base.appendingPathComponent("appdata", isDirectory: true)
        return [
            "$fs_root$": base,
            "$game_data$": gameData,
            "$game_levels$": gameData.appendingPathComponent("levels", isDirectory: true),
            "$game_textures$": gameData.appendingPathComponent("textures", isDirectory: true),
            "$game_sounds$": gameData.appendingPathComponent("sounds", isDirectory: true),
            "$game_config$": gameData.appendingPathComponent("config", isDirectory: true),
            "$game_scripts$": gameData.appendingPathComponent("scripts", isDirectory: true),
            "$app_data_root$": appData,
            "$game_saves$": appData.appendingPathComponent("savedgames", isDirectory: true),
            "$logs$": appData.appendingPathComponent("logs", isDirectory: true)
        ]
    }
}
