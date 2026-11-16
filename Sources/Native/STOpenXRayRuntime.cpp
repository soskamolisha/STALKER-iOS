#include "STOpenXRayRuntime.h"
#include <atomic>
#include <cstring>
#include <string>
#include <cstddef>
#include <cstdio>
#include <sys/stat.h>
#include <dirent.h>

// When a real static engine adapter is compiled into the target, use a strong
// link reference. dlsym cannot reliably discover dead-stripped static symbols.
#if defined(ST_XRAY_STATIC_ENGINE_ADAPTER)
// The resource-aware ABI is mandatory for a statically linked engine.
// The legacy no-resource entry is optional: an iOS adapter need not expose it.
extern "C" int st_xray_embedded_run(const char*) __attribute__((weak));
extern "C" int st_xray_embedded_run_with_resources(const char*, const char*);
using EmbeddedRun = int (*)(const char*);
using EmbeddedRunWithResources = int (*)(const char*, const char*);
static EmbeddedRun embedded_run() { return st_xray_embedded_run; }
static EmbeddedRunWithResources embedded_run_with_resources() { return &st_xray_embedded_run_with_resources; }
// Force the resource-aware adapter to remain linked when the static integration
// macro is enabled. The app cannot claim a resource engine without this symbol.
#else
// Resolve optional exports at runtime in the standalone iOS launcher.
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

#endif // ST_XRAY_STATIC_ENGINE_ADAPTER

extern "C" int32_t st_openxray_runtime_linked(void) {
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

// Scan imported loose resources recursively without following symlinked folders.
// Level data is commonly nested deeper than gamedata/levels/<level>/.
static bool has_loose_resources(const std::string& directory, unsigned depth = 0) {
    if (depth > 32) return false;
    struct stat info {};
    if (lstat(directory.c_str(), &info) != 0 || !S_ISDIR(info.st_mode)) return false;
    DIR* entries = opendir(directory.c_str());
    if (!entries) return false;
    bool found = false;
    while (auto* entry = readdir(entries)) {
        if (std::strcmp(entry->d_name, ".") == 0 ||
            std::strcmp(entry->d_name, "..") == 0) continue;
        const std::string path = directory + "/" + entry->d_name;
        struct stat child {};
        if (lstat(path.c_str(), &child) != 0) continue;
        if (S_ISREG(child.st_mode) && child.st_size > 0) { found = true; break; }
        if (S_ISDIR(child.st_mode) && has_loose_resources(path, depth + 1)) {
            found = true;
            break;
        }
    }
    closedir(entries);
    return found;
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
    bool has_game_data = has_loose_resources(std::string(resource_root) + "/gamedata");
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
                S_ISREG(archive_info.st_mode) && archive_info.st_size > 0) {
                // Match the Swift importer's ZIP-signature rejection.
                // A renamed ZIP is not a usable X-Ray base archive.
                FILE* candidate = std::fopen(archive.c_str(), "rb");
                if (!candidate) continue;
                unsigned char signature[4] {};
                const size_t bytes = std::fread(signature, 1, sizeof(signature), candidate);
                std::fclose(candidate);
                const bool zip = bytes == sizeof(signature) &&
                    signature[0] == 0x50 && signature[1] == 0x4b &&
                    ((signature[2] == 0x03 && signature[3] == 0x04) ||
                     (signature[2] == 0x05 && signature[3] == 0x06) ||
                     (signature[2] == 0x07 && signature[3] == 0x08));
                if (!zip && bytes > 0) has_game_data = true;
            }
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
