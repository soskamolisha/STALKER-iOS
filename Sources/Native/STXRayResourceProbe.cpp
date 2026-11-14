#include "XRayBootstrap.hpp"
#include "XRayChunkIndex.hpp"
#include <vector>
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

extern "C" int32_t st_xray_probe_first_archive_chunks(const char* utf8_path,
                                                        uint32_t* chunk_count) {
    if (!utf8_path || !chunk_count) return -1;
    *chunk_count = 0;
    stalker::XRayBootstrap bootstrap;
    if (!bootstrap.initialize(stalker::GameEdition::shadow) ||
        !bootstrap.attachResources(utf8_path)) return -3;
    if (bootstrap.packedArchiveNames().empty()) return -2;
    const std::string archive = bootstrap.resourceRoot() + "/" +
                                bootstrap.packedArchiveNames().front();
    std::vector<stalker::ChunkEntry> entries;
    if (!stalker::indexXRayChunks(archive, entries, 65536)) return -4;
    *chunk_count = static_cast<uint32_t>(entries.size());
    return 0;
}
