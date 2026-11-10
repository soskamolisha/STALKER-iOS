#include "../Sources/Native/XRayChunkIndex.hpp"
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>
#include <vector>
#include <unistd.h>

int main() {
    char name[] = "/tmp/xray-chunks-XXXXXX";
    int fd = mkstemp(name);
    assert(fd >= 0);
    close(fd);
    const std::string path(name);
    std::vector<stalker::ChunkEntry> chunks;
    {
        std::ofstream out(path, std::ios::binary | std::ios::trunc);
        const unsigned char bytes[] = {1,0,0,0,2,0,0,0,10,11,
                                       2,0,0,128,1,0,0,0,99};
        out.write(reinterpret_cast<const char*>(bytes), sizeof(bytes));
    }
    assert(stalker::indexXRayChunks(path, chunks));
    assert(chunks.size() == 2);
    assert(chunks[0].id == 1 && chunks[0].offset == 8 && chunks[0].size == 2);
    assert(chunks[1].id == 2 && chunks[1].offset == 18 && chunks[1].compressed);
    assert(!stalker::indexXRayChunks(path, chunks, 1));
    assert(chunks.empty());
    {
        std::ofstream out(path, std::ios::binary | std::ios::trunc);
        const unsigned char bad[] = {1,0,0,0,9,0,0,0,1};
        out.write(reinterpret_cast<const char*>(bad), sizeof(bad));
    }
    assert(!stalker::indexXRayChunks(path, chunks));
    assert(chunks.empty());
    assert(std::remove(path.c_str()) == 0);
}
