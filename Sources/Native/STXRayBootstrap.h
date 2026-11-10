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
void st_xray_shutdown(STXRayHandle handle);
#ifdef __cplusplus
}
#endif
