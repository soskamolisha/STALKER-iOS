#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
// 1 only when the embedded OpenXRay entry symbol was linked into the app.
int32_t st_openxray_runtime_linked(void);
// Returns -1 if no runtime was linked, -2 for an invalid command line.
// Never call on the UIKit main thread: the upstream entry point is blocking.
int32_t st_openxray_runtime_run(const char* command_line);
#ifdef __cplusplus
}
#endif
