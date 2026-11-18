#include "../Sources/Native/XRayLevelFiles.hpp"
#include <cassert>
#include <cstdlib>
#include <fstream>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

int main() {
    char folder[] = "/tmp/xray-level-files-XXXXXX";
    char* dir = mkdtemp(folder);
    assert(dir);
    const std::string root(dir);
    assert(!stalker::inspectLooseLevel(root, "../outside").hasLevel);
    assert(!stalker::inspectLooseLevel(root, "zaton").hasLevel);
    assert(mkdir((root + "/gamedata").c_str(), 0700) == 0);
    assert(mkdir((root + "/gamedata/levels").c_str(), 0700) == 0);
    const std::string level = root + "/gamedata/levels/zaton";
    assert(mkdir(level.c_str(), 0700) == 0);
    { std::ofstream out(level + "/level", std::ios::binary); out << "fixture"; }
    { std::ofstream out(level + "/level.cform", std::ios::binary); out << "collision"; }
    auto files = stalker::inspectLooseLevel(root, "zaton");
    assert(files.hasLevel && !files.hasLevelLtx);
    assert(files.hasGeometry && files.geometryBytes == 7);
    assert(files.hasCollision && files.collisionBytes == 9);
    assert(!stalker::inspectLooseLevel(root, "../../zaton").hasLevel);
    assert(std::remove((level + "/level").c_str()) == 0);
    assert(std::remove((level + "/level.cform").c_str()) == 0);
    assert(rmdir(level.c_str()) == 0);
    assert(rmdir((root + "/gamedata/levels").c_str()) == 0);
    assert(rmdir((root + "/gamedata").c_str()) == 0);
    assert(rmdir(root.c_str()) == 0);
}
