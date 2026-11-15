#include "../../Sources/Native/Portable/ChunkReader.hpp"
#include <cassert>
#include <cstdint>

using namespace stalker_port;

int main() {
    const std::uint8_t data[] = {
        1, 0, 0, 0, 3, 0, 0, 0, 'a', 'b', 'c',
        2, 0, 0, 0x80, 0, 0, 0, 0
    };
    ChunkReader reader(data, sizeof(data));
    ChunkHeader chunk;
    assert(reader.next(chunk) == ChunkStatus::ok);
    assert(chunk.id == 1 && chunk.payload_size == 3);
    assert(chunk.payload_offset == 8 && !chunk.compressed);
    assert(reader.next(chunk) == ChunkStatus::ok);
    assert(chunk.id == 2 && chunk.compressed && chunk.payload_size == 0);
    assert(reader.next(chunk) == ChunkStatus::end);

    const std::uint8_t truncated[] = {1, 0, 0};
    ChunkReader short_reader(truncated, sizeof(truncated));
    assert(short_reader.next(chunk) == ChunkStatus::truncated_header);

    const std::uint8_t oversized[] = {1, 0, 0, 0, 255, 255, 255, 255};
    ChunkReader bad_reader(oversized, sizeof(oversized));
    assert(bad_reader.next(chunk) == ChunkStatus::payload_out_of_bounds);

    ChunkReader empty(nullptr, 0);
    assert(empty.next(chunk) == ChunkStatus::end);
    return 0;
}
