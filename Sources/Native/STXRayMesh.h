#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
// Reads bounded collision vertices and triangle indices into caller-owned buffers.
// Returns 0 on success, -1 for invalid arguments, -2 for invalid/unavailable mesh,
// -3 when the mesh exceeds caller capacity.
int32_t st_xray_read_collision_mesh(const char* utf8_path,
                                    float* xyz, uint32_t vertex_capacity,
                                    uint32_t* indices, uint32_t triangle_capacity,
                                    uint32_t* vertex_count, uint32_t* triangle_count);
// Returns a bounded triangle subset for large maps, remapping referenced vertices.
int32_t st_xray_read_collision_sample(const char* utf8_path,
                                      float* xyz, uint32_t vertex_capacity,
                                      uint32_t* indices, uint32_t triangle_capacity,
                                      uint32_t* vertex_count, uint32_t* triangle_count);
#ifdef __cplusplus
}
#endif
