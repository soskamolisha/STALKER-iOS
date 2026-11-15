#include "../../Sources/Native/STOpenXRayRuntime.h"
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
namespace fs = std::filesystem;
static void expect(int got, int want, const char* label) {
    if (got != want) {
        std::fprintf(stderr, "%s: expected %d, got %d (%s)\n", label, want, got,
                     st_openxray_runtime_status_message(got));
        std::exit(1);
    }
}
int main() {
    expect(st_openxray_runtime_run(nullptr), -2, "null command");
    expect(st_openxray_runtime_run_with_resources(nullptr, "/tmp"), -2, "null resource command");
    expect(st_openxray_runtime_preflight(nullptr), -2, "null root");
    expect(st_openxray_runtime_preflight("relative"), -6, "relative root");
    expect(st_openxray_runtime_preflight("/does-not-exist-stalker"), -6, "missing root");
    const auto base = fs::temp_directory_path() / ("stalker-native-test-" + std::to_string(std::rand()));
    fs::create_directories(base);
    const std::string root = base.string();
    expect(st_openxray_runtime_preflight(root.c_str()), -7, "missing config");
    { std::ofstream config(base / "fsgame.ltx"); config << "$game_data$ = true| true| $fs_root$| gamedata\\n"; }
    expect(st_openxray_runtime_preflight(root.c_str()), -8, "missing game data");
    { std::ofstream zipped(base / "gamedata.db0.zip"); zipped << "zip is not an engine archive"; }
    expect(st_openxray_runtime_preflight(root.c_str()), -8, "zip must not count as archive");
    fs::remove(base / "gamedata.db0.zip");
    { std::ofstream later(base / "gamedata.db1"); later << "later archive"; }
    expect(st_openxray_runtime_preflight(root.c_str()), -8, "missing base archive");
    fs::remove(base / "gamedata.db1");
    { std::ofstream empty(base / "gamedata.db0"); }
    expect(st_openxray_runtime_preflight(root.c_str()), -8, "empty base archive");
    fs::remove(base / "gamedata.db0");
    { std::ofstream data(base / "gamedata.db0", std::ios::binary); data << "placeholder"; }
    expect(st_openxray_runtime_preflight(root.c_str()), -5, "archive present but engine missing");
    expect(st_openxray_runtime_resources_linked(), 0, "unlinked resources");
    fs::remove(base / "gamedata.db0");
    fs::create_directories(base / "gamedata");
    { std::ofstream data(base / "gamedata" / "test.ltx"); data << "test"; }
    expect(st_openxray_runtime_preflight(root.c_str()), -5, "loose resources but engine missing");
    fs::remove_all(base);
    std::puts("PASS: native X-Ray bridge validation (no embedded engine)");
}
