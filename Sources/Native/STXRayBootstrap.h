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
// Reads at most 64 KiB from a recognized archive. Returns 0 on success.
int32_t st_xray_archive_read(STXRayHandle handle, uint32_t index, uint64_t offset,
                             void* destination, uint32_t size);
// Locate an X-Ray top-level chunk (8-byte LE id/size header), without decompressing it.
// Returns -2 when chunk_index is outside the archive; -3 for malformed data.
int32_t st_xray_archive_chunk_info(STXRayHandle handle, uint32_t archive_index,
                                   uint32_t chunk_index, uint32_t* chunk_id,
                                   uint64_t* payload_offset, uint32_t* payload_size,
                                   int32_t* compressed);
// Read a bounded range within a chunk payload. Compressed chunks remain raw bytes.
int32_t st_xray_archive_chunk_read(STXRayHandle handle, uint32_t archive_index,
                                   uint32_t chunk_index, uint32_t relative_offset,
                                   void* destination, uint32_t size);
// Decode X-Ray trivial encryption in place (0=worldwide, 1=Russian).
// Used for legacy encrypted directory payloads before LZO decoding.
int32_t st_xray_directory_decrypt(void* data, uint32_t size, int32_t key_variant);
// Validate decoded X-Ray directory records before exposing them to the launcher.
// Returns number of records, or -3 for malformed paths, offsets or sizes.
int32_t st_xray_validate_directory_records(const void* bytes, uint32_t length);
// Try plain, worldwide-key and Russian-key LZO directory decoding in that order.
// Returns key_used: -1 plain, 0 worldwide, 1 Russian.
int32_t st_xray_archive_directory_auto_decrypt(STXRayHandle handle,
                                                 uint32_t archive_index,
                                                 void* destination, uint32_t capacity,
                                                 uint32_t* actual_size, int32_t* key_used);
// Discover decompressed size of a plain LZO directory chunk by bounded attempts.
// Returns -4 when compressed data is invalid or requires decryption.
int32_t st_xray_archive_directory_auto_decompress(STXRayHandle handle,
                                                    uint32_t archive_index,
                                                    void* destination, uint32_t capacity,
                                                    uint32_t* actual_size);
// Decode a compressed directory chunk (ID 1) with an explicit expected size.
// This supports plain LZO streams only; encrypted X-Ray catalogs need a key.
int32_t st_xray_archive_directory_decompress(STXRayHandle handle, uint32_t archive_index,
                                               void* destination, uint32_t capacity,
                                               uint32_t expected_size);
// Count records in a plain or LZO-compressed X-Ray directory without extracting files.
// Compressed directories use bounded decoding with key autodetection.
int32_t st_xray_archive_directory_count(STXRayHandle handle, uint32_t archive_index);
// Decode an LZO1X payload into an exactly sized caller-provided buffer.
// Returns -4 for corrupt compressed data, -3 for invalid output size.
int32_t st_xray_lzo_decompress(const void* source, uint32_t source_size,
                               void* destination, uint32_t destination_capacity,
                               uint32_t expected_size);
// Parse one directory record from plain or LZO-compressed chunk 1.
int32_t st_xray_archive_directory_entry(STXRayHandle handle, uint32_t archive_index,
                                        uint32_t entry_index, char* name, uint32_t name_capacity,
                                        uint32_t* size_real, uint32_t* size_compressed,
                                        uint32_t* crc, uint32_t* data_offset);
// Read bytes of an uncompressed directory entry's file from its absolute archive offset.
// Returns -4 if file content is compressed and needs the X-Ray codec.
// Find a file by its exact relative path in one archive directory.
// Returns the entry index, or -2 if absent. The path must be normalized
// with forward slashes; no absolute paths or parent traversal.
int32_t st_xray_archive_find_file(STXRayHandle handle, uint32_t archive_index,
                                   const char* relative_path);
// Locate an asset across imported game archives, searching later archives
// first so expansion packs can override earlier resources.
// Returns 0 on success, -2 when absent, or another negative error.
int32_t st_xray_find_resource(STXRayHandle handle, const char* relative_path,
                              uint32_t* archive_index, uint32_t* entry_index);
// Load a named packed resource into caller-owned memory (up to 64 MiB).
// Set actual_size to the required decoded size; -3 means buffer too small
// or resource exceeds the safety limit. Does not handle loose files yet.
int32_t st_xray_load_packed_resource(STXRayHandle handle, const char* relative_path,
                                     void* destination, uint32_t capacity,
                                     uint32_t* actual_size);
// Query packed resource size without allocating or decoding its file body.
// Later archives take priority; returns -2 for a missing resource.
int32_t st_xray_packed_resource_info(STXRayHandle handle, const char* relative_path,
                                     uint32_t* decoded_size, uint32_t* stored_size);
// Probe packed level geometry and collision file availability without loading either.
// Level names must be single safe path components. Returns 0 if either exists,
// -2 if both are absent; missing members have size zero.
int32_t st_xray_packed_level_info(STXRayHandle handle, const char* level_name,
                                  uint32_t* geometry_size, uint32_t* collision_size);
// Load one packed level member: 0=geometry (level), 1=collision (level.cform).
// Uses the same 64 MiB limit and atomic caller-buffer guarantees as resource loading.
int32_t st_xray_load_packed_level_file(STXRayHandle handle, const char* level_name,
                                        int32_t member, void* destination,
                                        uint32_t capacity, uint32_t* actual_size);
// Load and validate a packed level's geometry chunk table (max 64 MiB).
// Returns the chunk count, or a negative loader/parser error.
int32_t st_xray_packed_level_chunk_count(STXRayHandle handle, const char* level_name);
// Return metadata for one geometry chunk from a packed level.
// Offset is relative to decoded level bytes, not the archive file.
int32_t st_xray_packed_level_chunk_info(STXRayHandle handle, const char* level_name,
                                         uint32_t index, uint32_t* chunk_id,
                                         uint32_t* payload_offset, uint32_t* payload_size,
                                         int32_t* compressed);
// Copy one decoded level chunk's raw payload (compressed chunks stay compressed).
// Required size is reported even when capacity is insufficient. Maximum 64 MiB.
int32_t st_xray_packed_level_chunk_read(STXRayHandle handle, const char* level_name,
                                         uint32_t index, void* destination,
                                         uint32_t capacity, uint32_t* actual_size);
// Load and validate a packed collision mesh header and payload bounds.
int32_t st_xray_packed_level_collision_info(STXRayHandle handle, const char* level_name,
                                            uint32_t* vertices, uint32_t* triangles,
                                            float* minimum_xyz, float* maximum_xyz);
// Inspect an already decoded X-Ray geometry chunk stream in memory.
// Chunk payload offsets are relative to the supplied byte buffer.
int32_t st_xray_memory_chunk_count(const void* bytes, uint32_t size);
int32_t st_xray_memory_chunk_info(const void* bytes, uint32_t size,
                                  uint32_t index, uint32_t* chunk_id,
                                  uint32_t* payload_offset, uint32_t* payload_size,
                                  int32_t* compressed);
// Validate a decoded 36-byte collision header and its vertex/triangle payload.
int32_t st_xray_memory_cform_info(const void* bytes, uint32_t size,
                                  uint32_t* vertices, uint32_t* triangles,
                                  float* minimum_xyz, float* maximum_xyz);
int32_t st_xray_archive_file_read(STXRayHandle handle, uint32_t archive_index,
                                  uint32_t entry_index, uint32_t relative_offset,
                                  void* destination, uint32_t size);
// Decode a compressed file entry from a validated plain or decoded archive directory.
// Caller supplies the full uncompressed output buffer. Maximum 64 MiB.
int32_t st_xray_archive_file_decompress(STXRayHandle handle, uint32_t archive_index,
                                         uint32_t entry_index, void* destination,
                                         uint32_t destination_capacity);
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
