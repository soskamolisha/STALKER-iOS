#pragma once
// Original structural reader for X-Ray chunk containers. No payload decoding.
#include <array>
#include <cstdint>
#include <istream>
#include <stdexcept>
#include <vector>

namespace stalker {
struct ArchiveChunk {
    uint32_t id;
    uint64_t offset;
    uint32_t size;
    bool compressed;
};
inline uint32_t little32(const unsigned char* p) {
    return uint32_t(p[0]) | (uint32_t(p[1]) << 8) |
        (uint32_t(p[2]) << 16) | (uint32_t(p[3]) << 24);
}
// Requires a seekable stream. Reads headers only; never allocates payload sizes.
// Successful parsing proves framing only, not game identity or payload validity.
inline std::vector<ArchiveChunk> scanArchive(std::istream& input) {
    input.clear();
    input.seekg(0, std::ios::end);
    const auto end = input.tellg();
    if (end < std::streampos(0)) throw std::runtime_error("Cannot measure archive");
    const uint64_t length = static_cast<uint64_t>(static_cast<std::streamoff>(end));
    if (length == 0) throw std::runtime_error("Empty archive");
    uint64_t position = 0;
    std::vector<ArchiveChunk> chunks;
    while (position < length) {
        if (chunks.size() == 4096) throw std::runtime_error("Too many chunks");
        if (length - position < 8) throw std::runtime_error("Truncated chunk header");
        input.seekg(static_cast<std::streamoff>(position), std::ios::beg);
        std::array<unsigned char, 8> header{};
        if (!input.read(reinterpret_cast<char*>(header.data()), header.size()))
            throw std::runtime_error("Cannot read chunk header");
        const auto type = little32(header.data());
        const auto size = little32(header.data() + 4);
        position += 8;
        if (size > length - position) throw std::runtime_error("Chunk exceeds archive");
        chunks.push_back({type & 0x7fffffffU, position, size, (type & 0x80000000U) != 0});
        position += size;
    }
    return chunks;
}
}
