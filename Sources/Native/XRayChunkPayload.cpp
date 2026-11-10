#include "XRayChunkPayload.hpp"
#include <fstream>
#include <limits>
#include <sys/stat.h>

namespace stalker {
bool readChunkPayload(const std::string& path, const ChunkEntry& chunk,
                      std::vector<std::uint8_t>& bytes, std::size_t maxBytes) {
    bytes.clear();
    if (chunk.compressed || chunk.size > maxBytes) return false;
    struct stat info {};
    if (lstat(path.c_str(), &info) != 0 || !S_ISREG(info.st_mode) || info.st_size < 0)
        return false;
    const auto fileSize = static_cast<std::uint64_t>(info.st_size);
    if (chunk.offset > fileSize || chunk.size > fileSize - chunk.offset ||
        chunk.offset > static_cast<std::uint64_t>(std::numeric_limits<std::streamoff>::max()))
        return false;
    std::ifstream file(path, std::ios::binary);
    if (!file) return false;
    file.seekg(static_cast<std::streamoff>(chunk.offset), std::ios::beg);
    if (!file) return false;
    bytes.resize(chunk.size);
    if (chunk.size && !file.read(reinterpret_cast<char*>(bytes.data()), chunk.size)) {
        bytes.clear();
        return false;
    }
    return true;
}
}
