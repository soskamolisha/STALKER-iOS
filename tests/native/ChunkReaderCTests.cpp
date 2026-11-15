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
    // Scanning from a nonzero offset must report absolute payload positions.
    offset = 10;
    assert(stalker_next_chunk(bytes, sizeof(bytes), &offset, &info) == STALKER_CHUNK_OK);
    assert(info.id == 2 && info.payload_offset == 18 && offset == sizeof(bytes));

    // Truncated header leaves offset unchanged.
    offset = 0;
    const uint8_t partial[] = {1, 2, 3};
    assert(stalker_next_chunk(partial, sizeof(partial), &offset, &info)
           == STALKER_CHUNK_TRUNCATED);
    assert(offset == 0);

    // Invalid offset and missing data are rejected without dereferencing.
    offset = sizeof(bytes) + 1;
    assert(stalker_next_chunk(bytes, sizeof(bytes), &offset, &info)
           == STALKER_CHUNK_INVALID_ARGUMENT);
    offset = 0;
    assert(stalker_next_chunk(nullptr, 1, &offset, &info)
           == STALKER_CHUNK_INVALID_ARGUMENT);
    // Empty input is a valid end-of-stream, even with no backing pointer.
    offset = 0;
    assert(stalker_next_chunk(nullptr, 0, &offset, &info) == STALKER_CHUNK_END);
    assert(offset == 0);

    // A zero-length payload is legal and must advance by the header size.
    const uint8_t zero_payload[] = {7,0,0,0, 0,0,0,0};
    offset = 0;
    assert(stalker_next_chunk(zero_payload, sizeof(zero_payload), &offset, &info)
           == STALKER_CHUNK_OK);
    assert(info.id == 7 && info.payload_size == 0);
    assert(info.payload_offset == sizeof(zero_payload));
    assert(offset == sizeof(zero_payload));

    // A malformed header must not overwrite the previous successful result.
    const StalkerChunkInfo previous = info;
    offset = 0;
    assert(stalker_next_chunk(partial, sizeof(partial), &offset, &info)
           == STALKER_CHUNK_TRUNCATED);
    assert(info.id == previous.id && info.payload_size == previous.payload_size &&
           info.payload_offset == previous.payload_offset &&
           info.compressed == previous.compressed);

    // Exercise every truncated header length; none may consume bytes.
    const uint8_t header[] = {3,0,0,0, 0,0,0,0};
    for (size_t length = 1; length < sizeof(header); ++length) {
        offset = 0;
        assert(stalker_next_chunk(header, length, &offset, &info)
               == STALKER_CHUNK_TRUNCATED);
        assert(offset == 0);
    }
    return 0;
}
