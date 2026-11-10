#include "../Sources/Native/STChunks.h"
#include <cassert>
#include <cstdint>
#include <vector>
static void append32(std::vector<uint8_t>& v, uint32_t n) {
    for (int i = 0; i < 4; ++i) v.push_back(static_cast<uint8_t>(n >> (i * 8)));
}
int main() {
    std::vector<uint8_t> v;
    append32(v, 0x8000002aU); append32(v, 3);
    v.push_back('a'); v.push_back('b'); v.push_back('c');
    append32(v, 7); append32(v, 0);
    uint32_t count = 99, id = 99, size = 99;
    assert(st_chunk_scan(v.data(), static_cast<uint32_t>(v.size()), &count, &id, &size) == 0);
    assert(count == 2 && id == 42 && size == 3);
    assert(st_chunk_scan(v.data(), static_cast<uint32_t>(v.size()-1), &count, &id, &size) == -1);
    assert(st_chunk_scan(nullptr, 0, &count, &id, &size) == -1);
    std::vector<uint8_t> huge;
    append32(huge, 1); append32(huge, 0xffffffffU);
    assert(st_chunk_scan(huge.data(), static_cast<uint32_t>(huge.size()), &count, &id, &size) == -1);
    std::vector<uint8_t> many;
    for (int i = 0; i < 4097; ++i) { append32(many, 1); append32(many, 0); }
    assert(st_chunk_scan(many.data(), static_cast<uint32_t>(many.size()), &count, &id, &size) == -2);
}
