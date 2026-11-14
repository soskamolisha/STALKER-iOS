#include "../Sources/Native/XRayLevelInspection.hpp"
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

int main() {
    char folder[] = "/tmp/xray-level-inspect-XXXXXX";
    char* dir = mkdtemp(folder);
    assert(dir);
    const std::string root(dir);
    assert(mkdir((root + "/gamedata").c_str(), 0700) == 0);
    assert(mkdir((root + "/gamedata/levels").c_str(), 0700) == 0);
    const std::string level = root + "/gamedata/levels/zaton";
    assert(mkdir(level.c_str(), 0700) == 0);
    {
        std::ofstream out(level + "/level", std::ios::binary);
        const unsigned char bytes[] = {5,0,0,0,2,0,0,0,7,8};
        out.write(reinterpret_cast<const char*>(bytes), sizeof(bytes));
    }
    const auto inspection = stalker::inspectLevel(root, "zaton");
    assert(inspection.files.hasGeometry && inspection.files.geometryBytes == 10);
    assert(inspection.geometryIndexed && inspection.geometryChunks.size() == 1);
    assert(inspection.geometryChunks[0].id == 5);
    assert(!stalker::inspectLevel(root, "../zaton").geometryIndexed);
    assert(std::remove((level + "/level").c_str()) == 0);
    assert(rmdir(level.c_str()) == 0);
    assert(rmdir((root + "/gamedata/levels").c_str()) == 0);
    assert(rmdir((root + "/gamedata").c_str()) == 0);
    assert(rmdir(root.c_str()) == 0);
}
