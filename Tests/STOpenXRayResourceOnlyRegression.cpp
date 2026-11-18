#include "../Sources/Native/STOpenXRayRuntime.h"
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <unistd.h>

// Simulates a modern iOS engine adapter with NO legacy launch symbol.
extern "C" int st_xray_embedded_run_with_resources(const char* args, const char* root) {
    return args && root && std::strcmp(args, "-game") == 0 ? 23 : -99;
}
static void write_file(const std::string& path, const char* data) {
    FILE* f = std::fopen(path.c_str(), "wb");
    assert(f);
    assert(std::fwrite(data, 1, std::strlen(data), f) == std::strlen(data));
    assert(std::fclose(f) == 0);
}
int main() {
    assert(st_openxray_runtime_linked() == 0);
    assert(st_openxray_runtime_resources_linked() == 1);
    assert(st_openxray_runtime_run("-game") == -1);
    char pattern[] = "/tmp/xray-resource-only-XXXXXX";
    char* folder = mkdtemp(pattern);
    assert(folder);
    const std::string root(folder);
    write_file(root + "/fsgame.ltx", "[filesystem]");
    write_file(root + "/gamedata.db0", "not-a-zip");
    assert(st_openxray_runtime_preflight(root.c_str()) == 0);
    assert(st_openxray_runtime_run_with_resources("-game", root.c_str()) == 23);
    assert(unlink((root + "/gamedata.db0").c_str()) == 0);
    assert(unlink((root + "/fsgame.ltx").c_str()) == 0);
    assert(rmdir(root.c_str()) == 0);
    std::puts("Resource-only X-Ray adapter ABI test passed");
}
