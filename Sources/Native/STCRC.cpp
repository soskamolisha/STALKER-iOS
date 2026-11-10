#include "STCRC.h"
#include <cstdint>
using u32 = std::uint32_t;
extern u32 crc32(const void *, u32);
extern "C" uint32_t st_openxray_crc32(const void *bytes, uint32_t length) {
    if (!bytes && length != 0) return 0;
    return crc32(bytes, length);
}
