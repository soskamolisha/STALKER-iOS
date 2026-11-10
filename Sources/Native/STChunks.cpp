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

extern "C" int32_t st_chunk_at(const uint8_t* bytes, uint32_t length, uint32_t index,
    uint32_t* id, uint32_t* payload_offset, uint32_t* payload_size,
    uint32_t* compressed) {
    if (!bytes || length == 0 || !id || !payload_offset || !payload_size || !compressed)
        return -1;
    std::uint64_t offset = 0;
    std::uint32_t count = 0;
    bool found = false;
    std::uint32_t selectedID = 0, selectedOffset = 0, selectedSize = 0, selectedCompressed = 0;
    while (offset < length) {
        if (count == 4096) return -2;
        if (length - offset < 8) return -1;
        const std::uint32_t type = read32(bytes + offset);
        const std::uint32_t size = read32(bytes + offset + 4);
        offset += 8;
        if (size > length - offset) return -1;
        if (count == index) {
            found = true;
            selectedID = type & 0x7fffffffU;
            selectedOffset = static_cast<std::uint32_t>(offset);
            selectedSize = size;
            selectedCompressed = (type & 0x80000000U) != 0 ? 1U : 0U;
        }
        ++count;
        offset += size;
    }
    if (!found) return -3;
    *id = selectedID;
    *payload_offset = selectedOffset;
    *payload_size = selectedSize;
    *compressed = selectedCompressed;
    return 0;
}

extern "C" int32_t st_chunk_scan_children(const uint8_t* bytes, uint32_t length,
    uint32_t parent_index, uint32_t* child_count, uint32_t* first_child_id) {
    if (!bytes || !child_count || !first_child_id) return -1;
    *child_count = 0;
    *first_child_id = 0;
    std::uint32_t id = 0, offset = 0, size = 0, compressed = 0;
    const int32_t result = st_chunk_at(bytes, length, parent_index,
        &id, &offset, &size, &compressed);
    if (result != 0) return result;
    if (compressed != 0) return -4;
    if (size == 0) return 0;
    std::uint32_t first_size = 0;
    return st_chunk_scan(bytes + offset, size, child_count, first_child_id, &first_size);
}

extern "C" int32_t st_chunk_copy_slice(const uint8_t* bytes, uint32_t length,
    uint32_t index, uint32_t slice_offset, uint32_t slice_length,
    uint8_t* destination, uint32_t destination_capacity) {
    if (!bytes || (slice_length != 0 && !destination)) return -1;
    std::uint32_t id = 0, offset = 0, size = 0, compressed = 0;
    const int32_t result = st_chunk_at(bytes, length, index,
        &id, &offset, &size, &compressed);
    if (result != 0) return result;
    if (compressed != 0) return -4;
    if (slice_offset > size || slice_length > size - slice_offset) return -5;
    if (slice_length > destination_capacity) return -6;
    // A zero-byte slice needs no destination pointer.
    for (std::uint32_t i = 0; i < slice_length; ++i)
        destination[i] = bytes[static_cast<std::size_t>(offset) + slice_offset + i];
    return 0;
}
