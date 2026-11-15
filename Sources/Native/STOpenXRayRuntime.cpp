#include "STOpenXRayRuntime.h"
#include <atomic>
#include <cstring>
#include <string>
#include <cstddef>
#include <cstdio>
#include <sys/stat.h>
#include <dirent.h>

// When a real static engine adapter is compiled into the target, use a strong\n// link reference. dlsym cannot reliably discover dead-stripped static symbols.\n#if defined(ST_XRAY_STATIC_ENGINE_ADAPTER)\nextern "C" int st_xray_embedded_run(const char*);\nextern "C" int st_xray_embedded_run_with_resources(const char*, const char*);\nusing EmbeddedRun = int (*)(const char*);\nusing EmbeddedRunWithResources = int (*)(const char*, const char*);\nstatic EmbeddedRun embedded_run() { return &st_xray_embedded_run; }\nstatic EmbeddedRunWithResources embedded_run_with_resources() { return &st_xray_embedded_run_with_resources; }\n#else\n// Resolve optional exports at runtime in the standalone iOS launcher.
// The actual engine must export these C entry points when it is integrated.
#if defined(__APPLE__)
#include <dlfcn.h>
using EmbeddedRun = int (*)(const char*);
using EmbeddedRunWithResources = int (*)(const char*, const char*);
static EmbeddedRun embedded_run() {
    return reinterpret_cast<EmbeddedRun>(dlsym(RTLD_DEFAULT, "st_xray_embedded_run"));
}
static EmbeddedRunWithResources embedded_run_with_resources() {
    return reinterpret_cast<EmbeddedRunWithResources>(
        dlsym(RTLD_DEFAULT, "st_xray_embedded_run_with_resources"));
}
#else
extern "C" int st_xray_embedded_run(const char*) __attribute__((weak));
extern "C" int st_xray_embedded_run_with_resources(const char*, const char*) __attribute__((weak));
static auto embedded_run() -> int (*)(const char*) { return st_xray_embedded_run; }
static auto embedded_run_with_resources() -> int (*)(const char*, const char*) {
    return st_xray_embedded_run_with_resources;
}
#endif

#endif // ST_XRAY_STATIC_ENGINE_ADAPTER\n\nextern "C" int32_t st_openxray_runtime_linked(void) {
    return embedded_run() != nullptr ? 1 : 0;
}

static std::atomic_flag runtime_active = ATOMIC_FLAG_INIT;

extern "C" int32_t st_openxray_runtime_run(const char* command_line) {
    if (!command_line) return -2;
    const auto run = embedded_run();
    if (!run) return -1;
    // Upstream engine state is process-global: never run two instances at once.
    if (runtime_active.test_and_set(std::memory_order_acquire)) return -3;
    struct ReleaseFlag {
        ~ReleaseFlag() { runtime_active.clear(std::memory_order_release); }
    } release;
    // Never allow a C++ exception to unwind across the Swift-facing C ABI.
#if defined(__cpp_exceptions)
    try {
        return run(command_line);
    } catch (...) {
        return -4;
    }
#else
    return run(command_line);
#endif
}

extern "C" int32_t st_openxray_runtime_preflight(const char* resource_root) {
    if (!resource_root || !resource_root[0]) return -2;
    // The upstream resource-aware entry requires an absolute filesystem root.
    // Reject relative paths here instead of relying on upstream behavior.
    if (resource_root[0] != '/') return -6;
    // Upstream currently constructs an unquoted -fsltx argument.
    // Reject whitespace and quotes to prevent a broken config path.
    if (std::strpbrk(resource_root, " \t\r\n\"") != nullptr) return -6;
    // A trailing slash makes lstat follow a directory symlink on POSIX.
    // Reject it so callers cannot bypass the no-symlink root check.
    const size_t root_length = std::strlen(resource_root);
    if (root_length > 1 && resource_root[root_length - 1] == '/') return -6;
    struct stat root_info;
    if (lstat(resource_root, &root_info) != 0 || !S_ISDIR(root_info.st_mode)) return -6;
    // The sandbox may expose a directory that is not readable by the engine.
    DIR* root_probe = opendir(resource_root);
    if (!root_probe) return -6;
    closedir(root_probe);
    // Never silently call the old entry: it cannot receive the imported root.
    const std::string config_path = std::string(resource_root) + "/fsgame.ltx";
    struct stat config_info {};
    if (lstat(config_path.c_str(), &config_info) != 0 ||
        !S_ISREG(config_info.st_mode) || config_info.st_size <= 0) return -7;
    // Ensure the config is actually readable, not merely present in directory metadata.
    FILE* config = std::fopen(config_path.c_str(), "rb");
    if (!config) return -7;
    const int first_byte = std::fgetc(config);
    std::fclose(config);
    if (first_byte == EOF) return -7;
    // A config alone is not enough to boot Shadow of Chernobyl.
    // Accept either loose gamedata or a nonempty base gamedata.db0 archive.
    // Later numbered archives alone cannot form a bootable base resource set.
    bool has_game_data = false;
    const std::string loose_path = std::string(resource_root) + "/gamedata";
    struct stat loose_info {};
    if (lstat(loose_path.c_str(), &loose_info) == 0 && S_ISDIR(loose_info.st_mode)) {
        // An empty gamedata directory is not evidence of imported game files.
        DIR* loose = opendir(loose_path.c_str());
        if (loose) {
            while (struct dirent* item = readdir(loose)) {
                if (std::strcmp(item->d_name, ".") == 0 ||
                    std::strcmp(item->d_name, "..") == 0) continue;
                const std::string item_path = loose_path + "/" + item->d_name;
                struct stat item_info {};
                // Only actual nonempty files or nonempty directories count;
                // symlinks and empty placeholder files are not game data.
                if (lstat(item_path.c_str(), &item_info) != 0) continue;
                if (S_ISREG(item_info.st_mode) && item_info.st_size > 0) {
                    has_game_data = true;
                    break;
                }
                if (S_ISDIR(item_info.st_mode)) {
                    DIR* child = opendir(item_path.c_str());
                    if (child) {
                        while (struct dirent* nested = readdir(child)) {
                            if (std::strcmp(nested->d_name, ".") == 0 ||
                                std::strcmp(nested->d_name, "..") == 0) continue;
                            const std::string nested_path = item_path + "/" + nested->d_name;
                            struct stat nested_info {};
                            if (lstat(nested_path.c_str(), &nested_info) == 0 &&
                                S_ISREG(nested_info.st_mode) && nested_info.st_size > 0) {
                                has_game_data = true;
                                break;
                            }
                        }
                        closedir(child);
                    }
                    if (has_game_data) break;
                }
            }
            closedir(loose);
        }
    }
    DIR* dir = opendir(resource_root);
    if (dir) {
        while (!has_game_data) {
            struct dirent* entry = readdir(dir);
            if (!entry) break;
            // Match the same archive naming rule as the Swift importer.
            // Ignore unrelated files such as gamedata.db0.zip or backups.
            constexpr char prefix[] = "gamedata.db";
            constexpr size_t prefix_length = sizeof(prefix) - 1;
            if (std::strncmp(entry->d_name, prefix, prefix_length) != 0) continue;
            const char* suffix = entry->d_name + prefix_length;
            const size_t suffix_length = std::strlen(suffix);
            if (suffix_length < 1 || suffix_length > 4) continue;
            bool valid_suffix = true;
            unsigned int archive_index = 0;
            for (size_t i = 0; i < suffix_length; ++i) {
                const char digit = suffix[i];
                if (digit < '0' || digit > '9') {
                    valid_suffix = false;
                    break;
                }
                archive_index = archive_index * 10 + static_cast<unsigned int>(digit - '0');
            }
            if (!valid_suffix || archive_index != 0) continue;
            const std::string archive = std::string(resource_root) + "/" + entry->d_name;
            struct stat archive_info {};
            if (lstat(archive.c_str(), &archive_info) == 0 &&
                S_ISREG(archive_info.st_mode) && archive_info.st_size > 0)
                has_game_data = true;
        }
        closedir(dir);
    }
    if (!has_game_data) return -8;
    // Check resource validity before reporting a missing engine, so the
    // on-device diagnostics can identify incomplete imports independently.
    if (!embedded_run_with_resources()) return -5;
    return 0;
}

extern "C" int32_t st_openxray_runtime_run_with_resources(const char* command_line,
                                                             const char* resource_root) {
    if (!command_line) return -2;
    const int32_t readiness = st_openxray_runtime_preflight(resource_root);
    if (readiness != 0) return readiness;
    const auto run = embedded_run_with_resources();
    if (!run) return -5; // The optional entry may disappear between checks.
    if (runtime_active.test_and_set(std::memory_order_acquire)) return -3;
    struct ReleaseFlag {
        ~ReleaseFlag() { runtime_active.clear(std::memory_order_release); }
    } release;
    // The embedded engine needs a real filesystem config, not merely
    // gamedata archives. Validate again at the native boundary to cover
    // non-Swift callers and races after UI preflight.
#if defined(__cpp_exceptions)
    try {
        return run(command_line, resource_root);
    } catch (...) {
        return -4;
    }
#else
    return run(command_line, resource_root);
#endif
}

extern "C" int32_t st_openxray_runtime_resources_linked(void) {
    return embedded_run_with_resources() != nullptr ? 1 : 0;
}

extern "C" const char* st_openxray_runtime_status_message(int32_t status) {
    switch (status) {
        case 0: return "X-Ray runtime check passed (gameplay not yet verified)";
        case -1: return "X-Ray engine entry is not linked";
        case -2: return "Missing launch command or game folder";
        case -3: return "X-Ray engine is already running";
        case -4: return "X-Ray engine stopped with a native exception";
        case -5: return "Resource-aware X-Ray engine is not linked";
        case -6: return "Invalid or inaccessible game folder";
        case -7: return "Missing, empty, or invalid fsgame.ltx";
        case -8: return "Missing loose gamedata or nonempty gamedata.db0 base archive";
        default: return "X-Ray returned an unknown status";
    }
}
