#include "../Sources/Native/STXRayMesh.h"
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>
#include <unistd.h>

int main() {
    char name[] = "/tmp/xray-mesh-bridge-XXXXXX";
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
    { std::ofstream file(path, std::ios::binary); file.write(reinterpret_cast<const char*>(bytes), sizeof(bytes)); }
    float positions[9] = {};
    std::uint32_t indices[3] = {};
    std::uint32_t vertices = 0, triangles = 0;
    assert(st_xray_read_collision_mesh(nullptr, positions, 3, indices, 1, &vertices, &triangles) == -1);
    assert(st_xray_read_collision_mesh(path.c_str(), positions, 2, indices, 1, &vertices, &triangles) == -3);
    assert(st_xray_read_collision_mesh(path.c_str(), positions, 3, indices, 1, &vertices, &triangles) == 0);
    assert(vertices == 3 && triangles == 1);
    assert(positions[3] == 1.0f && positions[7] == 1.0f);
    assert(indices[0] == 0 && indices[1] == 1 && indices[2] == 2);
    assert(std::remove(path.c_str()) == 0);
}
