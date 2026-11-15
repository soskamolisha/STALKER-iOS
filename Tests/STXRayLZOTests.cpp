#include "STXRayBootstrap.h"
#include "Upstream/minilzo.h"
#include <cassert>
#include <cstdint>
#include <cstring>
#include <vector>

int main() {
    assert(lzo_init() == LZO_E_OK);
    const char original[] = "STALKER X-Ray archive directory test: gamedata/configs/system.ltx";
    const lzo_uint originalSize = static_cast<lzo_uint>(sizeof(original) - 1);
    std::vector<unsigned char> work(LZO1X_1_MEM_COMPRESS);
    std::vector<unsigned char> compressed(originalSize + originalSize / 16 + 128);
    lzo_uint compressedSize = static_cast<lzo_uint>(compressed.size());
    assert(lzo1x_1_compress(reinterpret_cast<const unsigned char*>(original),
                           originalSize, compressed.data(), &compressedSize,
                           work.data()) == LZO_E_OK);
    char decoded[sizeof(original)] = {};
    assert(st_xray_lzo_decompress(compressed.data(),
                                  static_cast<uint32_t>(compressedSize),
                                  decoded, sizeof(decoded), originalSize) == 0);
    assert(std::memcmp(decoded, original, originalSize) == 0);
    assert(st_xray_lzo_decompress(compressed.data(),
                                  static_cast<uint32_t>(compressedSize),
                                  decoded, originalSize - 1, originalSize) == -3);
    assert(st_xray_lzo_decompress(compressed.data(),
                                  static_cast<uint32_t>(compressedSize),
                                  decoded, sizeof(decoded), originalSize + 1) != 0);
    assert(st_xray_lzo_decompress(compressed.data(), 1,
                                  decoded, sizeof(decoded), originalSize) != 0);
    return 0;
}
