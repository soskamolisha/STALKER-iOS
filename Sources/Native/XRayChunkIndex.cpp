#include "XRayChunkIndex.hpp"
#include <fstream>
#include <sys/stat.h>

namespace stalker {
namespace {
std::uint32_t le32(const unsigned char* b) {
    return std::uint32_t(b[0]) | (std::uint32_t(b[1]) << 8) |
           (std::uint32_t(b[2]) << 16) | (std::uint32_t(b[3]) << 24);
}
}
bool indexXRayChunks(const std::string& filePath, std::vector<ChunkEntry>& entries,
                     std::uint32_t maxChunks) {
    entries.clear();
    struct stat st {};
    if (lstat(filePath.c_str(), &st) != 0 || !S_ISREG(st.st_mode) || st.st_size < 0)
        return false;
    const auto fileSize = static_cast<std::uint64_t>(st.st_size);
    std::ifstream input(filePath, std::ios::binary);
    if (!input) return false;
    std::uint64_t cursor = 0;
    while (cursor < fileSize) {
        if (entries.size() >= maxChunks || fileSize - cursor < 8) {
            entries.clear();
            return false;
        }
        unsigned char header[8] = {};
        if (!input.read(reinterpret_cast<char*>(header), sizeof(header))) {
            entries.clear();
            return false;
        }
        const auto rawId = le32(header);
        const auto length = le32(header + 4);
        cursor += 8;
        if (length > fileSize - cursor) {
            entries.clear();
            return false;
        }
        entries.push_back({rawId & 0x7fffffffU, cursor, length,
                           (rawId & 0x80000000U) != 0});
        cursor += length;
        input.seekg(static_cast<std::streamoff>(cursor), std::ios::beg);
        if (!input && cursor < fileSize) {
            entries.clear();
            return false;
        }
    }
    return true;
}
}
