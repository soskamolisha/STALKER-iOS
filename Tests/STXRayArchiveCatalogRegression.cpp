#include "../Sources/Native/XRayBootstrap.hpp"
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

static void put(const std::string& path, const std::vector<unsigned char>& data) {
    FILE* f = std::fopen(path.c_str(), "wb");
    assert(f);
    assert(std::fwrite(data.data(), 1, data.size(), f) == data.size());
    assert(std::fclose(f) == 0);
}
int main() {
    char pattern[] = "/tmp/stalker-catalog-XXXXXX";
    char* directory = mkdtemp(pattern);
    assert(directory);
    const std::string root(directory);
    put(root + "/gamedata.db10", {1, 2, 3, 4});
    put(root + "/gamedata.db2", {5, 6, 7, 8});
    put(root + "/gamedata.db0", {9});
    put(root + "/gamedata.dbAB", {1, 2, 3, 4});
    put(root + "/gamedata.db1", {0x50, 0x4b, 0x03, 0x04});
    stalker::XRayBootstrap engine;
    assert(engine.initialize(stalker::GameEdition::shadow));
    assert(engine.attachResources(root));
    const std::vector<std::string> expected = {
        "gamedata.db0", "gamedata.db2", "gamedata.db10"
    };
    assert(engine.packedArchiveNames() == expected);
    assert(engine.packedArchiveCount() == expected.size());
    assert(engine.packedArchiveBytes() == 9);
    engine.shutdown();
    for (const auto& name : {"gamedata.db10", "gamedata.db2", "gamedata.db0",
                              "gamedata.dbAB", "gamedata.db1"})
        assert(unlink((root + "/" + name).c_str()) == 0);
    assert(rmdir(root.c_str()) == 0);
    std::puts("X-Ray archive catalog regression passed");
}
