#include "XRayChunkIndex.hpp"
#include <cerrno>
#include <fcntl.h>
#include <limits>
#include <sys/stat.h>
#include <unistd.h>

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
    const int fd = open(filePath.c_str(), O_RDONLY | O_NOFOLLOW | O_CLOEXEC);
    if (fd < 0) return false;
    struct stat st {};
    if (fstat(fd, &st) != 0 || !S_ISREG(st.st_mode) || st.st_size < 0) {
        close(fd);
        return false;
    }
    const auto fileSize = static_cast<std::uint64_t>(st.st_size);
    std::uint64_t cursor = 0;
    while (cursor < fileSize) {
        if (entries.size() >= maxChunks || fileSize - cursor < 8 ||
            cursor > static_cast<std::uint64_t>(std::numeric_limits<off_t>::max())) {
            entries.clear();
            close(fd);
            return false;
        }
        unsigned char header[8] = {};
        std::size_t done = 0;
        while (done < sizeof(header)) {
            const ssize_t n = pread(fd, header + done, sizeof(header) - done,
                                     static_cast<off_t>(cursor + done));
            if (n < 0 && errno == EINTR) continue;
            if (n <= 0) {
                entries.clear();
                close(fd);
                return false;
            }
            done += static_cast<std::size_t>(n);
        }
        const auto rawId = le32(header);
        const auto length = le32(header + 4);
        cursor += 8;
        if (length > fileSize - cursor) {
            entries.clear();
            close(fd);
            return false;
        }
        entries.push_back({rawId & 0x7fffffffU, cursor, length,
                           (rawId & 0x80000000U) != 0});
        cursor += length;
    }
    close(fd);
    return true;
}
}
