#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// Experimental OpenXRay C ABI.
// The implementation is currently injected into upstream xr_3da/entry_point.cpp
// during the engine CI build. It is NOT linked into the SwiftUI launcher.
// Calling this on iOS requires a validated UIKit/SDL lifecycle and renderer.
int st_xray_embedded_run(const char *command_line);

#ifdef __cplusplus
}
#endif
