#include "../Sources/Native/STOpenXRayRuntime.h"
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

static void write_file(const std::string& path, const unsigned char* data, size_t size) {
    FILE* file = std::fopen(path.c_str(), "wb");
    assert(file);
    assert(std::fwrite(data, 1, size, file) == size);
    assert(std::fclose(file) == 0);
}

int main() {
    char temporary[] = "/tmp/stalker-preflight-XXXXXX";
    char* folder = mkdtemp(temporary);
    assert(folder);
    const std::string root(folder);
    const std::string config = root + "/fsgame.ltx";
    const std::string archive = root + "/gamedata.db0";
    const unsigned char config_data[] = "[filesystem]";
    write_file(config, config_data, sizeof(config_data) - 1);
    assert(st_openxray_runtime_preflight(root.c_str()) == -8);
    const unsigned char zip[] = {0x50, 0x4b, 0x03, 0x04};
    write_file(archive, zip, sizeof(zip));
    assert(st_openxray_runtime_preflight(root.c_str()) == -8);
    const unsigned char sample[] = {1, 2, 3, 4};
    write_file(archive, sample, sizeof(sample));
    // No engine is linked in this standalone test, even with an archive present.
    assert(st_openxray_runtime_preflight(root.c_str()) == -5);
    assert(st_openxray_runtime_resources_linked() == 0);
    assert(unlink(archive.c_str()) == 0);
    assert(unlink(config.c_str()) == 0);
    assert(rmdir(root.c_str()) == 0);
    std::puts("Native X-Ray preflight smoke tests passed");
}
