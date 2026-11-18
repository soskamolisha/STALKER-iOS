#include "../Sources/Native/XRayCFormGeometry.hpp"
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>
#include <vector>
#include <unistd.h>

int main() {
    char name[] = "/tmp/xray-cform-geometry-XXXXXX";
    int fd = mkstemp(name);
    assert(fd >= 0);
    close(fd);
    const std::string path(name);
    const unsigned char bytes[] = {
        4,0,0,0, 3,0,0,0, 1,0,0,0,
        0,0,0,0, 0,0,0,0, 0,0,0,0,
        0,0,128,63, 0,0,128,63, 0,0,128,63,
        0,0,0,0, 0,0,0,0, 0,0,0,0,
        0,0,128,63, 0,0,0,0, 0,0,0,0,
        0,0,0,0, 0,0,128,63, 0,0,0,0,
        0,0,0,0, 1,0,0,0, 2,0,0,0, 0,0,0,0
    };
    { std::ofstream out(path, std::ios::binary); out.write(reinterpret_cast<const char*>(bytes), sizeof(bytes)); }
    stalker::CFormHeader header;
    std::vector<stalker::CFormVertex> vertices;
    std::vector<stalker::CFormTriangle> triangles;
    assert(stalker::readCFormGeometry(path, header, vertices, triangles));
    assert(vertices.size() == 3 && triangles.size() == 1);
    assert(vertices[1].x == 1.0f && triangles[0].c == 2);
    assert(!stalker::readCFormGeometry(path, header, vertices, triangles, 2, 1));
    assert(vertices.empty() && triangles.empty());
    // Reject a face that references a vertex outside the declared vertex array.
    {
        auto invalid = std::vector<unsigned char>(bytes, bytes + sizeof(bytes));
        invalid[sizeof(bytes) - 8] = 9; // third vertex index
        std::ofstream out(path, std::ios::binary | std::ios::trunc);
        out.write(reinterpret_cast<const char*>(invalid.data()), invalid.size());
    }
    assert(!stalker::readCFormGeometry(path, header, vertices, triangles));
    assert(vertices.empty() && triangles.empty());
    // Reject NaN coordinates before they reach the Metal GPU buffers.
    {
        auto invalid = std::vector<unsigned char>(bytes, bytes + sizeof(bytes));
        invalid[36] = 0; invalid[37] = 0; invalid[38] = 192; invalid[39] = 127;
        std::ofstream out(path, std::ios::binary | std::ios::trunc);
        out.write(reinterpret_cast<const char*>(invalid.data()), invalid.size());
    }
    assert(!stalker::readCFormGeometry(path, header, vertices, triangles));
    assert(vertices.empty() && triangles.empty());
    assert(std::remove(path.c_str()) == 0);
}
