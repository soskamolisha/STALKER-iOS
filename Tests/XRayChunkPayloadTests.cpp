#include "../Sources/Native/XRayChunkPayload.hpp"
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>
#include <vector>
#include <unistd.h>

int main() {
    char name[] = "/tmp/xray-payload-XXXXXX";
    const int fd = mkstemp(name);
    assert(fd >= 0);
    close(fd);
    const std::string path(name);
    {
        std::ofstream out(path, std::ios::binary);
        const unsigned char bytes[] = {1,0,0,0,3,0,0,0,10,20,30};
        out.write(reinterpret_cast<const char*>(bytes), sizeof(bytes));
    }
    std::vector<stalker::ChunkEntry> chunks;
    assert(stalker::indexXRayChunks(path, chunks));
    assert(chunks.size() == 1);
    std::vector<std::uint8_t> payload;
    assert(stalker::readChunkPayload(path, chunks[0], payload));
    assert(payload == std::vector<std::uint8_t>({10,20,30}));
    assert(!stalker::readChunkPayload(path, chunks[0], payload, 2));
    assert(payload.empty());
    auto compressed = chunks[0];
    compressed.compressed = true;
    assert(!stalker::readChunkPayload(path, compressed, payload));
    auto invalid = chunks[0];
    invalid.offset = 100;
    assert(!stalker::readChunkPayload(path, invalid, payload));
    assert(std::remove(path.c_str()) == 0);
}
