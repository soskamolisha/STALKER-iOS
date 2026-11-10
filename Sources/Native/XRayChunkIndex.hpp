#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace stalker {
struct ChunkEntry {
    std::uint32_t id = 0;
    std::uint64_t offset = 0;
    std::uint32_t size = 0;
    bool compressed = false;
};
// Indexes top-level chunks without allocating or decoding payloads.
// Returns false for truncated data, malformed sizes, or too many chunks.
bool indexXRayChunks(const std::string& filePath, std::vector<ChunkEntry>& entries,
                     std::uint32_t maxChunks = 65536);
}
