#pragma once
#include "XRayCFormHeader.hpp"
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace stalker {
struct CFormVertex { float x = 0, y = 0, z = 0; };
struct CFormTriangle { std::uint32_t a = 0, b = 0, c = 0; };
// Reads only a bounded prefix of vertices and triangle indices.
// X-Ray triangle records contain an additional 4-byte metadata field.
bool readCFormGeometry(const std::string& path, CFormHeader& header,
                       std::vector<CFormVertex>& vertices,
                       std::vector<CFormTriangle>& triangles,
                       std::size_t maxVertices = 100000,
                       std::size_t maxTriangles = 100000);
}
