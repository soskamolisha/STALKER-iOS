#include "XRayBootstrap.hpp"
#include "XRayLevelCatalog.hpp"
#include <dirent.h>
#include <sys/stat.h>

namespace stalker {
namespace {
bool isDirectory(const std::string& path) {
    struct stat info {};
    return stat(path.c_str(), &info) == 0 && S_ISDIR(info.st_mode);
}
bool isRegularFile(const std::string& path) {
    struct stat info {};
    return stat(path.c_str(), &info) == 0 && S_ISREG(info.st_mode);
}
bool hasPackedArchives(const std::string& root) {
    DIR* dir = opendir(root.c_str());
    if (!dir) return false;
    bool found = false;
    while (auto* entry = readdir(dir)) {
        const std::string name(entry->d_name);
        if (name.compare(0, 11, "gamedata.db") == 0 &&
            isRegularFile(root + "/" + name)) {
            found = true;
            break;
        }
    }
    closedir(dir);
    return found;
}
}

bool XRayBootstrap::initialize(GameEdition edition) {
    if (state_ != EngineState::cold) return false;
    edition_ = edition;
    state_ = EngineState::initialized;
    return true;
}

bool XRayBootstrap::attachResources(const std::string& resourceRoot) {
    if (state_ != EngineState::initialized || resourceRoot.empty()) return false;
    if (!isDirectory(resourceRoot)) return false;
    const bool loose = isDirectory(resourceRoot + "/gamedata");
    const bool packed = hasPackedArchives(resourceRoot);
    // A directory by itself is not a valid game resource installation.
    if (!loose && !packed) return false;
    resourceRoot_ = resourceRoot;
    hasLooseGameData_ = loose;
    hasPackedGameData_ = packed;
    looseLevels_ = loose ? discoverLooseLevels(resourceRoot) : std::vector<std::string>{};
    state_ = EngineState::resourcesReady;
    return true;
}

void XRayBootstrap::shutdown() {
    resourceRoot_.clear();
    hasLooseGameData_ = false;
    hasPackedGameData_ = false;
    looseLevels_.clear();
    state_ = EngineState::cold;
}
}
