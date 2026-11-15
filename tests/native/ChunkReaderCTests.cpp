#include "../../Sources/Native/Portable/ChunkReaderC.h"
#include <cassert>
#include <cstdint>

int main() {
    const uint8_t bytes[] = {
        1,0,0,0, 2,0,0,0, 0xAB,0xCD,
        2,0,0,0x80, 0,0,0,0
    };
    size_t offset = 0;
    StalkerChunkInfo info{};
    assert(stalker_next_chunk(bytes, sizeof(bytes), &offset, &info) == STALKER_CHUNK_OK);
    assert(info.id == 1 && info.payload_size == 2 && info.payload_offset == 8);
    assert(offset == 10 && !info.compressed);
    assert(stalker_next_chunk(bytes, sizeof(bytes), &offset, &info) == STALKER_CHUNK_OK);
    assert(info.id == 2 && info.compressed && offset == sizeof(bytes));
    assert(stalker_next_chunk(bytes, sizeof(bytes), &offset, &info) == STALKER_CHUNK_END);

    const uint8_t invalid[] = {1,0,0,0, 100,0,0,0};
    offset = 0;
    assert(stalker_next_chunk(invalid, sizeof(invalid), &offset, &info)
           == STALKER_CHUNK_OUT_OF_BOUNDS);
    assert(offset == 0);
    assert(stalker_next_chunk(invalid, sizeof(invalid), nullptr, &info)
           == STALKER_CHUNK_INVALID_ARGUMENT);
    assert(stalker_next_chunk(invalid, sizeof(invalid), &offset, nullptr)
           == STALKER_CHUNK_INVALID_ARGUMENT);
    return 0;
}
