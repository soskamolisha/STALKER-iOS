#include "../Sources/Native/XRayBootstrap.hpp"
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

int main() {
    using namespace stalker;
    char temp[] = "/tmp/stalker-xray-XXXXXX";
    char* dir = mkdtemp(temp);
    assert(dir != nullptr);
    const std::string root(dir);
    XRayBootstrap engine;
    assert(engine.state() == EngineState::cold);
    assert(!engine.attachResources(root));
    assert(engine.initialize(GameEdition::pripyat));
    assert(engine.edition() == GameEdition::pripyat);
    assert(!engine.initialize(GameEdition::shadow));
    assert(!engine.attachResources(root));
    assert(!engine.attachResources("/definitely/not/a/game/folder"));
    assert(mkdir((root + "/gamedata").c_str(), 0700) == 0);
    assert(engine.attachResources(root));
    assert(engine.hasLooseGameData());
    assert(!engine.hasPackedGameData());
    assert(engine.state() == EngineState::resourcesReady);
    assert(!engine.attachResources(root));
    engine.shutdown();
    assert(engine.state() == EngineState::cold);
    assert(!engine.hasLooseGameData());
    assert(!engine.hasPackedGameData());
    assert(rmdir((root + "/gamedata").c_str()) == 0);
    std::ofstream archive(root + "/gamedata.db0", std::ios::binary);
    archive << "fixture";
    archive.close();
    assert(engine.initialize(GameEdition::shadow));
    assert(engine.attachResources(root));
    assert(!engine.hasLooseGameData());
    assert(engine.hasPackedGameData());
    engine.shutdown();
    assert(std::remove((root + "/gamedata.db0").c_str()) == 0);
    assert(rmdir(root.c_str()) == 0);
    std::cout << "X-Ray resource discovery lifecycle tests passed\n";
}
