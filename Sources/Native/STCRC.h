#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
// Calls the genuine OpenXRay xrCore crc32.cpp implementation.
// This proves a native engine utility can be invoked from Swift; not a game engine.
uint32_t st_openxray_crc32(const void *bytes, uint32_t length);
#ifdef __cplusplus
}
#endif
