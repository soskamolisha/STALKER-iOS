#include "XRayBootstrap.hpp"
#include "XRayChunkIndex.hpp"
#include "XRayChunkPayload.hpp"
#include <cstring>
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

/* Inspect one indexed top-level chunk without loading its payload.
   -2: chunk index out of range; -3: resources unavailable; -4: malformed archive. */
extern "C" int32_t st_xray_probe_first_archive_chunk_info(const char* utf8_path,
                                                             uint32_t chunk_index,
                                                             uint32_t* chunk_id,
                                                             uint32_t* payload_size,
                                                             int32_t* compressed) {
    if (!utf8_path || !chunk_id || !payload_size || !compressed) return -1;
    *chunk_id = 0;
    *payload_size = 0;
    *compressed = 0;
    stalker::XRayBootstrap bootstrap;
    if (!bootstrap.initialize(stalker::GameEdition::shadow) ||
        !bootstrap.attachResources(utf8_path)) return -3;
    if (bootstrap.packedArchiveNames().empty()) return -3;
    const std::string archive = bootstrap.resourceRoot() + "/" +
                                bootstrap.packedArchiveNames().front();
    std::vector<stalker::ChunkEntry> entries;
    if (!stalker::indexXRayChunks(archive, entries, 65536)) return -4;
    if (chunk_index >= entries.size()) return -2;
    const auto& entry = entries[chunk_index];
    *chunk_id = entry.id;
    *payload_size = entry.size;
    *compressed = entry.compressed ? 1 : 0;
    return 0;
}

/* Read a bounded uncompressed X-Ray chunk into a caller-owned buffer.
   Compressed chunks deliberately require a separate decoder. */
extern "C" int32_t st_xray_probe_first_archive_chunk_read(const char* utf8_path,
                                                             uint32_t chunk_index,
                                                             void* destination,
                                                             uint32_t capacity,
                                                             uint32_t* actual_size) {
    if (!utf8_path || !actual_size || (!destination && capacity)) return -1;
    *actual_size = 0;
    stalker::XRayBootstrap bootstrap;
    if (!bootstrap.initialize(stalker::GameEdition::shadow) ||
        !bootstrap.attachResources(utf8_path) ||
        bootstrap.packedArchiveNames().empty()) return -3;
    const std::string archive = bootstrap.resourceRoot() + "/" +
                                bootstrap.packedArchiveNames().front();
    std::vector<stalker::ChunkEntry> entries;
    if (!stalker::indexXRayChunks(archive, entries, 65536)) return -4;
    if (chunk_index >= entries.size()) return -2;
    const auto& entry = entries[chunk_index];
    if (entry.compressed) return -5;
    if (entry.size > capacity || entry.size > 65536) return -6;
    std::vector<std::uint8_t> payload;
    if (!stalker::readChunkPayload(archive, entry, payload, 65536)) return -4;
    if (!payload.empty()) std::memcpy(destination, payload.data(), payload.size());
    *actual_size = static_cast<uint32_t>(payload.size());
    return 0;
}
