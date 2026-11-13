#include "STOpenXRayRuntime.h"
#include <atomic>

// Weak linkage allows the launcher to build without the optional upstream
// OpenXRay archive. This bridge does not imply iOS runtime compatibility.
#if defined(__GNUC__) || defined(__clang__)
extern "C" int st_xray_embedded_run(const char*) __attribute__((weak));
#else
extern "C" int st_xray_embedded_run(const char*);
#endif

extern "C" int32_t st_openxray_runtime_linked(void) {
#if defined(__GNUC__) || defined(__clang__)
    return st_xray_embedded_run != nullptr ? 1 : 0;
#else
    return 0;
#endif
}

static std::atomic_flag runtime_active = ATOMIC_FLAG_INIT;

extern "C" int32_t st_openxray_runtime_run(const char* command_line) {
    if (!command_line) return -2;
#if defined(__GNUC__) || defined(__clang__)
    if (!st_xray_embedded_run) return -1;
    // Upstream engine state is process-global: never run two instances at once.
    if (runtime_active.test_and_set(std::memory_order_acquire)) return -3;
    struct ReleaseFlag {
        ~ReleaseFlag() { runtime_active.clear(std::memory_order_release); }
    } release;
    // Never allow a C++ exception to unwind across the Swift-facing C ABI.
#if defined(__cpp_exceptions)
    try {
        return st_xray_embedded_run(command_line);
    } catch (...) {
        return -4;
    }
#else
    return st_xray_embedded_run(command_line);
#endif
#else
    return -1;
#endif
}
