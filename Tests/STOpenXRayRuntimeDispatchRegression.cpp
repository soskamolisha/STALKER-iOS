#include "../Sources/Native/STOpenXRayRuntime.h"
#include <cassert>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

static std::atomic<int> calls{0};
static std::string received_root;
static std::string received_args;

// Mock the exact resource-aware engine ABI, not the real game engine.
extern "C" int st_xray_embedded_run_with_resources(const char* args, const char* root) {
    ++calls;
    received_args = args ? args : "";
    received_root = root ? root : "";
    return 42;
}
extern "C" int st_xray_embedded_run(const char*) { return 7; }

static void write_file(const std::string& path, const char* bytes) {
    FILE* file = std::fopen(path.c_str(), "wb");
    assert(file);
    assert(std::fwrite(bytes, 1, std::strlen(bytes), file) == std::strlen(bytes));
    assert(std::fclose(file) == 0);
}

int main() {
    assert(st_openxray_runtime_linked() == 1);
    assert(st_openxray_runtime_resources_linked() == 1);
    assert(st_openxray_runtime_run_with_resources("", "/not-a-real-game") == -6);
    assert(calls == 0);
    char temporary[] = "/tmp/stalker-runtime-XXXXXX";
    char* folder = mkdtemp(temporary);
    assert(folder);
    const std::string root(folder);
    write_file(root + "/fsgame.ltx", "[filesystem]");
    write_file(root + "/gamedata.db0", "not-a-zip");
    assert(st_openxray_runtime_preflight(root.c_str()) == 0);
    assert(st_openxray_runtime_run_with_resources("-start", root.c_str()) == 42);
    assert(calls == 1);
    assert(received_root == root);
    assert(received_args == "-start");
    assert(st_openxray_runtime_run("plain") == 7);
    assert(unlink((root + "/gamedata.db0").c_str()) == 0);
    assert(unlink((root + "/fsgame.ltx").c_str()) == 0);
    assert(rmdir(root.c_str()) == 0);
    std::puts("Native X-Ray runtime dispatch regression passed (mock adapter only)");
}
