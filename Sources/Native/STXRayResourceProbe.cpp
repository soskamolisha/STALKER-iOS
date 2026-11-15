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

/* CRC32 of a bounded, uncompressed chunk using the linked upstream xrCore utility.
   This validates that the archive payload can flow through real OpenXRay code. */
extern "C" uint32_t st_openxray_crc32(const void*, uint32_t);
extern "C" int32_t st_xray_probe_first_archive_chunk_crc32(const char* utf8_path,
                                                              uint32_t chunk_index,
                                                              uint32_t* crc) {
    if (!crc) return -1;
    *crc = 0;
    std::uint8_t payload[65536] = {};
    uint32_t size = 0;
    const int32_t result = st_xray_probe_first_archive_chunk_read(
        utf8_path, chunk_index, payload, sizeof(payload), &size);
    if (result != 0) return result;
    *crc = st_openxray_crc32(payload, size);
    return 0;
}

/* Find X-Ray's directory chunk (ID 1) without decoding or trusting its payload. */
extern "C" int32_t st_xray_probe_first_archive_directory(const char* utf8_path,
                                                            uint32_t* chunk_index,
                                                            uint32_t* payload_size,
                                                            int32_t* compressed) {
    if (!utf8_path || !chunk_index || !payload_size || !compressed) return -1;
    *chunk_index = 0;
    *payload_size = 0;
    *compressed = 0;
    stalker::XRayBootstrap bootstrap;
    if (!bootstrap.initialize(stalker::GameEdition::shadow) ||
        !bootstrap.attachResources(utf8_path) ||
        bootstrap.packedArchiveNames().empty()) return -3;
    const std::string archive = bootstrap.resourceRoot() + "/" +
                                bootstrap.packedArchiveNames().front();
    std::vector<stalker::ChunkEntry> entries;
    if (!stalker::indexXRayChunks(archive, entries, 65536)) return -4;
    for (std::size_t i = 0; i < entries.size(); ++i) {
        if (entries[i].id != 1) continue;
        *chunk_index = static_cast<uint32_t>(i);
        *payload_size = entries[i].size;
        *compressed = entries[i].compressed ? 1 : 0;
        return 0;
    }
    return -2;
}

/* Verify the actual directory chunk with upstream CRC32 when it is uncompressed.
   Compressed directories are reported as -5 until an LZO decoder is linked. */
extern "C" int32_t st_xray_probe_first_archive_directory_crc32(const char* utf8_path,
                                                                  uint32_t* crc) {
    if (!utf8_path || !crc) return -1;
    *crc = 0;
    uint32_t index = 0, size = 0;
    int32_t compressed = 0;
    const int32_t status = st_xray_probe_first_archive_directory(
        utf8_path, &index, &size, &compressed);
    if (status != 0) return status;
    if (compressed) return -5;
    if (size > 65536) return -6;
    return st_xray_probe_first_archive_chunk_crc32(utf8_path, index, crc);
}

/* Extract a bounded, uncompressed directory chunk for a future record parser.
   This deliberately rejects compressed directories until LZO is available. */
extern "C" int32_t st_xray_probe_first_archive_directory_read(const char* utf8_path,
                                                                  void* destination,
                                                                  uint32_t capacity,
                                                                  uint32_t* actual_size) {
    if (!utf8_path || !actual_size || (!destination && capacity)) return -1;
    *actual_size = 0;
    uint32_t index = 0, size = 0;
    int32_t compressed = 0;
    const int32_t status = st_xray_probe_first_archive_directory(
        utf8_path, &index, &size, &compressed);
    if (status != 0) return status;
    if (compressed) return -5;
    if (size > capacity || size > 65536) return -6;
    return st_xray_probe_first_archive_chunk_read(
        utf8_path, index, destination, capacity, actual_size);
}

/* Validate X-Ray directory records in a small uncompressed directory.
   Each record has a 16-bit length, a 12-byte metadata prefix, a path and
   a 4-byte trailer. Do not expose untrusted names without validation. */
extern "C" int32_t st_xray_probe_first_archive_directory_entries(const char* utf8_path,
                                                                     uint32_t* entry_count) {
    if (!utf8_path || !entry_count) return -1;
    *entry_count = 0;
    std::vector<std::uint8_t> data(65536);
    uint32_t size = 0;
    const int32_t result = st_xray_probe_first_archive_directory_read(
        utf8_path, data.data(), static_cast<uint32_t>(data.size()), &size);
    if (result != 0) return result;
    if (size == 0) return -7;
    uint32_t cursor = 0;
    uint32_t verifiedCount = 0;
    while (cursor < size) {
        if (size - cursor < 2) return -7;
        const uint32_t length = static_cast<uint32_t>(data[cursor]) |
                                (static_cast<uint32_t>(data[cursor + 1]) << 8);
        if (length < 17 || length > 4096 || length > size - cursor - 2) return -7;
        const uint32_t nameLength = length - 16;
        const std::uint8_t* name = data.data() + cursor + 14;
        if (nameLength == 0 || name[0] == '/' || name[0] == '\\' ||
            (nameLength >= 2 && name[1] == ':')) return -7;
        for (uint32_t i = 0; i < nameLength; ++i) {
            if (name[i] == 0 || name[i] == '\\') return -7;
            if (name[i] == '/' && (i + 1 == nameLength || name[i + 1] == '/')) return -7;
            if (name[i] == '.' && (i == 0 || name[i - 1] == '/') &&
                (i + 1 == nameLength || name[i + 1] == '/')) return -7;
            if (name[i] == '.' && i + 1 < nameLength && name[i + 1] == '.' &&
                (i == 0 || name[i - 1] == '/') &&
                (i + 2 == nameLength || name[i + 2] == '/')) return -7;
        }
        cursor += length + 2;
        ++verifiedCount;
    }
    *entry_count = verifiedCount;
    return 0;
}

/* Return one validated relative path from a bounded uncompressed directory.
   This revalidates every record before exposing any path to the caller. */
extern "C" int32_t st_xray_probe_first_archive_directory_name(const char* utf8_path,
                                                                  uint32_t wanted_index,
                                                                  char* destination,
                                                                  uint32_t capacity) {
    if (!utf8_path || !destination || capacity == 0) return -1;
    destination[0] = 0;
    // Validate the entire directory before returning even its first name.
    // Otherwise a valid first entry could conceal a corrupt later record.
    uint32_t validatedCount = 0;
    const int32_t validation = st_xray_probe_first_archive_directory_entries(
        utf8_path, &validatedCount);
    if (validation != 0) return validation;
    if (wanted_index >= validatedCount) return -2;
    std::vector<std::uint8_t> data(65536);
    uint32_t size = 0;
    const int32_t result = st_xray_probe_first_archive_directory_read(
        utf8_path, data.data(), static_cast<uint32_t>(data.size()), &size);
    if (result != 0) return result;
    uint32_t cursor = 0, index = 0;
    while (cursor < size) {
        if (size - cursor < 2) return -7;
        const uint32_t length = static_cast<uint32_t>(data[cursor]) |
                                (static_cast<uint32_t>(data[cursor + 1]) << 8);
        if (length < 17 || length > 4096 || length > size - cursor - 2) return -7;
        const uint32_t nameLength = length - 16;
        const std::uint8_t* name = data.data() + cursor + 14;
        if (nameLength == 0 || name[0] == '/' ||
            (nameLength >= 2 && name[1] == ':')) return -7;
        for (uint32_t i = 0; i < nameLength; ++i) {
            if (name[i] == 0 || name[i] == 92) return -7;
            if (name[i] == '/' && (i + 1 == nameLength || name[i + 1] == '/')) return -7;
            if (name[i] == '.' && (i == 0 || name[i - 1] == '/') &&
                (i + 1 == nameLength || name[i + 1] == '/')) return -7;
            if (name[i] == '.' && i + 1 < nameLength && name[i + 1] == '.' &&
                (i == 0 || name[i - 1] == '/') &&
                (i + 2 == nameLength || name[i + 2] == '/')) return -7;
        }
        if (index == wanted_index) {
            if (nameLength >= capacity) return -6;
            std::memcpy(destination, name, nameLength);
            destination[nameLength] = 0;
            return 0;
        }
        cursor += length + 2;
        ++index;
    }
    return -2;
}
