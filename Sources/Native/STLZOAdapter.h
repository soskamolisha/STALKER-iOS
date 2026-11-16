#pragma once
#include "STChunks.h"
#ifdef __cplusplus
extern "C" {
#endif

// Adapter for LZO1X payloads. Link liblzo2 and compile with
// ST_HAVE_LZO1X=1 when the library is available for the target platform.
// Otherwise returns -4 (decoder unavailable). Never guesses output length.
int32_t st_lzo1x_decode(const uint8_t *source, uint32_t source_size,
                       uint8_t *destination, uint32_t capacity,
                       uint32_t *decoded_size, void *context);

#ifdef __cplusplus
}
#endif
