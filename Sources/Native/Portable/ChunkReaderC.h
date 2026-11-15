#pragma once
// C-compatible boundary for the portable chunk reader.
// This is a diagnostic interface, NOT a .db0 archive parser.
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint32_t id;
    uint32_t payload_size;
    size_t payload_offset;
    int compressed;
} StalkerChunkInfo;

typedef enum {
    STALKER_CHUNK_OK = 0,
    STALKER_CHUNK_END = 1,
    STALKER_CHUNK_TRUNCATED = 2,
    STALKER_CHUNK_OUT_OF_BOUNDS = 3,
    STALKER_CHUNK_INVALID_ARGUMENT = 4
} StalkerChunkResult;

// Reads a candidate chunk at *offset and advances on success only.
// Caller owns data; no allocation or decompression occurs.
StalkerChunkResult stalker_next_chunk(const uint8_t* data,
                                      size_t length,
                                      size_t* offset,
                                      StalkerChunkInfo* result);

#ifdef __cplusplus
}
#endif
