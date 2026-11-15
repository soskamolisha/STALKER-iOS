#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
// Portable CRC-32 diagnostic helper used by the launcher.
// It does not link xrCore or demonstrate a working X-Ray game engine.
uint32_t st_openxray_crc32(const void *bytes, uint32_t length);
#ifdef __cplusplus
}
#endif
