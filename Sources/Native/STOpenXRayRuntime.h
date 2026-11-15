#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
// 1 only when the embedded OpenXRay entry symbol was linked into the app.
int32_t st_openxray_runtime_linked(void);
// True only when the embedded engine supports an explicit resource root.
int32_t st_openxray_runtime_resources_linked(void);
// Returns -1 if no runtime was linked, -2 for an invalid command line.
// Never call on the UIKit main thread: the upstream entry point is blocking.
int32_t st_openxray_runtime_run(const char* command_line);
// Non-invasive launch readiness probe: 0 means an embedded engine entry is present;
// -5 means no resource-aware engine is linked; -6 means invalid resource root;
// -7 means the imported root has no usable fsgame.ltx. Does not start gameplay.
int32_t st_openxray_runtime_preflight(const char* resource_root);
// Stable diagnostic text for SwiftUI launch status; never returns a null pointer.
const char* st_openxray_runtime_status_message(int32_t status);
// Resource-aware embedded ABI must be implemented by the actual engine.
// Returns -5 if the engine does not export this integration entry.
int32_t st_openxray_runtime_run_with_resources(const char* command_line, const char* resource_root);
#ifdef __cplusplus
}
#endif
