#include "../Sources/Native/STOpenXRayRuntime.h"
#include <cassert>
#include <cstring>
#include <string>
#include <cstdio>
#include <stdexcept>
#include <unistd.h>
#include <cstdlib>
#include <sys/stat.h>

// Mock only: verifies resource-root forwarding and the shared runtime guard.
// This does not build or run the real OpenXRay engine.
static int nested_result = 0;
static int cross_entry_result = 0;
static std::string test_root;
static const char* expected_root = nullptr;
extern "C" int st_xray_embedded_run(const char*) {
    cross_entry_result = st_openxray_runtime_run_with_resources("", expected_root);
    return 23;
}
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
    assert(st_openxray_runtime_preflight(expected_root) == -7);
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
    // A directory named fsgame.ltx is not a usable filesystem config.
    assert(mkdir(config_path.c_str(), 0700) == 0);
    assert(st_openxray_runtime_run_with_resources("", expected_root) == -7);
    assert(rmdir(config_path.c_str()) == 0);
    config = std::fopen(config_path.c_str(), "wb");
    assert(config != nullptr);
    assert(std::fputs("$app_data_root$ = true| false| $fs_root$| appdata\\\\\n", config) >= 0);
    assert(std::fclose(config) == 0);
    assert(st_openxray_runtime_resources_linked() == 1);
    assert(st_openxray_runtime_preflight(expected_root) == -8);
    // A ZIP upload or backup is not a directly readable X-Ray archive.
    const std::string zip_path = test_root + "/gamedata.db0.zip";
    FILE* zip = std::fopen(zip_path.c_str(), "wb");
    assert(zip != nullptr);
    assert(std::fputs("zip placeholder", zip) >= 0);
    assert(std::fclose(zip) == 0);
    assert(st_openxray_runtime_preflight(expected_root) == -8);
    assert(std::remove(zip_path.c_str()) == 0);
    const std::string loose_path = test_root + "/gamedata";
    assert(mkdir(loose_path.c_str(), 0700) == 0);
    assert(st_openxray_runtime_preflight(expected_root) == -8);
    const std::string empty_file = loose_path + "/placeholder";
    FILE* placeholder = std::fopen(empty_file.c_str(), "wb");
    assert(placeholder != nullptr);
    assert(std::fclose(placeholder) == 0);
    assert(st_openxray_runtime_preflight(expected_root) == -8);
    assert(std::remove(empty_file.c_str()) == 0);
    assert(symlink("/etc/hosts", empty_file.c_str()) == 0);
    assert(st_openxray_runtime_preflight(expected_root) == -8);
    assert(unlink(empty_file.c_str()) == 0);
    const std::string nested_dir = loose_path + "/levels";
    assert(mkdir(nested_dir.c_str(), 0700) == 0);
    assert(st_openxray_runtime_preflight(expected_root) == -8);
    const std::string nested_empty = nested_dir + "/placeholder";
    FILE* nested = std::fopen(nested_empty.c_str(), "wb");
    assert(nested != nullptr);
    assert(std::fclose(nested) == 0);
    assert(st_openxray_runtime_preflight(expected_root) == -8);
    assert(std::remove(nested_empty.c_str()) == 0);
    assert(symlink("/etc/hosts", nested_empty.c_str()) == 0);
    assert(st_openxray_runtime_preflight(expected_root) == -8);
    assert(unlink(nested_empty.c_str()) == 0);
    FILE* nested_content = std::fopen(nested_empty.c_str(), "wb");
    assert(nested_content != nullptr);
    assert(std::fputs("mock nested level", nested_content) >= 0);
    assert(std::fclose(nested_content) == 0);
    assert(st_openxray_runtime_preflight(expected_root) == 0);
    assert(std::remove(nested_empty.c_str()) == 0);
    assert(st_openxray_runtime_preflight(expected_root) == -8);
    // Real level resources can be nested several directories deep.
    const std::string deeper = nested_dir + "/l01_escape";
    const std::string deepest = deeper + "/textures";
    assert(mkdir(deeper.c_str(), 0700) == 0);
    assert(mkdir(deepest.c_str(), 0700) == 0);
    const std::string deep_file = deepest + "/level.bin";
    FILE* deep_content = std::fopen(deep_file.c_str(), "wb");
    assert(deep_content != nullptr);
    assert(std::fputs("mock level", deep_content) >= 0);
    assert(std::fclose(deep_content) == 0);
    assert(st_openxray_runtime_preflight(expected_root) == 0);
    assert(std::remove(deep_file.c_str()) == 0);
    assert(st_openxray_runtime_preflight(expected_root) == -8);
    assert(rmdir(deepest.c_str()) == 0);
    assert(rmdir(deeper.c_str()) == 0);
    assert(rmdir(nested_dir.c_str()) == 0);
    // A nonempty regular file inside a loose gamedata directory is
    // sufficient for resource discovery (not proof of valid game data).
    const std::string loose_file = loose_path + "/test.ltx";
    FILE* loose_content = std::fopen(loose_file.c_str(), "wb");
    assert(loose_content != nullptr);
    assert(std::fputs("mock data", loose_content) >= 0);
    assert(std::fclose(loose_content) == 0);
    assert(st_openxray_runtime_preflight(expected_root) == 0);
    assert(std::remove(loose_file.c_str()) == 0);
    assert(st_openxray_runtime_preflight(expected_root) == -8);
    assert(rmdir(loose_path.c_str()) == 0);
    // Archive names alone do not establish resource availability.
    const std::string empty_archive = test_root + "/gamedata.db1";
    FILE* zero_archive = std::fopen(empty_archive.c_str(), "wb");
    assert(zero_archive != nullptr);
    assert(std::fclose(zero_archive) == 0);
    assert(st_openxray_runtime_preflight(expected_root) == -8);
    assert(std::remove(empty_archive.c_str()) == 0);
    assert(symlink("/etc/hosts", empty_archive.c_str()) == 0);
    assert(st_openxray_runtime_preflight(expected_root) == -8);
    assert(unlink(empty_archive.c_str()) == 0);
    const std::string invalid_archive = test_root + "/gamedata.db0.backup";
    FILE* backup = std::fopen(invalid_archive.c_str(), "wb");
    assert(backup != nullptr);
    assert(std::fputs("not an archive", backup) >= 0);
    assert(std::fclose(backup) == 0);
    assert(st_openxray_runtime_preflight(expected_root) == -8);
    assert(std::remove(invalid_archive.c_str()) == 0);
    const std::string archive_path = test_root + "/gamedata.db0";
    FILE* archive = std::fopen(archive_path.c_str(), "wb");
    assert(archive != nullptr);
    assert(std::fputs("mock archive placeholder", archive) >= 0);
    assert(std::fclose(archive) == 0);
    assert(st_openxray_runtime_preflight(expected_root) == 0);
    assert(st_openxray_runtime_preflight("relative/game") == -6);
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
    assert(std::remove(archive_path.c_str()) == 0);
    assert(rmdir(expected_root) == 0);
}
