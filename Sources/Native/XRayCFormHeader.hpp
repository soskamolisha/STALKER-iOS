#pragma once
#include <cstdint>
#include <string>

namespace stalker {
struct CFormHeader {
    std::uint32_t version = 0;
    std::uint32_t vertexCount = 0;
    std::uint32_t faceCount = 0;
    float minimum[3] = {};
    float maximum[3] = {};
};
// Reads the fixed 36-byte collision header; does not decode vertices or faces.
// Validates finite bounding box values and payload minimum size.
bool readCFormHeader(const std::string& path, CFormHeader& result);
}
