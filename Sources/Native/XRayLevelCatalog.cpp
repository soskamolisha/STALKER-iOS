#include "XRayLevelCatalog.hpp"
#include <algorithm>
#include <dirent.h>
#include <sys/stat.h>

namespace stalker {
std::vector<std::string> discoverLooseLevels(const std::string& gameRoot) {
    std::vector<std::string> result;
    if (gameRoot.empty()) return result;
    const std::string path = gameRoot + "/gamedata/levels";
    DIR* dir = opendir(path.c_str());
    if (!dir) return result;
    while (auto* entry = readdir(dir)) {
        const std::string name(entry->d_name);
        if (name == "." || name == ".." || name.empty() || name[0] == '.') continue;
        const std::string fullPath = path + "/" + name;
        struct stat info {};
        // lstat avoids following symlinked folders outside imported resources.
        if (lstat(fullPath.c_str(), &info) == 0 && S_ISDIR(info.st_mode))
            result.push_back(name);
        if (result.size() >= 4096) break;
    }
    closedir(dir);
    std::sort(result.begin(), result.end());
    return result;
}
}
