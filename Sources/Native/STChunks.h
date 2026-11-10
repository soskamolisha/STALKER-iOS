#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
// Structural X-Ray chunk framing only; does not decompress or load game assets.
// Returns 0 on success, -1 on invalid input, -2 on too many chunks.
int32_t st_chunk_scan(const uint8_t *bytes, uint32_t length,
                      uint32_t *count, uint32_t *first_id, uint32_t *first_size);
// Locate the Nth top-level chunk without allocating payload memory.
// Returns 0 if found, -1 for malformed input, -2 for excessive chunks, -3 if absent.
int32_t st_chunk_at(const uint8_t *bytes, uint32_t length, uint32_t index,
                    uint32_t *id, uint32_t *payload_offset, uint32_t *payload_size,
                    uint32_t *compressed);
#ifdef __cplusplus
}
#endif
