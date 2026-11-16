// Portable integration contract test: no game data or Apple SDK required.
// Exercises the same C ABI used by the Swift launcher with a fake engine.
#include "../Sources/Native/STOpenXRayRuntime.h"
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

static int calls = 0;
extern "C" int st_xray_embedded_run_with_resources(const char* args, const char* root) {
    ++calls;
    assert(std::strcmp(args, "-soc -nointro -nosplash") == 0);
    assert(root && root[0] == '/');
    return 42;
}

static void write_file(const std::string& path, const char* content) {
    FILE* file = std::fopen(path.c_str(), "wb");
    assert(file);
    assert(std::fwrite(content, 1, std::strlen(content), file) == std::strlen(content));
    assert(std::fclose(file) == 0);
}

int main() {
    char path[] = "/tmp/stalker-xray-contract-XXXXXX";
    char* created = mkdtemp(path);
    assert(created);
    const std::string root(created);
    assert(st_openxray_runtime_resources_linked() == 1);
    assert(st_openxray_runtime_preflight(root.c_str()) == -7);
    write_file(root + "/fsgame.ltx", "[filesystem]\n");
    assert(st_openxray_runtime_preflight(root.c_str()) == -8);
    write_file(root + "/gamedata.db0", "PK\003\004fakezip");
    assert(st_openxray_runtime_preflight(root.c_str()) == -8);
    assert(std::remove((root + "/gamedata.db0").c_str()) == 0);
    write_file(root + "/gamedata.db0", "XRay archive placeholder");
    assert(st_openxray_runtime_preflight(root.c_str()) == 0);
    assert(st_openxray_runtime_run_with_resources("-soc -nointro -nosplash", root.c_str()) == 42);
    assert(calls == 1);
    assert(std::remove((root + "/gamedata.db0").c_str()) == 0);
    assert(std::remove((root + "/fsgame.ltx").c_str()) == 0);
    assert(rmdir(root.c_str()) == 0);
    std::puts("X-Ray resource-aware C ABI contract passed (mock engine only).");
}
