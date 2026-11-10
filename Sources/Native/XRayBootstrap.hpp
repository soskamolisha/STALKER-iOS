#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace stalker {
enum class EngineState : std::uint8_t { cold, initialized, resourcesReady };
enum class GameEdition : std::uint8_t { shadow, clearSky, pripyat };

class XRayBootstrap {
public:
    bool initialize(GameEdition edition);
    bool attachResources(const std::string& resourceRoot);
    bool hasLooseGameData() const { return hasLooseGameData_; }
    bool hasPackedGameData() const { return hasPackedGameData_; }
    void shutdown();
    EngineState state() const { return state_; }
    GameEdition edition() const { return edition_; }
    const std::string& resourceRoot() const { return resourceRoot_; }
    const std::vector<std::string>& looseLevels() const { return looseLevels_; }
private:
    EngineState state_ = EngineState::cold;
    GameEdition edition_ = GameEdition::shadow;
    std::string resourceRoot_;
    bool hasLooseGameData_ = false;
    bool hasPackedGameData_ = false;
    std::vector<std::string> looseLevels_;
};
}
