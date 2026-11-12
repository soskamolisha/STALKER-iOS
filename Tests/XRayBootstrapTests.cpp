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
    // Imported root symlinks must be rejected even when the target has game data.
    const std::string rootLink = root + "-link";
    assert(symlink(root.c_str(), rootLink.c_str()) == 0);
    assert(!engine.attachResources(rootLink));
    // A trailing slash must not bypass the symlink-root check.
    assert(!engine.attachResources(rootLink + "/"));
    // Ordinary directory paths with a trailing slash are also rejected.
    assert(!engine.attachResources(root + "/"));
    assert(std::remove(rootLink.c_str()) == 0);
    // A symlink to external data must not be treated as a loose game installation.
    assert(symlink("/tmp", (root + "/gamedata").c_str()) == 0);
    assert(!engine.attachResources(root));
    assert(std::remove((root + "/gamedata").c_str()) == 0);
    assert(mkdir((root + "/gamedata").c_str(), 0700) == 0);
    assert(!engine.attachResources(root));
    // A nested symlink must not count external files as imported loose data.
    assert(symlink("/tmp", (root + "/gamedata/external").c_str()) == 0);
    assert(!engine.attachResources(root));
    assert(std::remove((root + "/gamedata/external").c_str()) == 0);
    { std::ofstream file(root + "/gamedata/fixture.ltx"); file << "fixture"; }
    assert(engine.attachResources(root));
    assert(engine.hasLooseGameData());
    assert(engine.looseLevels().empty());
    assert(!engine.hasPackedGameData());
    assert(engine.state() == EngineState::resourcesReady);
    assert(!engine.attachResources(root));
    engine.shutdown();
    assert(engine.state() == EngineState::cold);
    assert(!engine.hasLooseGameData());
    assert(!engine.hasPackedGameData());
    assert(engine.looseLevels().empty());
    assert(std::remove((root + "/gamedata/fixture.ltx").c_str()) == 0);
    assert(rmdir((root + "/gamedata").c_str()) == 0);
    { std::ofstream fake(root + "/gamedata.db.backup"); fake << "backup"; }
    { std::ofstream empty(root + "/gamedata.db1"); }
    assert(engine.initialize(GameEdition::shadow));
    assert(!engine.attachResources(root));
    engine.shutdown();
    // Archive symlinks must not import data outside the selected game folder.
    const std::string outsideArchive = root + "-external-archive";
    { std::ofstream external(outsideArchive, std::ios::binary); external << "external"; }
    assert(symlink(outsideArchive.c_str(), (root + "/gamedata.db0").c_str()) == 0);
    assert(engine.initialize(GameEdition::shadow));
    assert(!engine.attachResources(root));
    engine.shutdown();
    assert(std::remove((root + "/gamedata.db0").c_str()) == 0);
    assert(std::remove(outsideArchive.c_str()) == 0);
    std::ofstream archive(root + "/gamedata.db0", std::ios::binary);
    archive << "fixture";
    archive.close();
    assert(engine.initialize(GameEdition::shadow));
    assert(engine.attachResources(root));
    assert(!engine.hasLooseGameData());
    assert(engine.hasPackedGameData());
    assert(engine.packedArchiveCount() == 1);
    assert(engine.packedArchiveBytes() == 7);
    // Count a second valid archive, but ignore empty and backup-like files.
    { std::ofstream second(root + "/gamedata.dba", std::ios::binary); second << "data"; }
    engine.shutdown();
    assert(engine.initialize(GameEdition::shadow));
    assert(engine.attachResources(root));
    assert(engine.packedArchiveCount() == 2);
    assert(engine.packedArchiveBytes() == 11);
    engine.shutdown();
    assert(engine.packedArchiveCount() == 0);
    assert(engine.packedArchiveBytes() == 0);
    assert(std::remove((root + "/gamedata.db0").c_str()) == 0);
    assert(std::remove((root + "/gamedata.dba").c_str()) == 0);
    assert(std::remove((root + "/gamedata.db1").c_str()) == 0);
    assert(std::remove((root + "/gamedata.db.backup").c_str()) == 0);
    assert(rmdir(root.c_str()) == 0);
    std::cout << "X-Ray resource discovery lifecycle tests passed\n";
}
