#include "STChunks.h"
#include <cstdint>
#include <cstddef>
static std::uint32_t read32(const std::uint8_t* p) {
    return std::uint32_t(p[0]) | (std::uint32_t(p[1]) << 8) |
        (std::uint32_t(p[2]) << 16) | (std::uint32_t(p[3]) << 24);
}
extern "C" int32_t st_chunk_scan(const uint8_t* bytes, uint32_t length,
    uint32_t* count, uint32_t* first_id, uint32_t* first_size) {
    if (!count || !first_id || !first_size || !bytes || length == 0) return -1;
    *count = 0; *first_id = 0; *first_size = 0;
    std::uint64_t offset = 0;
    while (offset < length) {
        if (*count == 4096) return -2;
        if (length - offset < 8) return -1;
        const std::uint32_t id = read32(bytes + offset);
        const std::uint32_t size = read32(bytes + offset + 4);
        offset += 8;
        if (size > length - offset) return -1;
        if (*count == 0) { *first_id = id & 0x7fffffffU; *first_size = size; }
        ++*count;
        offset += size;
    }
    return 0;
}
