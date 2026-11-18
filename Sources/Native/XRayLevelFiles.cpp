#include "XRayLevelFiles.hpp"
#include <sys/stat.h>

namespace stalker {
namespace {
bool safeLevelName(const std::string& name) {
    if (name.empty() || name == "." || name == ".." || name[0] == '.') return false;
    if (name.find('/') != std::string::npos || name.find('\\') != std::string::npos) return false;
    return true;
}
bool regularFile(const std::string& path, std::uint64_t* size = nullptr) {
    struct stat info {};
    if (lstat(path.c_str(), &info) != 0 || !S_ISREG(info.st_mode)) return false;
    if (size) *size = static_cast<std::uint64_t>(info.st_size);
    return true;
}
}
LevelFiles inspectLooseLevel(const std::string& gameRoot, const std::string& levelName) {
    LevelFiles result;
    if (gameRoot.empty() || !safeLevelName(levelName)) return result;
    const std::string base = gameRoot + "/gamedata/levels/" + levelName;
    struct stat info {};
    if (lstat(base.c_str(), &info) != 0 || !S_ISDIR(info.st_mode)) return result;
    result.hasLevel = true;
    result.hasLevelLtx = regularFile(base + "/level.ltx");
    result.hasGeometry = regularFile(base + "/level", &result.geometryBytes);
    result.hasCollision = regularFile(base + "/level.cform", &result.collisionBytes);
    return result;
}
}
