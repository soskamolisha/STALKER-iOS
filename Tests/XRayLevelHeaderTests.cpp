#include "../Sources/Native/XRayLevelHeader.hpp"
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>
#include <unistd.h>

int main() {
    char name[] = "/tmp/xray-level-header-XXXXXX";
    int fd = mkstemp(name);
    assert(fd >= 0);
    close(fd);
    const std::string path(name);
    stalker::LevelHeader header;
    assert(!stalker::readLevelFirstChunk(path, header));
    {
        std::ofstream out(path, std::ios::binary | std::ios::trunc);
        const unsigned char data[] = {42,0,0,128,3,0,0,0,'A','B','C'};
        out.write(reinterpret_cast<const char*>(data), sizeof(data));
    }
    assert(stalker::readLevelFirstChunk(path, header));
    assert(header.chunkId == 42 && header.compressed && header.payloadSize == 3);
    {
        std::ofstream out(path, std::ios::binary | std::ios::trunc);
        const unsigned char truncated[] = {42,0,0,0,10,0,0,0,'A'};
        out.write(reinterpret_cast<const char*>(truncated), sizeof(truncated));
    }
    assert(!stalker::readLevelFirstChunk(path, header));
    assert(header.chunkId == 0 && header.payloadSize == 0);
    assert(std::remove(path.c_str()) == 0);
}
