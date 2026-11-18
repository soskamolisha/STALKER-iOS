#include "XRayCFormHeader.hpp"
#include <cmath>
#include <cstring>
#include <fstream>
#include <sys/stat.h>

namespace stalker {
namespace {
std::uint32_t le32(const unsigned char* p) {
    return std::uint32_t(p[0]) | (std::uint32_t(p[1]) << 8) |
           (std::uint32_t(p[2]) << 16) | (std::uint32_t(p[3]) << 24);
}
float leFloat(const unsigned char* p) {
    const std::uint32_t bits = le32(p);
    float result;
    static_assert(sizeof(result) == sizeof(bits), "Requires 32-bit floats");
    std::memcpy(&result, &bits, sizeof(result));
    return result;
}
}
bool readCFormHeader(const std::string& path, CFormHeader& result) {
    result = {};
    struct stat st {};
    if (lstat(path.c_str(), &st) != 0 || !S_ISREG(st.st_mode) || st.st_size < 36)
        return false;
    std::ifstream file(path, std::ios::binary);
    unsigned char bytes[36] = {};
    if (!file.read(reinterpret_cast<char*>(bytes), sizeof(bytes))) return false;
    CFormHeader parsed;
    parsed.version = le32(bytes);
    parsed.vertexCount = le32(bytes + 4);
    parsed.faceCount = le32(bytes + 8);
    for (int axis = 0; axis < 3; ++axis) {
        parsed.minimum[axis] = leFloat(bytes + 12 + axis * 4);
        parsed.maximum[axis] = leFloat(bytes + 24 + axis * 4);
        if (!std::isfinite(parsed.minimum[axis]) || !std::isfinite(parsed.maximum[axis]) ||
            parsed.minimum[axis] > parsed.maximum[axis]) return false;
    }
    // Common X-Ray collision format: 12 bytes per vertex, 16 per triangle.
    // Only enforce that enough bytes exist; format-specific validation comes later.
    const auto minimumSize = std::uint64_t(36) +
        std::uint64_t(parsed.vertexCount) * 12 +
        std::uint64_t(parsed.faceCount) * 16;
    if (minimumSize > static_cast<std::uint64_t>(st.st_size)) return false;
    result = parsed;
    return true;
}
}
