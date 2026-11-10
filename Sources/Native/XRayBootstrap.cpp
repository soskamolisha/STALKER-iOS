#include "XRayBootstrap.hpp"
#include <sys/stat.h>

namespace stalker {
bool XRayBootstrap::initialize(GameEdition edition) {
    if (state_ != EngineState::cold) return false;
    edition_ = edition;
    state_ = EngineState::initialized;
    return true;
}

bool XRayBootstrap::attachResources(const std::string& resourceRoot) {
    if (state_ != EngineState::initialized || resourceRoot.empty()) return false;
    struct stat info {};
    if (stat(resourceRoot.c_str(), &info) != 0 || !S_ISDIR(info.st_mode)) return false;
    resourceRoot_ = resourceRoot;
    state_ = EngineState::resourcesReady;
    return true;
}

void XRayBootstrap::shutdown() {
    resourceRoot_.clear();
    state_ = EngineState::cold;
}
}
