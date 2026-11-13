#include "STOpenXRayRuntime.h"

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

extern "C" int32_t st_openxray_runtime_run(const char* command_line) {
    if (!command_line) return -2;
#if defined(__GNUC__) || defined(__clang__)
    if (!st_xray_embedded_run) return -1;
    return st_xray_embedded_run(command_line);
#else
    return -1;
#endif
}
