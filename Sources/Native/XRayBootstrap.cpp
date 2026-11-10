#include "XRayBootstrap.hpp"
#include "XRayLevelCatalog.hpp"
#include <dirent.h>
#include <cctype>
#include <sys/stat.h>

namespace stalker {
namespace {
bool isDirectory(const std::string& path) {
    struct stat info {};
    return stat(path.c_str(), &info) == 0 && S_ISDIR(info.st_mode);
}
bool isXRayArchiveName(const std::string& name) {
    const std::string prefix = "gamedata.db";
    if (name.compare(0, prefix.size(), prefix) != 0) return false;
    const std::string suffix = name.substr(prefix.size());
    if (suffix.empty() || suffix.size() > 2) return false;
    for (unsigned char c : suffix) {
        if (!std::isalnum(c) || c > 127) return false;
    }
    return true;
}
// Inspect loose resources without following directory symlinks.
bool hasNonemptyLooseFile(const std::string& directory, int depth = 0) {
    if (depth > 32) return false;
    DIR* dir = opendir(directory.c_str());
    if (!dir) return false;
    bool found = false;
    while (auto* entry = readdir(dir)) {
        const std::string name(entry->d_name);
        if (name == "." || name == "..") continue;
        const std::string path = directory + "/" + name;
        struct stat info {};
        if (lstat(path.c_str(), &info) != 0) continue;
        if (S_ISREG(info.st_mode) && info.st_size > 0) { found = true; break; }
        if (S_ISDIR(info.st_mode) && hasNonemptyLooseFile(path, depth + 1)) {
            found = true;
            break;
        }
    }
    closedir(dir);
    return found;
}
struct ArchiveInventory { std::uint32_t count = 0; std::uint64_t bytes = 0; };
ArchiveInventory inspectPackedArchives(const std::string& root) {
    DIR* dir = opendir(root.c_str());
    if (!dir) return {};
    ArchiveInventory result;
    while (auto* entry = readdir(dir)) {
        const std::string name(entry->d_name);
        if (!isXRayArchiveName(name)) continue;
        const std::string path = root + "/" + name;
        struct stat info {};
        if (lstat(path.c_str(), &info) != 0 || !S_ISREG(info.st_mode) || info.st_size <= 0) continue;
        ++result.count;
        result.bytes += static_cast<std::uint64_t>(info.st_size);
    }
    closedir(dir);
    return result;
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
    const bool loose = isDirectory(resourceRoot + "/gamedata") &&
                       hasNonemptyLooseFile(resourceRoot + "/gamedata");
    const ArchiveInventory archives = inspectPackedArchives(resourceRoot);
    const bool packed = archives.count > 0;
    // A directory by itself is not a valid game resource installation.
    if (!loose && !packed) return false;
    resourceRoot_ = resourceRoot;
    hasLooseGameData_ = loose;
    hasPackedGameData_ = packed;
    packedArchiveCount_ = archives.count;
    packedArchiveBytes_ = archives.bytes;
    looseLevels_ = loose ? discoverLooseLevels(resourceRoot) : std::vector<std::string>{};
    state_ = EngineState::resourcesReady;
    return true;
}

void XRayBootstrap::shutdown() {
    resourceRoot_.clear();
    hasLooseGameData_ = false;
    hasPackedGameData_ = false;
    packedArchiveCount_ = 0;
    packedArchiveBytes_ = 0;
    looseLevels_.clear();
    state_ = EngineState::cold;
}
}
