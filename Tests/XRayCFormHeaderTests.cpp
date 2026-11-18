#include "../Sources/Native/XRayCFormHeader.hpp"
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>
#include <unistd.h>

int main() {
    char name[] = "/tmp/xray-cform-XXXXXX";
    int fd = mkstemp(name);
    assert(fd >= 0);
    close(fd);
    const std::string path(name);
    stalker::CFormHeader header;
    {
        std::ofstream out(path, std::ios::binary);
        const unsigned char bytes[] = {
            4,0,0,0, 1,0,0,0, 0,0,0,0,
            0,0,0,0, 0,0,0,0, 0,0,0,0,
            0,0,128,63, 0,0,128,63, 0,0,128,63,
            0,0,0,0, 0,0,0,0, 0,0,0,0
        };
        out.write(reinterpret_cast<const char*>(bytes), sizeof(bytes));
    }
    assert(stalker::readCFormHeader(path, header));
    assert(header.version == 4 && header.vertexCount == 1 && header.faceCount == 0);
    assert(header.maximum[0] == 1.0f);
    {
        std::ofstream out(path, std::ios::binary | std::ios::trunc);
        const unsigned char bad[] = {4,0,0,0, 255,255,255,255};
        out.write(reinterpret_cast<const char*>(bad), sizeof(bad));
    }
    assert(!stalker::readCFormHeader(path, header));
    assert(std::remove(path.c_str()) == 0);
}
