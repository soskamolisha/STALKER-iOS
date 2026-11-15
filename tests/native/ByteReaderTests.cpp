#include "../../Sources/Native/Portable/ByteReader.hpp"
#include <cassert>
#include <cstdint>
#include <limits>

int main() {
    const std::uint8_t bytes[] = {0x78, 0x56, 0x34, 0x12, 0xAA};
    stalker_port::ByteReader reader(bytes, sizeof(bytes));
    std::uint32_t value = 0;
    assert(reader.read_u32_le(value));
    assert(value == 0x12345678u);
    assert(reader.position() == 4);
    assert(reader.remaining() == 1);
    assert(!reader.read_u32_le(value));
    assert(reader.position() == 4);
    assert(!reader.skip(std::numeric_limits<std::size_t>::max()));
    assert(reader.skip(1));
    assert(reader.remaining() == 0);
    assert(!reader.skip(1));

    const std::uint8_t small[] = {0x34, 0x12, 0xAB};
    stalker_port::ByteReader second(small, sizeof(small));
    std::uint16_t short_value = 0;
    assert(second.read_u16_le(short_value));
    assert(short_value == 0x1234u);
    std::uint8_t last = 0;
    assert(second.read_bytes(&last, 1));
    assert(last == 0xAB);
    assert(!second.read_u16_le(short_value));
    assert(!second.read_bytes(nullptr, 1));
    assert(second.read_bytes(nullptr, 0));

    stalker_port::ByteReader empty(nullptr, 0);
    assert(!empty.read_u32_le(value));
    assert(empty.remaining() == 0);
    return 0;
}
