#include "STXRayMesh.h"
#include "XRayCFormHeader.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <limits>
#include <unordered_map>
#include <vector>

namespace {
uint32_t le32(const unsigned char* p) {
    return uint32_t(p[0]) | (uint32_t(p[1]) << 8) |
           (uint32_t(p[2]) << 16) | (uint32_t(p[3]) << 24);
}
float leFloat(const unsigned char* p) {
    const auto bits = le32(p);
    float value;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}
}

extern "C" int32_t st_xray_read_collision_sample(const char* utf8_path,
    float* xyz, uint32_t vertex_capacity, uint32_t* indices,
    uint32_t triangle_capacity, uint32_t* vertex_count, uint32_t* triangle_count) {
    if (!utf8_path || !xyz || !indices || !vertex_count || !triangle_count ||
        !vertex_capacity || !triangle_capacity) return -1;
    *vertex_count = 0;
    *triangle_count = 0;
    stalker::CFormHeader header;
    if (!stalker::readCFormHeader(utf8_path, header)) return -2;
    if (!header.vertexCount || !header.faceCount) return -2;
    std::ifstream file(utf8_path, std::ios::binary);
    if (!file) return -2;
    const uint64_t facesOffset = uint64_t(36) + uint64_t(header.vertexCount) * 12;
    // Sample evenly across the collision face table instead of loading the full map.
    const uint64_t stride = std::max(uint64_t(1),
        (uint64_t(header.faceCount) + triangle_capacity - 1) / triangle_capacity);
    std::unordered_map<uint32_t, uint32_t> remap;
    remap.reserve(vertex_capacity);
    uint32_t outputFaces = 0;
    uint32_t outputVertices = 0;
    for (uint64_t face = 0; face < header.faceCount && outputFaces < triangle_capacity; face += stride) {
        unsigned char record[16];
        file.seekg(static_cast<std::streamoff>(facesOffset + face * 16), std::ios::beg);
        if (!file.read(reinterpret_cast<char*>(record), 16)) return -2;
        const uint32_t original[3] = {le32(record), le32(record + 4), le32(record + 8)};
        // Degenerate faces have no drawable area and waste scarce preview capacity.
        if (original[0] == original[1] || original[1] == original[2] ||
            original[0] == original[2]) continue;
        uint32_t mapped[3] = {};
        bool valid = true;
        uint32_t newVertices = 0;
        for (int j = 0; j < 3; ++j) {
            if (original[j] >= header.vertexCount) return -2;
            if (remap.find(original[j]) == remap.end()) {
                bool seen = false;
                for (int k = 0; k < j; ++k) if (original[k] == original[j]) seen = true;
                if (!seen) ++newVertices;
            }
        }
        if (newVertices > vertex_capacity - outputVertices) break;
        for (int j = 0; j < 3; ++j) {
            auto found = remap.find(original[j]);
            if (found != remap.end()) { mapped[j] = found->second; continue; }
            unsigned char position[12];
            file.seekg(static_cast<std::streamoff>(36 + uint64_t(original[j]) * 12), std::ios::beg);
            if (!file.read(reinterpret_cast<char*>(position), 12)) return -2;
            float point[3] = {leFloat(position), leFloat(position + 4), leFloat(position + 8)};
            for (float coordinate : point) if (!std::isfinite(coordinate)) valid = false;
            if (!valid) return -2;
            for (int axis = 0; axis < 3; ++axis) xyz[outputVertices * 3 + axis] = point[axis];
            mapped[j] = outputVertices;
            remap.emplace(original[j], outputVertices++);
        }
        for (int j = 0; j < 3; ++j) indices[outputFaces * 3 + j] = mapped[j];
        ++outputFaces;
    }
    *vertex_count = outputVertices;
    *triangle_count = outputFaces;
    return outputFaces ? 0 : -3;
}
