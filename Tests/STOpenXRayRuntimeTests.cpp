#include "../Sources/Native/STOpenXRayRuntime.h"
#include <cassert>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

// Launch preflight without a linked OpenXRay engine: never claim gameplay started.
int main() {
    assert(std::strstr(st_openxray_runtime_status_message(-5), "not linked") != nullptr);
    assert(std::strstr(st_openxray_runtime_status_message(-7), "fsgame.ltx") != nullptr);
    assert(std::strstr(st_openxray_runtime_status_message(0), "not yet verified") != nullptr);
    assert(st_openxray_runtime_status_message(12345) != nullptr);
    assert(st_openxray_runtime_linked() == 0);
    assert(st_openxray_runtime_resources_linked() == 0);
    assert(st_openxray_runtime_run(nullptr) == -2);
    assert(st_openxray_runtime_run("") == -1);
    assert(st_openxray_runtime_run_with_resources(nullptr, "/tmp") == -2);
    assert(st_openxray_runtime_run_with_resources("", "relative") == -6);

    char root_template[] = "/tmp/stalker-shadow-of-chernobyl-XXXXXX";
    char* root = mkdtemp(root_template);
    assert(root != nullptr);
    const std::string config_path = std::string(root) + "/fsgame.ltx";
    // Even a real imported directory cannot start without the engine entry.
    assert(st_openxray_runtime_run_with_resources("-start server(single/alife/new)", root) == -5);
    FILE* config = std::fopen(config_path.c_str(), "wb");
    assert(config != nullptr);
    assert(std::fputs("$app_data_root$ = true| false| $fs_root$| appdata\\\\\n", config) >= 0);
    assert(std::fclose(config) == 0);
    assert(st_openxray_runtime_run_with_resources("-start server(single/alife/new)", root) == -5);
    assert(std::remove(config_path.c_str()) == 0);
    assert(rmdir(root) == 0);
}
