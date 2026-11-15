#include "STCRC.h"
#include <cstdint>

// Portable CRC-32 (IEEE 802.3). No unresolved external xrCore symbols.
// This is a standalone diagnostic helper, not proof that X-Ray runs.
extern "C" uint32_t st_openxray_crc32(const void *bytes, uint32_t length) {
    if (!bytes && length != 0) return 0;
    const auto *data = static_cast<const uint8_t *>(bytes);
    uint32_t crc = 0xFFFFFFFFu;
    for (uint32_t i = 0; i < length; ++i) {
        crc ^= data[i];
        for (unsigned bit = 0; bit < 8; ++bit) {
            crc = (crc >> 1) ^ ((crc & 1u) ? 0xEDB88320u : 0u);
        }
    }
    return crc ^ 0xFFFFFFFFu;
}
