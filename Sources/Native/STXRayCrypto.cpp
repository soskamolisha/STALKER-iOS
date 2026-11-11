#include "STXRayBootstrap.h"
#include <array>
#include <algorithm>
#include <cstdint>

// Compatible with upstream OpenXRay xrCore/Crypto/trivial_encryptor.cpp
// and xrCore/Math/Random32.hpp (pinned upstream source).
namespace {
struct Random32 {
    uint32_t seed;
    uint32_t next(uint32_t range) {
        seed = 0x08088405u * seed + 1u;
        return static_cast<uint32_t>((static_cast<uint64_t>(seed) * range) >> 32u);
    }
};
}

extern "C" int32_t st_xray_directory_decrypt(void* data, uint32_t size, int32_t key_variant) {
    if ((!data && size != 0) || (key_variant != 0 && key_variant != 1)) return -1;
    if (size > 64u * 1024u * 1024u) return -3;
    const uint32_t iterations = key_variant == 0 ? 1024u : 2048u;
    const uint32_t table_seed = key_variant == 0 ? 6011979u : 20091958u;
    const uint32_t encrypt_seed = key_variant == 0 ? 24031979u : 20031955u;
    std::array<uint8_t, 256> alphabet {};
    std::array<uint8_t, 256> reverse {};
    for (uint32_t i = 0; i < 256; ++i) alphabet[i] = static_cast<uint8_t>(i);
    Random32 shuffle {table_seed};
    for (uint32_t i = 0; i < iterations; ++i) {
        uint32_t j = shuffle.next(256);
        uint32_t k = shuffle.next(256);
        while (j == k) k = shuffle.next(256);
        std::swap(alphabet[j], alphabet[k]);
    }
    for (uint32_t i = 0; i < 256; ++i) reverse[alphabet[i]] = static_cast<uint8_t>(i);
    Random32 stream {encrypt_seed};
    auto* bytes = static_cast<uint8_t*>(data);
    for (uint32_t i = 0; i < size; ++i)
        bytes[i] = reverse[bytes[i] ^ static_cast<uint8_t>(stream.next(256) & 0xffu)];
    return 0;
}
