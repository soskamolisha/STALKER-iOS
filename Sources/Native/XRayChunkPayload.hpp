#pragma once
#include "XRayChunkIndex.hpp"
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace stalker {
// Reads an uncompressed chunk payload with an explicit memory cap.
// Compressed chunks require a separate decoder.
bool readChunkPayload(const std::string& path, const ChunkEntry& chunk,
                      std::vector<std::uint8_t>& bytes,
                      std::size_t maxBytes = 1024 * 1024);
}
