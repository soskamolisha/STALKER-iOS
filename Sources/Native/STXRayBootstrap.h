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
int32_t st_xray_archive_count(STXRayHandle handle);
uint64_t st_xray_archive_bytes(STXRayHandle handle);
// Copies an archive filename with NUL terminator; -2 index out of range, -3 buffer too small.
int32_t st_xray_archive_name(STXRayHandle handle, uint32_t index, char* destination, uint32_t capacity);
int32_t st_xray_level_count(STXRayHandle handle);
// Copies a UTF-8 level name including its NUL terminator; returns -1 for invalid input,
// -2 for out-of-range index, -3 when destination capacity is insufficient.
int32_t st_xray_level_name(STXRayHandle handle, uint32_t index, char* destination, uint32_t capacity);
// Returns chunk count for an unpacked level's geometry, or -1 on invalid input,
// -2 when the level is missing or its chunk stream is malformed.
int32_t st_xray_level_chunk_count(STXRayHandle handle, uint32_t level_index);
// Returns 0 for a valid indexed chunk; -1 for invalid input, -2 for missing geometry.
int32_t st_xray_level_chunk_info(STXRayHandle handle, uint32_t level_index,
                                 uint32_t chunk_index, uint32_t* chunk_id,
                                 uint64_t* payload_offset, uint32_t* payload_size,
                                 int32_t* compressed);
// Read collision mesh counts and bounds without copying the full mesh into Swift.
int32_t st_xray_level_collision_info(STXRayHandle handle, uint32_t level_index,
                                      uint32_t* vertex_count, uint32_t* triangle_count,
                                      float* bounds_min_xyz, float* bounds_max_xyz);
void st_xray_shutdown(STXRayHandle handle);
#ifdef __cplusplus
}
#endif
