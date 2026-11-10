#pragma once
#include <cstdint>
#include <string>

namespace stalker {
struct LevelHeader {
    std::uint32_t chunkId = 0;
    std::uint32_t payloadSize = 0;
    bool compressed = false;
};
// Reads only the first 8-byte X-Ray chunk header. Does not decode the level.
bool readLevelFirstChunk(const std::string& path, LevelHeader& result);
}
