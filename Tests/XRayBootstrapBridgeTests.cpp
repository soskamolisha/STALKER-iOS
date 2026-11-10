#include "../Sources/Native/STXRayBootstrap.h"
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>
#include <sys/stat.h>
#include <cstring>
#include <unistd.h>

int main() {
    assert(st_xray_initialize(nullptr, 0) == -1);
    STXRayHandle handle = st_xray_create();
    assert(handle);
    assert(st_xray_state(handle) == 0);
    assert(st_xray_initialize(handle, 3) == -1);
    assert(st_xray_initialize(handle, 2) == 0);
    assert(st_xray_state(handle) == 1);
    char folder[] = "/tmp/xray-bridge-XXXXXX";
    char* path = mkdtemp(folder);
    assert(path);
    assert(st_xray_attach_resources(handle, path) == -2);
    const std::string archive = std::string(path) + "/gamedata.db0";
    { std::ofstream file(archive); file << "fixture"; }
    assert(st_xray_attach_resources(handle, path) == 0);
    assert(st_xray_state(handle) == 2);
    assert(st_xray_has_packed_data(handle) == 1);
    assert(st_xray_has_loose_data(handle) == 0);
    assert(st_xray_level_count(handle) == 0);
    assert(st_xray_level_chunk_count(handle, 0) == -1);
    assert(st_xray_level_name(handle, 0, nullptr, 0) == -1);
    st_xray_shutdown(handle);
    assert(st_xray_initialize(handle, 2) == 0);
    const std::string data = std::string(path) + "/gamedata";
    const std::string levels = data + "/levels";
    const std::string zaton = levels + "/zaton";
    assert(mkdir(data.c_str(), 0700) == 0);
    assert(mkdir(levels.c_str(), 0700) == 0);
    assert(mkdir(zaton.c_str(), 0700) == 0);
    {
        std::ofstream file(zaton + "/level", std::ios::binary);
        const unsigned char bytes[] = {7,0,0,0,2,0,0,0,11,12};
        file.write(reinterpret_cast<const char*>(bytes), sizeof(bytes));
    }
    assert(st_xray_attach_resources(handle, path) == 0);
    assert(st_xray_level_count(handle) == 1);
    char levelName[16] = {};
    assert(st_xray_level_name(handle, 0, levelName, sizeof(levelName)) == 0);
    assert(std::strcmp(levelName, "zaton") == 0);
    assert(st_xray_level_name(handle, 0, levelName, 2) == -3);
    assert(st_xray_level_chunk_count(handle, 0) == 1);
    st_xray_shutdown(handle);
    assert(st_xray_level_count(handle) == 0);
    assert(std::remove((zaton + "/level").c_str()) == 0);
    assert(rmdir(zaton.c_str()) == 0);
    assert(rmdir(levels.c_str()) == 0);
    assert(rmdir(data.c_str()) == 0);
    assert(st_xray_state(handle) == 0);
    st_xray_destroy(handle);
    assert(std::remove(archive.c_str()) == 0);
    assert(rmdir(path) == 0);
}
