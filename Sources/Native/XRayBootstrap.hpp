#pragma once
#include <cstdint>
#include <string>

namespace stalker {
enum class EngineState : std::uint8_t { cold, initialized, resourcesReady };
enum class GameEdition : std::uint8_t { shadow, clearSky, pripyat };

class XRayBootstrap {
public:
    bool initialize(GameEdition edition);
    bool attachResources(const std::string& resourceRoot);
    void shutdown();
    EngineState state() const { return state_; }
    GameEdition edition() const { return edition_; }
    const std::string& resourceRoot() const { return resourceRoot_; }
private:
    EngineState state_ = EngineState::cold;
    GameEdition edition_ = GameEdition::shadow;
    std::string resourceRoot_;
};
}
