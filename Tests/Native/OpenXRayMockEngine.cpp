#include "../../Sources/Native/STOpenXRayRuntime.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
namespace fs = std::filesystem;
static int run_calls = 0;
static int resource_calls = 0;
extern "C" int st_xray_embedded_run(const char* command) {
    ++run_calls;
    return std::strcmp(command, "-test") == 0 ? 42 : 43;
}
extern "C" int st_xray_embedded_run_with_resources(const char* command, const char* root) {
    ++resource_calls;
    if (std::strcmp(command, "-test") != 0 || !fs::exists(fs::path(root) / "fsgame.ltx"))
        return 44;
    return 45;
}
static void check(bool value, const char* label) {
    if (!value) { std::fprintf(stderr, "FAIL: %s\n", label); std::exit(1); }
}
int main() {
    check(st_openxray_runtime_linked() == 1, "legacy entry linked");
    check(st_openxray_runtime_resources_linked() == 1, "resource entry linked");
    check(st_openxray_runtime_run("-test") == 42, "legacy return forwarded");
    check(run_calls == 1, "legacy called exactly once");
    const auto root = fs::temp_directory_path() / ("stalker-mock-" + std::to_string(std::rand()));
    fs::create_directories(root);
    { std::ofstream config(root / "fsgame.ltx"); config << "config"; }
    { std::ofstream archive(root / "gamedata.db0"); archive << "placeholder"; }
    const std::string path = root.string();
    check(st_openxray_runtime_preflight(path.c_str()) == 0, "linked runtime passes preflight");
    check(st_openxray_runtime_run_with_resources("-test", path.c_str()) == 45,
          "resource-aware return forwarded");
    check(resource_calls == 1, "resource-aware called exactly once");
    fs::remove_all(root);
    std::puts("PASS: linked mock engine ABI forwarding (not a real game engine)");
}
