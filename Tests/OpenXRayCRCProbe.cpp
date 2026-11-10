// Tests the real upstream xrCore/crc32.cpp implementation linked separately.
// No game archives or proprietary data are needed.
#include <cstdint>
#include <cstring>

using u32 = std::uint32_t;
extern u32 crc32(const void*, u32);
extern u32 crc32(const void*, u32, u32);
extern u32 path_crc32(const char*, u32);

extern "C" std::uint32_t st_openxray_crc32(const void* bytes, std::uint32_t length) {
    return crc32(bytes, length);
}

#ifdef STALKER_PROBE_TEST_MAIN
int main() {
    const char* s = "123456789";
    if (crc32(s, 9) != 0xcbf43926U) return 1;
    if (crc32("", 0) != 0U) return 2;
    if (crc32(s + 4, 5, crc32(s, 4)) != crc32(s, 9)) return 3;
    const char* slash = "a/b";
    const char* backslash = "a\\b";
    if (path_crc32(slash, static_cast<u32>(std::strlen(slash))) !=
        path_crc32(backslash, static_cast<u32>(std::strlen(backslash)))) return 4;
    if (st_openxray_crc32(s, 9) != 0xcbf43926U) return 5;
    return 0;
}
#endif
