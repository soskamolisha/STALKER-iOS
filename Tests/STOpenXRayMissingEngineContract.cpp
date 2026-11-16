// Contract regression: valid imported resources must NOT imply an engine exists.
// Compiled without ST_XRAY_STATIC_ENGINE_ADAPTER or a mock engine export.
#include "../Sources/Native/STOpenXRayRuntime.h"
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <unistd.h>

static void write_file(const std::string& path, const char* data) {
    FILE* f = std::fopen(path.c_str(), "wb");
    assert(f);
    assert(std::fwrite(data, 1, std::strlen(data), f) == std::strlen(data));
    assert(std::fclose(f) == 0);
}

int main() {
    char pattern[] = "/tmp/stalker-no-engine-XXXXXX";
    char* temp = mkdtemp(pattern);
    assert(temp);
    const std::string root(temp);
    write_file(root + "/fsgame.ltx", "[filesystem]\n");
    write_file(root + "/gamedata.db0", "non-zip fixture");
    assert(st_openxray_runtime_resources_linked() == 0);
    assert(st_openxray_runtime_preflight(root.c_str()) == -5);
    assert(st_openxray_runtime_run_with_resources("-soc", root.c_str()) == -5);
    assert(std::remove((root + "/fsgame.ltx").c_str()) == 0);
    assert(std::remove((root + "/gamedata.db0").c_str()) == 0);
    assert(rmdir(root.c_str()) == 0);
    std::puts("Missing-engine contract passed: resource import cannot fake gameplay.");
}
