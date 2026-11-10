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
    uint32_t offset = 0, compressed = 0;
    assert(st_chunk_at(v.data(), static_cast<uint32_t>(v.size()), 0,
                       &id, &offset, &size, &compressed) == 0);
    assert(id == 42 && offset == 8 && size == 3 && compressed == 1);
    assert(st_chunk_at(v.data(), static_cast<uint32_t>(v.size()), 1,
                       &id, &offset, &size, &compressed) == 0);
    assert(id == 7 && offset == 19 && size == 0 && compressed == 0);
    assert(st_chunk_at(v.data(), static_cast<uint32_t>(v.size()), 2,
                       &id, &offset, &size, &compressed) == -3);
    assert(st_chunk_at(v.data(), static_cast<uint32_t>(v.size()-1), 0,
                       &id, &offset, &size, &compressed) == -1);
    assert(st_chunk_scan(v.data(), static_cast<uint32_t>(v.size()-1), &count, &id, &size) == -1);
    assert(st_chunk_scan(nullptr, 0, &count, &id, &size) == -1);
    std::vector<uint8_t> huge;
    append32(huge, 1); append32(huge, 0xffffffffU);
    assert(st_chunk_scan(huge.data(), static_cast<uint32_t>(huge.size()), &count, &id, &size) == -1);
    uint32_t children = 99, childID = 99;
    assert(st_chunk_scan_children(v.data(), static_cast<uint32_t>(v.size()), 0,
                                  &children, &childID) == -4);
    assert(st_chunk_scan_children(v.data(), static_cast<uint32_t>(v.size()), 1,
                                  &children, &childID) == 0);
    assert(children == 0 && childID == 0);
    assert(st_chunk_scan_children(v.data(), static_cast<uint32_t>(v.size()), 2,
                                  &children, &childID) == -3);
    std::vector<uint8_t> nested;
    append32(nested, 21); append32(nested, 10);
    append32(nested, 9); append32(nested, 2);
    nested.push_back(0xAA); nested.push_back(0xBB);
    assert(st_chunk_scan_children(nested.data(), static_cast<uint32_t>(nested.size()),
                                  0, &children, &childID) == 0);
    assert(children == 1 && childID == 9);
    nested.pop_back();
    assert(st_chunk_scan_children(nested.data(), static_cast<uint32_t>(nested.size()),
                                  0, &children, &childID) == -1);
    std::vector<uint8_t> many;
    for (int i = 0; i < 4097; ++i) { append32(many, 1); append32(many, 0); }
    assert(st_chunk_scan(many.data(), static_cast<uint32_t>(many.size()), &count, &id, &size) == -2);
}
