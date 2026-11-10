#include "../Sources/Native/XRayLevelCatalog.hpp"
#include <cassert>
#include <cstdlib>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

int main() {
    char folder[] = "/tmp/xray-levels-XXXXXX";
    char* root = mkdtemp(folder);
    assert(root);
    const std::string base(root);
    assert(stalker::discoverLooseLevels(base).empty());
    assert(mkdir((base + "/gamedata").c_str(), 0700) == 0);
    assert(mkdir((base + "/gamedata/levels").c_str(), 0700) == 0);
    assert(mkdir((base + "/gamedata/levels/zaton").c_str(), 0700) == 0);
    assert(mkdir((base + "/gamedata/levels/jupiter").c_str(), 0700) == 0);
    assert(symlink("/tmp", (base + "/gamedata/levels/outside").c_str()) == 0);
    const auto levels = stalker::discoverLooseLevels(base);
    assert(levels.size() == 2);
    assert(levels[0] == "jupiter");
    assert(levels[1] == "zaton");
    assert(unlink((base + "/gamedata/levels/outside").c_str()) == 0);
    assert(rmdir((base + "/gamedata/levels/zaton").c_str()) == 0);
    assert(rmdir((base + "/gamedata/levels/jupiter").c_str()) == 0);
    assert(rmdir((base + "/gamedata/levels").c_str()) == 0);
    assert(rmdir((base + "/gamedata").c_str()) == 0);
    assert(rmdir(base.c_str()) == 0);
}
