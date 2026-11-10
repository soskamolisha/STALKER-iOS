#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef void* STXRayHandle;
STXRayHandle st_xray_create(void);
void st_xray_destroy(STXRayHandle handle);
int32_t st_xray_initialize(STXRayHandle handle, int32_t edition);
int32_t st_xray_attach_resources(STXRayHandle handle, const char* utf8_path);
int32_t st_xray_state(STXRayHandle handle);
int32_t st_xray_has_loose_data(STXRayHandle handle);
int32_t st_xray_has_packed_data(STXRayHandle handle);
int32_t st_xray_level_count(STXRayHandle handle);
// Copies a UTF-8 level name including its NUL terminator; returns -1 for invalid input,
// -2 for out-of-range index, -3 when destination capacity is insufficient.
int32_t st_xray_level_name(STXRayHandle handle, uint32_t index, char* destination, uint32_t capacity);
void st_xray_shutdown(STXRayHandle handle);
#ifdef __cplusplus
}
#endif
