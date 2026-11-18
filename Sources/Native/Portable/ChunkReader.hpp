#pragma once
// Generic bounded X-Ray-style chunk reader, not a .db0 archive parser.
#include "ByteReader.hpp"
#include <cstddef>
#include <cstdint>

namespace stalker_port {
struct ChunkHeader {
    std::uint32_t id = 0;
    std::uint32_t payload_size = 0;
    bool compressed = false;
    std::size_t payload_offset = 0;
};

enum class ChunkStatus { ok, end, truncated_header, payload_out_of_bounds };

class ChunkReader {
public:
    ChunkReader(const std::uint8_t* data, std::size_t size)
        : reader_(data, size) {}

    ChunkStatus next(ChunkHeader& out) {
        if (reader_.remaining() == 0) return ChunkStatus::end;
        if (reader_.remaining() < 8) return ChunkStatus::truncated_header;
        std::uint32_t raw_id = 0, payload_size = 0;
        if (!reader_.read_u32_le(raw_id) || !reader_.read_u32_le(payload_size))
            return ChunkStatus::truncated_header;
        if (payload_size > reader_.remaining())
            return ChunkStatus::payload_out_of_bounds;
        ChunkHeader candidate;
        candidate.id = raw_id & 0x7fffffffu;
        candidate.compressed = (raw_id & 0x80000000u) != 0;
        candidate.payload_size = payload_size;
        candidate.payload_offset = reader_.position();
        if (!reader_.skip(payload_size))
            return ChunkStatus::payload_out_of_bounds;
        out = candidate;
        return ChunkStatus::ok;
    }

    std::size_t position() const { return reader_.position(); }

private:
    ByteReader reader_;
};
} // namespace stalker_port
