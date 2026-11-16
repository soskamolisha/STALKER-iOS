#include "STLZOAdapter.h"
#include <cstddef>
#include <limits>

#if defined(ST_HAVE_LZO1X) && ST_HAVE_LZO1X
#include <lzo/lzo1x.h>
#endif

extern "C" int32_t st_lzo1x_decode(const uint8_t *source, uint32_t source_size,
    uint8_t *destination, uint32_t capacity, uint32_t *decoded_size, void *context) {
    (void)context;
    if (!decoded_size || (source_size && !source) || (capacity && !destination))
        return -1;
    *decoded_size = 0;
#if defined(ST_HAVE_LZO1X) && ST_HAVE_LZO1X
    if (!source || !destination || !capacity) return -1;
    if (static_cast<uint64_t>(capacity) >
        static_cast<uint64_t>(std::numeric_limits<lzo_uint>::max())) return -1;
    lzo_uint output_length = static_cast<lzo_uint>(capacity);
    const int result = lzo1x_decompress_safe(source,
        static_cast<lzo_uint>(source_size), destination, &output_length, nullptr);
    if (result != LZO_E_OK || output_length > capacity) return -7;
    *decoded_size = static_cast<uint32_t>(output_length);
    return 0;
#else
    (void)source;
    (void)source_size;
    (void)destination;
    (void)capacity;
    return -4;
#endif
}

extern "C" int32_t st_chunk_decode_lzo(const uint8_t *bytes, uint32_t length,
    uint32_t index, uint8_t *destination, uint32_t destination_capacity,
    uint32_t *decoded_size) {
    return st_chunk_decode(bytes, length, index, destination,
                           destination_capacity, decoded_size,
                           st_lzo1x_decode, nullptr);
}
