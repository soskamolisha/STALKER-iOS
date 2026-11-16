#pragma once
#include "STChunks.h"
#ifdef __cplusplus
extern "C" {
#endif

// Returns 1 when a real LZO decoder is compiled in, otherwise 0.
int32_t st_lzo1x_available(void);

// Adapter for LZO1X payloads. Link liblzo2 and compile with
// ST_HAVE_LZO1X=1 when the library is available for the target platform.
// Otherwise returns -4 (decoder unavailable). Never guesses output length.
int32_t st_lzo1x_decode(const uint8_t *source, uint32_t source_size,
                       uint8_t *destination, uint32_t capacity,
                       uint32_t *decoded_size, void *context);

// Decode a top-level X-Ray chunk, using LZO1X only when its compression
// flag is set. Plain chunks are copied without requiring LZO.
// -4 means LZO support is not linked; -7 means decompression failed.
int32_t st_chunk_decode_lzo(const uint8_t *bytes, uint32_t length,
                            uint32_t index, uint8_t *destination,
                            uint32_t destination_capacity,
                            uint32_t *decoded_size);

#ifdef __cplusplus
}
#endif
