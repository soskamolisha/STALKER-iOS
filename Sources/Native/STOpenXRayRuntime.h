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
// The iOS SDL adapter requires the main thread for its blocking video loop.
int32_t st_openxray_runtime_run(const char* command_line);
// Non-invasive launch readiness probe: 0 means an embedded engine entry is present;
// -5 means no resource-aware engine is linked; -6 means invalid resource root;
// -7 means the imported root has no usable fsgame.ltx;
// -8 means no nonempty gamedata.db0 or loose gamedata directory was found;
// Does not start gameplay and does not validate archive contents.
int32_t st_openxray_runtime_preflight(const char* resource_root);
// Stable diagnostic text for SwiftUI launch status; never returns a null pointer.
const char* st_openxray_runtime_status_message(int32_t status);
// Resource-aware embedded ABI must be implemented by the actual engine.
// Returns -5 if the engine does not export this integration entry.
// The iOS SDL adapter returns -10 when called from a background thread.
int32_t st_openxray_runtime_run_with_resources(const char* command_line, const char* resource_root);
#ifdef __cplusplus
}
#endif
