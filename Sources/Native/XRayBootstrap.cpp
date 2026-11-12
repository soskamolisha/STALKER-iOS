#include "XRayBootstrap.hpp"
#include "XRayLevelCatalog.hpp"
#include <dirent.h>
#include <cctype>
#include <algorithm>
#include <limits>
#include <sys/stat.h>

namespace stalker {
namespace {
bool isDirectory(const std::string& path) {
    // Do not follow a symlink at the imported game root.
    struct stat info {};
    return lstat(path.c_str(), &info) == 0 && S_ISDIR(info.st_mode);
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
    // Prevent opening a symlink if a nested directory changes during traversal.
    struct stat directoryInfo {};
    if (lstat(directory.c_str(), &directoryInfo) != 0 ||
        !S_ISDIR(directoryInfo.st_mode)) return false;
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
struct ArchiveInventory { std::uint32_t count = 0; std::uint64_t bytes = 0; std::vector<std::string> names; };
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
        if (result.count == std::numeric_limits<std::uint32_t>::max()) break;
        const auto size = static_cast<std::uint64_t>(info.st_size);
        if (size > std::numeric_limits<std::uint64_t>::max() - result.bytes) break;
        ++result.count;
        result.bytes += size;
        result.names.push_back(name);
    }
    closedir(dir);
    std::sort(result.names.begin(), result.names.end());
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
    // A trailing slash can make lstat follow a symlink to a directory.
    if (resourceRoot.size() > 1 && resourceRoot.back() == '/') return false;
    // Avoid scanning external paths through a symlinked game-data root.
    struct stat looseRootInfo {};
    const std::string looseRoot = resourceRoot + "/gamedata";
    const bool looseRootIsDirectory = lstat(looseRoot.c_str(), &looseRootInfo) == 0 &&
                                      S_ISDIR(looseRootInfo.st_mode);
    const bool loose = looseRootIsDirectory && hasNonemptyLooseFile(looseRoot);
    const ArchiveInventory archives = inspectPackedArchives(resourceRoot);
    const bool packed = archives.count > 0;
    // A directory by itself is not a valid game resource installation.
    if (!loose && !packed) return false;
    resourceRoot_ = resourceRoot;
    hasLooseGameData_ = loose;
    hasPackedGameData_ = packed;
    packedArchiveCount_ = archives.count;
    packedArchiveBytes_ = archives.bytes;
    packedArchiveNames_ = archives.names;
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
    packedArchiveNames_.clear();
    looseLevels_.clear();
    state_ = EngineState::cold;
}
}
