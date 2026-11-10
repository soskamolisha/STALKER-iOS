#include "XRayLevelHeader.hpp"
#include <fstream>
#include <sys/stat.h>

namespace stalker {
bool readLevelFirstChunk(const std::string& path, LevelHeader& result) {
    result = {};
    struct stat info {};
    if (lstat(path.c_str(), &info) != 0 || !S_ISREG(info.st_mode) || info.st_size < 8)
        return false;
    std::ifstream file(path, std::ios::binary);
    unsigned char bytes[8] = {};
    if (!file.read(reinterpret_cast<char*>(bytes), sizeof(bytes))) return false;
    const auto read32 = [](const unsigned char* p) -> std::uint32_t {
        return std::uint32_t(p[0]) | (std::uint32_t(p[1]) << 8) |
               (std::uint32_t(p[2]) << 16) | (std::uint32_t(p[3]) << 24);
    };
    const std::uint32_t rawId = read32(bytes);
    const std::uint32_t payload = read32(bytes + 4);
    // Reject a truncated first chunk, including compressed payloads.
    if (static_cast<std::uint64_t>(payload) > static_cast<std::uint64_t>(info.st_size) - 8)
        return false;
    result.chunkId = rawId & 0x7fffffffU;
    result.compressed = (rawId & 0x80000000U) != 0;
    result.payloadSize = payload;
    return true;
}
}
