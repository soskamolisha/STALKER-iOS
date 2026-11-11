#include "STXRayBootstrap.h"
#include "Upstream/minilzo.h"
#include <cstdint>

extern "C" int32_t st_xray_lzo_decompress(const void* source, uint32_t source_size,
                                          void* destination, uint32_t destination_capacity,
                                          uint32_t expected_size) {
    if (!source || !destination || source_size == 0 || expected_size == 0 ||
        expected_size > destination_capacity || expected_size > 64u * 1024u * 1024u)
        return -3;
    lzo_uint decoded_size = destination_capacity;
    const int result = lzo1x_decompress_safe(
        reinterpret_cast<const unsigned char*>(source), source_size,
        reinterpret_cast<unsigned char*>(destination), &decoded_size, nullptr);
    if (result != LZO_E_OK) return -4;
    if (decoded_size != expected_size) return -3;
    return 0;
}
