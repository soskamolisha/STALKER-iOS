// Standalone native regression tests for structural chunk parsing.
// Build on macOS/Linux: c++ -std=c++17 -Wall -Wextra -Werror
//   Tests/STChunksRegression.cpp Sources/Native/STChunks.cpp -o /tmp/stchunks-test
// Run: /tmp/stchunks-test
#include "../Sources/Native/STChunks.h"
#include <cassert>
#include <cstdint>
#include <vector>

static void append32(std::vector<uint8_t>& data, uint32_t value) {
    for (unsigned i = 0; i < 4; ++i)
        data.push_back(static_cast<uint8_t>(value >> (8 * i)));
}
static std::vector<uint8_t> chunk(uint32_t id, const std::vector<uint8_t>& payload) {
    std::vector<uint8_t> bytes;
    append32(bytes, id);
    append32(bytes, static_cast<uint32_t>(payload.size()));
    bytes.insert(bytes.end(), payload.begin(), payload.end());
    return bytes;
}
int main() {
    // The high bit indicates compression; structural scanning must not
    // mistake it for an ordinary chunk ID or silently decompress payloads.
    const auto compressed = chunk(0x8000002Au, {1, 2, 3});
    uint32_t count = 0, first = 0, size = 0;
    assert(st_chunk_scan(compressed.data(), static_cast<uint32_t>(compressed.size()),
                         &count, &first, &size) == 0);
    assert(count == 1 && first == 42 && size == 3);
    uint32_t id = 0, offset = 0, payloadSize = 0, flag = 0;
    assert(st_chunk_at(compressed.data(), static_cast<uint32_t>(compressed.size()), 0,
                       &id, &offset, &payloadSize, &flag) == 0);
    assert(id == 42 && offset == 8 && payloadSize == 3 && flag == 1);
    uint32_t children = 0, childId = 0;
    assert(st_chunk_scan_children(compressed.data(), static_cast<uint32_t>(compressed.size()),
                                  0, &children, &childId) == -4);
    uint8_t output[3] = {};
    assert(st_chunk_copy_slice(compressed.data(), static_cast<uint32_t>(compressed.size()),
                               0, 0, 3, output, sizeof(output)) == -4);

    // Uncompressed payloads can be copied without a decompressor.
    const auto plain = chunk(7, {10, 20, 30});
    assert(st_chunk_copy_slice(plain.data(), static_cast<uint32_t>(plain.size()),
                               0, 1, 2, output, sizeof(output)) == 0);
    assert(output[0] == 20 && output[1] == 30);
    assert(st_chunk_copy_slice(plain.data(), static_cast<uint32_t>(plain.size()),
                               0, 2, 2, output, sizeof(output)) == -5);

    // A declared payload larger than available bytes must be rejected.
    auto truncated = plain;
    truncated[4] = 100;
    assert(st_chunk_scan(truncated.data(), static_cast<uint32_t>(truncated.size()),
                         &count, &first, &size) == -1);
    assert(st_chunk_at(truncated.data(), static_cast<uint32_t>(truncated.size()), 0,
                       &id, &offset, &payloadSize, &flag) == -1);
    return 0;
}
