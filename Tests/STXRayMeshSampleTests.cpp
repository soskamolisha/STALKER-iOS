#include "../Sources/Native/STXRayMesh.h"
#include <cassert>
#include <cstring>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>
#include <vector>
#include <unistd.h>

static void write32(std::ofstream& out, unsigned value) {
    const unsigned char b[] = {
        static_cast<unsigned char>(value),
        static_cast<unsigned char>(value >> 8),
        static_cast<unsigned char>(value >> 16),
        static_cast<unsigned char>(value >> 24)
    };
    out.write(reinterpret_cast<const char*>(b), 4);
}
static void writeFloat(std::ofstream& out, float value) {
    static_assert(sizeof(float) == 4, "requires 32-bit float");
    unsigned bits = 0;
    std::memcpy(&bits, &value, 4);
    write32(out, bits);
}
int main() {
    char name[] = "/tmp/xray-sampler-XXXXXX";
    int fd = mkstemp(name);
    assert(fd >= 0);
    close(fd);
    const std::string path(name);
    {
        std::ofstream out(path, std::ios::binary);
        write32(out, 4); write32(out, 6); write32(out, 4);
        for (int i = 0; i < 3; ++i) writeFloat(out, 0);
        for (int i = 0; i < 3; ++i) writeFloat(out, 10);
        for (int i = 0; i < 6; ++i) {
            writeFloat(out, static_cast<float>(i));
            writeFloat(out, 0);
            writeFloat(out, 0);
        }
        const unsigned faces[4][3] = {{0,1,2},{1,2,3},{2,3,4},{3,4,5}};
        for (const auto& face : faces) {
            for (unsigned index : face) write32(out, index);
            write32(out, 0);
        }
    }
    float xyz[18] = {};
    uint32_t indices[12] = {};
    uint32_t vertices = 0, triangles = 0;
    assert(st_xray_read_collision_sample(path.c_str(), xyz, 6, indices, 2, &vertices, &triangles) == 0);
    assert(triangles == 2 && vertices == 5);
    assert(indices[0] == 0 && indices[1] == 1 && indices[2] == 2);
    assert(indices[3] == 2 && indices[4] == 3 && indices[5] == 4);
    assert(xyz[0] == 0 && xyz[6] == 2 && xyz[12] == 4);
    assert(st_xray_read_collision_sample(path.c_str(), xyz, 3, indices, 2, &vertices, &triangles) == 0);
    assert(triangles == 1 && vertices == 3);
    assert(st_xray_read_collision_sample(path.c_str(), xyz, 2, indices, 2, &vertices, &triangles) == -3);
    assert(st_xray_read_collision_sample(nullptr, xyz, 6, indices, 2, &vertices, &triangles) == -1);
    assert(std::remove(path.c_str()) == 0);
}
