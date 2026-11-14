#include "STOpenXRayRuntime.h"
#include <atomic>
#include <cstring>
#include <string>
#include <cstddef>
#include <sys/stat.h>

// On Apple, resolve optional engine exports dynamically to avoid undefined
// symbols in the standalone iOS app. On Linux, preserve weak references so
// native tests can link their mock exports without -rdynamic.
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

extern "C" int32_t st_openxray_runtime_run_with_resources(const char* command_line,
                                                             const char* resource_root) {
    if (!command_line || !resource_root || !resource_root[0]) return -2;
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
    // Never silently call the old entry: it cannot receive the imported root.
    const auto run = embedded_run_with_resources();
    if (!run) return -5;
    if (runtime_active.test_and_set(std::memory_order_acquire)) return -3;
    struct ReleaseFlag {
        ~ReleaseFlag() { runtime_active.clear(std::memory_order_release); }
    } release;
    // The embedded engine needs a real filesystem config, not merely
    // gamedata archives. Validate again at the native boundary to cover
    // non-Swift callers and races after UI preflight.
    const std::string config_path = std::string(resource_root) + "/fsgame.ltx";
    struct stat config_info {};
    if (lstat(config_path.c_str(), &config_info) != 0 ||
        !S_ISREG(config_info.st_mode) || config_info.st_size <= 0) return -7;
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
