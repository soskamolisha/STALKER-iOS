#include "XRayBootstrap.hpp"
#include <cstdint>

// Resource inspection only. This is NOT a game runtime.
extern "C" int32_t st_xray_probe_shadow_resources(const char* utf8_path,
                                                      uint32_t* archives,
                                                      uint64_t* archive_bytes,
                                                      uint32_t* loose_levels) {
    if (!utf8_path || !archives || !archive_bytes || !loose_levels) return -1;
    *archives = 0;
    *archive_bytes = 0;
    *loose_levels = 0;
    stalker::XRayBootstrap bootstrap;
    if (!bootstrap.initialize(stalker::GameEdition::shadow)) return -2;
    if (!bootstrap.attachResources(utf8_path)) return -3;
    *archives = bootstrap.packedArchiveCount();
    *archive_bytes = bootstrap.packedArchiveBytes();
    *loose_levels = static_cast<uint32_t>(bootstrap.looseLevels().size());
    return 0;
}
