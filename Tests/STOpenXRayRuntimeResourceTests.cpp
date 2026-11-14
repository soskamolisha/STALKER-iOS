#include "../Sources/Native/STOpenXRayRuntime.h"
#include <cassert>
#include <cstring>
#include <string>
#include <cstdio>
#include <stdexcept>
#include <unistd.h>
#include <cstdlib>

// Mock only: verifies resource-root forwarding and the shared runtime guard.
// This does not build or run the real OpenXRay engine.
static int nested_result = 0;
static int cross_entry_result = 0;
extern "C" int st_xray_embedded_run(const char*) {
    cross_entry_result = st_openxray_runtime_run_with_resources("", "/tmp");
    return 23;
}
static std::string test_root;
static const char* expected_root = nullptr;
extern "C" int st_xray_embedded_run_with_resources(const char* args, const char* root) {
    assert(root && std::strcmp(root, expected_root) == 0);
    if (std::strcmp(args, "-nested") == 0) {
        nested_result = st_openxray_runtime_run_with_resources("", root);
        return 19;
    }
    if (std::strcmp(args, "-throw") == 0) throw std::runtime_error("mock exception");
    return 11;
}

int main() {
    char root_template[] = "/tmp/stalker-xray-config-XXXXXX";
    char* created_root = mkdtemp(root_template);
    assert(created_root != nullptr);
    test_root = created_root;
    expected_root = test_root.c_str();
    assert(st_openxray_runtime_run_with_resources("", expected_root) == -7);
    const std::string config_path = test_root + "/fsgame.ltx";
    FILE* config = std::fopen(config_path.c_str(), "wb");
    assert(config != nullptr);
    assert(std::fputs("$app_data_root$ = true| false| $fs_root$| appdata\\\\\n", config) >= 0);
    assert(std::fclose(config) == 0);
    // Empty and symlinked configs must never reach the native engine.
    assert(std::remove(config_path.c_str()) == 0);
    config = std::fopen(config_path.c_str(), "wb");
    assert(config != nullptr);
    assert(std::fclose(config) == 0);
    assert(st_openxray_runtime_run_with_resources("", expected_root) == -7);
    assert(std::remove(config_path.c_str()) == 0);
    assert(symlink("/etc/hosts", config_path.c_str()) == 0);
    assert(st_openxray_runtime_run_with_resources("", expected_root) == -7);
    assert(unlink(config_path.c_str()) == 0);
    config = std::fopen(config_path.c_str(), "wb");
    assert(config != nullptr);
    assert(std::fputs("$app_data_root$ = true| false| $fs_root$| appdata\\\\\n", config) >= 0);
    assert(std::fclose(config) == 0);
    assert(st_openxray_runtime_resources_linked() == 1);
    assert(st_openxray_runtime_run_with_resources(nullptr, expected_root) == -2);
    assert(st_openxray_runtime_run_with_resources("", nullptr) == -2);
    assert(st_openxray_runtime_run_with_resources("", "") == -2);
    assert(st_openxray_runtime_run_with_resources("", "/this-directory-must-not-exist-stalker") == -6);
    assert(st_openxray_runtime_run_with_resources("", ".") == -6);
    assert(st_openxray_runtime_run_with_resources("", "relative/game") == -6);
    char temporary[] = "/tmp/stalker-resource-link-XXXXXX";
    const int fd = mkstemp(temporary);
    assert(fd >= 0);
    assert(close(fd) == 0);
    assert(unlink(temporary) == 0);
    assert(symlink("/tmp", temporary) == 0);
    assert(st_openxray_runtime_run_with_resources("", temporary) == -6);
    const std::string slash_link = std::string(temporary) + "/";
    assert(st_openxray_runtime_run_with_resources("", slash_link.c_str()) == -6);
    assert(st_openxray_runtime_run_with_resources("", "/tmp/") == -6);
    assert(unlink(temporary) == 0);
    assert(st_openxray_runtime_run("-legacy") == 23);
    assert(cross_entry_result == -3);
    assert(st_openxray_runtime_run_with_resources("-nested", expected_root) == 19);
    assert(nested_result == -3);
    assert(st_openxray_runtime_run_with_resources("-throw", expected_root) == -4);
    assert(st_openxray_runtime_run_with_resources("", expected_root) == 11);
    assert(std::remove(config_path.c_str()) == 0);
    assert(rmdir(expected_root) == 0);
}
