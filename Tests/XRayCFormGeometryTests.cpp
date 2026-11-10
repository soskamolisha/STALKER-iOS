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
    assert(std::remove(path.c_str()) == 0);
}
