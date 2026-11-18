#pragma once
#include <cstdint>
#include <string>

namespace stalker {
struct LevelFiles {
    bool hasLevel = false;
    bool hasLevelLtx = false;
    bool hasGeometry = false;
    bool hasCollision = false;
    std::uint64_t geometryBytes = 0;
    std::uint64_t collisionBytes = 0;
};
// Inspects loose files only. This does not decode geometry or verify level format.
LevelFiles inspectLooseLevel(const std::string& gameRoot, const std::string& levelName);
}
