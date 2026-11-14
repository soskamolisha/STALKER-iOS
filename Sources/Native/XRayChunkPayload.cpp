#include "XRayChunkPayload.hpp"
#include <cerrno>
#include <fcntl.h>
#include <limits>
#include <sys/stat.h>
#include <unistd.h>

namespace stalker {
bool readChunkPayload(const std::string& path, const ChunkEntry& chunk,
                      std::vector<std::uint8_t>& bytes, std::size_t maxBytes) {
    bytes.clear();
    if (chunk.compressed || chunk.size > maxBytes) return false;
    const int fd = open(path.c_str(), O_RDONLY | O_NOFOLLOW | O_CLOEXEC);
    if (fd < 0) return false;
    struct stat info {};
    if (fstat(fd, &info) != 0 || !S_ISREG(info.st_mode) || info.st_size < 0) {
        close(fd);
        return false;
    }
    const auto fileSize = static_cast<std::uint64_t>(info.st_size);
    if (chunk.offset > fileSize || chunk.size > fileSize - chunk.offset ||
        chunk.offset > static_cast<std::uint64_t>(std::numeric_limits<off_t>::max()) ||
        chunk.size > static_cast<std::uint64_t>(std::numeric_limits<off_t>::max()) - chunk.offset) {
        close(fd);
        return false;
    }
    bytes.resize(chunk.size);
    std::size_t done = 0;
    while (done < bytes.size()) {
        const ssize_t count = pread(fd, bytes.data() + done, bytes.size() - done,
                                    static_cast<off_t>(chunk.offset + done));
        if (count < 0 && errno == EINTR) continue;
        if (count <= 0) {
            bytes.clear();
            close(fd);
            return false;
        }
        done += static_cast<std::size_t>(count);
    }
    close(fd);
    return true;
}
}
