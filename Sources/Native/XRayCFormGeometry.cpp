#include "XRayCFormGeometry.hpp"
#include <cmath>
#include <cstring>
#include <fstream>
#include <limits>

namespace stalker {
namespace {
std::uint32_t u32(const unsigned char* p) {
    return std::uint32_t(p[0]) | (std::uint32_t(p[1]) << 8) |
           (std::uint32_t(p[2]) << 16) | (std::uint32_t(p[3]) << 24);
}
float f32(const unsigned char* p) {
    auto bits = u32(p);
    float value;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}
}
bool readCFormGeometry(const std::string& path, CFormHeader& header,
                       std::vector<CFormVertex>& vertices,
                       std::vector<CFormTriangle>& triangles,
                       std::size_t maxVertices, std::size_t maxTriangles) {
    vertices.clear();
    triangles.clear();
    header = {};
    if (!readCFormHeader(path, header)) return false;
    if (header.vertexCount > maxVertices || header.faceCount > maxTriangles) return false;
    std::ifstream input(path, std::ios::binary);
    if (!input.seekg(36, std::ios::beg)) return false;
    std::vector<CFormVertex> parsedVertices;
    std::vector<CFormTriangle> parsedTriangles;
    parsedVertices.reserve(header.vertexCount);
    parsedTriangles.reserve(header.faceCount);
    unsigned char record[16];
    for (std::uint32_t i = 0; i < header.vertexCount; ++i) {
        if (!input.read(reinterpret_cast<char*>(record), 12)) return false;
        CFormVertex vertex{f32(record), f32(record + 4), f32(record + 8)};
        if (!std::isfinite(vertex.x) || !std::isfinite(vertex.y) || !std::isfinite(vertex.z))
            return false;
        parsedVertices.push_back(vertex);
    }
    for (std::uint32_t i = 0; i < header.faceCount; ++i) {
        if (!input.read(reinterpret_cast<char*>(record), 16)) return false;
        CFormTriangle triangle{u32(record), u32(record + 4), u32(record + 8)};
        if (triangle.a >= header.vertexCount || triangle.b >= header.vertexCount ||
            triangle.c >= header.vertexCount) return false;
        parsedTriangles.push_back(triangle);
    }
    vertices.swap(parsedVertices);
    triangles.swap(parsedTriangles);
    return true;
}
}
