#include "ChunkReaderC.h"
#include "ChunkReader.hpp"

extern "C" StalkerChunkResult stalker_next_chunk(const uint8_t* data,
                                                   size_t length,
                                                   size_t* offset,
                                                   StalkerChunkInfo* result) {
    if (!offset || !result || *offset > length || (length && !data))
        return STALKER_CHUNK_INVALID_ARGUMENT;
    const size_t start = *offset;
    stalker_port::ChunkReader reader(data ? data + start : nullptr,
                                     length - start);
    stalker_port::ChunkHeader header;
    const auto status = reader.next(header);
    switch (status) {
    case stalker_port::ChunkStatus::end: return STALKER_CHUNK_END;
    case stalker_port::ChunkStatus::truncated_header: return STALKER_CHUNK_TRUNCATED;
    case stalker_port::ChunkStatus::payload_out_of_bounds:
        return STALKER_CHUNK_OUT_OF_BOUNDS;
    case stalker_port::ChunkStatus::ok:
        result->id = header.id;
        result->payload_size = header.payload_size;
        result->payload_offset = start + header.payload_offset;
        result->compressed = header.compressed ? 1 : 0;
        *offset = start + reader.position();
        return STALKER_CHUNK_OK;
    }
    return STALKER_CHUNK_INVALID_ARGUMENT;
}
