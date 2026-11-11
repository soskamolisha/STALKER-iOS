#include "STXRayBootstrap.h"
#include "XRayBootstrap.hpp"
#include "XRayLevelInspection.hpp"
#include "XRayCFormHeader.hpp"
#include <new>
#include <cstring>
#include <limits>
#include <vector>
#include <fcntl.h>
#include <unistd.h>
#include <cerrno>
#include <sys/stat.h>

using stalker::XRayBootstrap;
extern "C" {
STXRayHandle st_xray_create(void) {
    return new (std::nothrow) XRayBootstrap();
}
void st_xray_destroy(STXRayHandle handle) {
    delete static_cast<XRayBootstrap*>(handle);
}
int32_t st_xray_initialize(STXRayHandle handle, int32_t edition) {
    if (!handle || edition < 0 || edition > 2) return -1;
    return static_cast<XRayBootstrap*>(handle)->initialize(
        static_cast<stalker::GameEdition>(edition)) ? 0 : -2;
}
int32_t st_xray_attach_resources(STXRayHandle handle, const char* utf8_path) {
    if (!handle || !utf8_path) return -1;
    return static_cast<XRayBootstrap*>(handle)->attachResources(utf8_path) ? 0 : -2;
}
int32_t st_xray_state(STXRayHandle handle) {
    if (!handle) return -1;
    return static_cast<int32_t>(static_cast<XRayBootstrap*>(handle)->state());
}
int32_t st_xray_has_loose_data(STXRayHandle handle) {
    return handle && static_cast<XRayBootstrap*>(handle)->hasLooseGameData() ? 1 : 0;
}
int32_t st_xray_has_packed_data(STXRayHandle handle) {
    return handle && static_cast<XRayBootstrap*>(handle)->hasPackedGameData() ? 1 : 0;
}
int32_t st_xray_archive_count(STXRayHandle handle) {
    return handle ? static_cast<int32_t>(static_cast<XRayBootstrap*>(handle)->packedArchiveCount()) : -1;
}
uint64_t st_xray_archive_bytes(STXRayHandle handle) {
    return handle ? static_cast<XRayBootstrap*>(handle)->packedArchiveBytes() : 0;
}
int32_t st_xray_archive_name(STXRayHandle handle, uint32_t index, char* destination, uint32_t capacity) {
    if (!handle || !destination || capacity == 0) return -1;
    const auto& names = static_cast<XRayBootstrap*>(handle)->packedArchiveNames();
    if (index >= names.size()) return -2;
    const auto& name = names[index];
    if (name.size() + 1 > capacity) return -3;
    std::memcpy(destination, name.c_str(), name.size() + 1);
    return 0;
}
int32_t st_xray_archive_read(STXRayHandle handle, uint32_t index, uint64_t offset,
                             void* destination, uint32_t size) {
    if (!handle || (!destination && size != 0) || size > 65536) return -1;
    const auto* engine = static_cast<XRayBootstrap*>(handle);
    if (engine->state() != stalker::EngineState::resourcesReady) return -2;
    const auto& names = engine->packedArchiveNames();
    if (index >= names.size()) return -2;
    const std::string path = engine->resourceRoot() + "/" + names[index];
    const int fd = open(path.c_str(), O_RDONLY | O_NOFOLLOW | O_CLOEXEC);
    if (fd < 0) return -3;
    struct stat info {};
    if (fstat(fd, &info) != 0 || !S_ISREG(info.st_mode) || info.st_size < 0) {
        close(fd);
        return -3;
    }
    const auto length = static_cast<uint64_t>(info.st_size);
    if (offset > length || size > length - offset ||
        offset > static_cast<uint64_t>(std::numeric_limits<off_t>::max())) {
        close(fd);
        return -3;
    }
    uint32_t done = 0;
    while (done < size) {
        const ssize_t count = pread(fd, static_cast<char*>(destination) + done,
                                    size - done, static_cast<off_t>(offset + done));
        if (count < 0 && errno == EINTR) continue;
        if (count <= 0) { close(fd); return -3; }
        done += static_cast<uint32_t>(count);
    }
    close(fd);
    return 0;
}
int32_t st_xray_archive_chunk_info(STXRayHandle handle, uint32_t archive_index,
                                   uint32_t chunk_index, uint32_t* chunk_id,
                                   uint64_t* payload_offset, uint32_t* payload_size,
                                   int32_t* compressed) {
    if (!chunk_id || !payload_offset || !payload_size || !compressed) return -1;
    if (!handle || chunk_index >= 4096) return -1;
    const auto* engine = static_cast<XRayBootstrap*>(handle);
    if (engine->state() != stalker::EngineState::resourcesReady ||
        archive_index >= engine->packedArchiveNames().size()) return -2;
    const std::string path = engine->resourceRoot() + "/" + engine->packedArchiveNames()[archive_index];
    const int fd = open(path.c_str(), O_RDONLY | O_NOFOLLOW | O_CLOEXEC);
    if (fd < 0) return -3;
    struct stat info {};
    if (fstat(fd, &info) != 0 || !S_ISREG(info.st_mode) || info.st_size < 0) {
        close(fd); return -3;
    }
    const uint64_t length = static_cast<uint64_t>(info.st_size);
    uint64_t offset = 0;
    for (uint32_t i = 0; i <= chunk_index; ++i) {
        if (offset == length) { close(fd); return -2; }
        if (offset > length || length - offset < 8 ||
            offset > static_cast<uint64_t>(std::numeric_limits<off_t>::max())) {
            close(fd); return -3;
        }
        unsigned char header[8] {};
        const ssize_t count = pread(fd, header, sizeof(header), static_cast<off_t>(offset));
        if (count != 8) { close(fd); return -3; }
        auto le32 = [](const unsigned char* p) -> uint32_t {
            return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) |
                   (static_cast<uint32_t>(p[2]) << 16) | (static_cast<uint32_t>(p[3]) << 24);
        };
        const uint32_t raw_id = le32(header);
        const uint32_t size = le32(header + 4);
        if (static_cast<uint64_t>(size) > length - offset - 8) { close(fd); return -3; }
        if (i == chunk_index) {
            *chunk_id = raw_id & 0x7fffffffU;
            *compressed = (raw_id & 0x80000000U) ? 1 : 0;
            *payload_offset = offset + 8;
            *payload_size = size;
            close(fd);
            return 0;
        }
        offset += 8 + static_cast<uint64_t>(size);
    }
    close(fd);
    return -2;
}
int32_t st_xray_archive_chunk_read(STXRayHandle handle, uint32_t archive_index,
                                   uint32_t chunk_index, uint32_t relative_offset,
                                   void* destination, uint32_t size) {
    if ((!destination && size != 0) || size > 65536) return -1;
    uint32_t chunk_id = 0, payload_size = 0;
    uint64_t payload_offset = 0;
    int32_t compressed = 0;
    const int32_t status = st_xray_archive_chunk_info(handle, archive_index, chunk_index,
                                                       &chunk_id, &payload_offset,
                                                       &payload_size, &compressed);
    if (status != 0) return status;
    if (relative_offset > payload_size || size > payload_size - relative_offset) return -3;
    return st_xray_archive_read(handle, archive_index, payload_offset + relative_offset,
                                destination, size);
}
int32_t st_xray_archive_directory_decompress(STXRayHandle handle, uint32_t archive_index,
                                               void* destination, uint32_t capacity,
                                               uint32_t expected_size) {
    if (!handle || !destination || expected_size == 0 ||
        expected_size > capacity || expected_size > 64u * 1024u * 1024u) return -1;
    uint32_t chunk_id = 0, chunk_size = 0;
    uint64_t payload_offset = 0;
    int32_t compressed = 0;
    for (uint32_t i = 0; i < 64; ++i) {
        const int32_t result = st_xray_archive_chunk_info(
            handle, archive_index, i, &chunk_id, &payload_offset, &chunk_size, &compressed);
        if (result != 0) return result;
        if (chunk_id != 1) continue;
        if (!compressed) return -4;
        if (chunk_size == 0 || chunk_size > 64u * 1024u * 1024u) return -3;
        std::vector<unsigned char> bytes(chunk_size);
        uint32_t cursor = 0;
        while (cursor < chunk_size) {
            const uint32_t amount = chunk_size - cursor > 65536u ? 65536u : chunk_size - cursor;
            const int32_t read_result = st_xray_archive_read(
                handle, archive_index, payload_offset + cursor, bytes.data() + cursor, amount);
            if (read_result != 0) return read_result;
            cursor += amount;
        }
        return st_xray_lzo_decompress(bytes.data(), chunk_size,
                                      destination, capacity, expected_size);
    }
    return -2;
}
int32_t st_xray_archive_directory_count(STXRayHandle handle, uint32_t archive_index) {
    if (!handle) return -1;
    uint32_t chunk_id = 0, chunk_size = 0;
    uint64_t payload_offset = 0;
    int32_t compressed = 0;
    for (uint32_t chunk_index = 0; chunk_index < 64; ++chunk_index) {
        const int32_t result = st_xray_archive_chunk_info(handle, archive_index, chunk_index,
                                                           &chunk_id, &payload_offset,
                                                           &chunk_size, &compressed);
        if (result != 0) return result;
        if (chunk_id != 1) continue;
        if (compressed) return -4;
        uint32_t cursor = 0;
        int32_t count = 0;
        while (cursor < chunk_size) {
            if (chunk_size - cursor < 2 || count >= 65536) return -3;
            unsigned char length_bytes[2] {};
            if (st_xray_archive_chunk_read(handle, archive_index, chunk_index,
                                           cursor, length_bytes, 2) != 0) return -3;
            const uint32_t length = static_cast<uint32_t>(length_bytes[0]) |
                                    (static_cast<uint32_t>(length_bytes[1]) << 8);
            if (length < 17 || length > 4096 || length + 2 > chunk_size - cursor) return -3;
            cursor += length + 2;
            ++count;
        }
        return count;
    }
    return -2;
}
int32_t st_xray_archive_directory_entry(STXRayHandle handle, uint32_t archive_index,
                                        uint32_t entry_index, char* name, uint32_t name_capacity,
                                        uint32_t* size_real, uint32_t* size_compressed,
                                        uint32_t* crc, uint32_t* data_offset) {
    if (!name || name_capacity == 0 || !size_real || !size_compressed || !crc ||
        !data_offset || entry_index >= 65536) return -1;
    uint32_t chunk_id = 0, chunk_size = 0;
    uint64_t payload_offset = 0;
    int32_t compressed = 0;
    // X-Ray archive directory is chunk ID 1, not necessarily the first chunk.
    for (uint32_t chunk_index = 0; chunk_index < 64; ++chunk_index) {
        const int32_t result = st_xray_archive_chunk_info(handle, archive_index, chunk_index,
                                                           &chunk_id, &payload_offset,
                                                           &chunk_size, &compressed);
        if (result != 0) return result;
        if (chunk_id != 1) continue;
        if (compressed) return -4;
        uint32_t cursor = 0;
        for (uint32_t i = 0; i <= entry_index; ++i) {
            if (cursor == chunk_size) return -2;
            if (chunk_size - cursor < 2) return -3;
            unsigned char length_bytes[2] {};
            if (st_xray_archive_chunk_read(handle, archive_index, chunk_index,
                                           cursor, length_bytes, 2) != 0) return -3;
            const uint32_t length = static_cast<uint32_t>(length_bytes[0]) |
                                    (static_cast<uint32_t>(length_bytes[1]) << 8);
            // 16 fixed bytes: real size, compressed size, CRC32 and data offset.
            if (length < 16 || length > 4096 || length + 2 > chunk_size - cursor) return -3;
            if (i == entry_index) {
                const uint32_t name_length = length - 16;
                if (name_length == 0 || name_length + 1 > name_capacity) return -3;
                unsigned char fixed[12] {};
                if (st_xray_archive_chunk_read(handle, archive_index, chunk_index,
                                               cursor + 2, fixed, 12) != 0) return -3;
                if (st_xray_archive_chunk_read(handle, archive_index, chunk_index,
                                               cursor + 14, name, name_length) != 0) return -3;
                name[name_length] = 0;
                if (std::memchr(name, 0, name_length) != nullptr) return -3;
                // Archive names are relative game paths. Never accept absolute
                // paths or parent-directory components before exposing entries.
                if (name[0] == '/' || name[0] == '\\') return -3;
                for (uint32_t j = 0; j < name_length; ++j) {
                    const char ch = name[j];
                    if (ch == ':' || static_cast<unsigned char>(ch) < 32) return -3;
                    if (ch == '/' || ch == '\\') {
                        if (j == 0 || j + 1 == name_length) return -3;
                    }
                    const bool segment_start = j == 0 || name[j - 1] == '/' || name[j - 1] == '\\';
                    if (segment_start && ch == '.' &&
                        (j + 1 == name_length || name[j + 1] == '/' || name[j + 1] == '\\' ||
                         (j + 1 < name_length && name[j + 1] == '.' &&
                          (j + 2 == name_length || name[j + 2] == '/' || name[j + 2] == '\\'))))
                        return -3;
                }
                unsigned char pointer_bytes[4] {};
                if (st_xray_archive_chunk_read(handle, archive_index, chunk_index,
                                               cursor + 14 + name_length,
                                               pointer_bytes, 4) != 0) return -3;
                auto le32 = [](const unsigned char* p) -> uint32_t {
                    return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) |
                           (static_cast<uint32_t>(p[2]) << 16) | (static_cast<uint32_t>(p[3]) << 24);
                };
                *size_real = le32(fixed);
                *size_compressed = le32(fixed + 4);
                *crc = le32(fixed + 8);
                *data_offset = le32(pointer_bytes);
                return 0;
            }
            cursor += 2 + length;
        }
    }
    return -2;
}
int32_t st_xray_archive_file_read(STXRayHandle handle, uint32_t archive_index,
                                  uint32_t entry_index, uint32_t relative_offset,
                                  void* destination, uint32_t size) {
    if ((!destination && size != 0) || size > 65536) return -1;
    char name[4097] {};
    uint32_t real_size = 0, compressed_size = 0, crc = 0, data_offset = 0;
    const int32_t status = st_xray_archive_directory_entry(handle, archive_index, entry_index,
                                                            name, sizeof(name), &real_size,
                                                            &compressed_size, &crc, &data_offset);
    if (status != 0) return status;
    if (real_size != compressed_size) return -4;
    // File data must not overlap the archive header or its directory record.
    // The current reader supports only archives with a valid in-file data region.
    if (relative_offset > real_size || size > real_size - relative_offset) return -3;
    if (size != 0 && data_offset < 8) return -3;
    // Reject records pointing into the archive directory chunk itself.
    // The chunk header is eight bytes immediately before its payload.
    uint32_t chunk_id = 0, chunk_size = 0;
    uint64_t chunk_offset = 0;
    int32_t chunk_compressed = 0;
    for (uint32_t i = 0; i < 64; ++i) {
        const int32_t result = st_xray_archive_chunk_info(handle, archive_index, i,
                                                           &chunk_id, &chunk_offset,
                                                           &chunk_size, &chunk_compressed);
        if (result == -2) break;
        if (result != 0) return result;
        if (chunk_id == 1) {
            const uint64_t file_begin = static_cast<uint64_t>(data_offset) + relative_offset;
            const uint64_t file_end = file_begin + size;
            const uint64_t directory_begin = chunk_offset - 8;
            const uint64_t directory_end = chunk_offset + chunk_size;
            if (size != 0 && file_begin < directory_end && file_end > directory_begin) return -3;
            break;
        }
    }
    return st_xray_archive_read(handle, archive_index,
                                static_cast<uint64_t>(data_offset) + relative_offset,
                                destination, size);
}
int32_t st_xray_archive_file_decompress(STXRayHandle handle, uint32_t archive_index,
                                         uint32_t entry_index, void* destination,
                                         uint32_t destination_capacity) {
    if (!handle || !destination) return -1;
    char name[4097] {};
    uint32_t real_size = 0, compressed_size = 0, crc = 0, data_offset = 0;
    const int32_t status = st_xray_archive_directory_entry(
        handle, archive_index, entry_index, name, sizeof(name),
        &real_size, &compressed_size, &crc, &data_offset);
    if (status != 0) return status;
    if (real_size == compressed_size) return -4;
    if (real_size == 0 || compressed_size == 0 ||
        real_size > 64u * 1024u * 1024u ||
        compressed_size > 64u * 1024u * 1024u ||
        real_size > destination_capacity || data_offset < 8) return -3;
    // Validate that the compressed file does not point into directory metadata.
    uint32_t chunk_id = 0, chunk_size = 0;
    uint64_t chunk_offset = 0;
    int32_t chunk_compressed = 0;
    bool found_directory = false;
    for (uint32_t i = 0; i < 64; ++i) {
        const int32_t chunk_result = st_xray_archive_chunk_info(
            handle, archive_index, i, &chunk_id, &chunk_offset, &chunk_size, &chunk_compressed);
        if (chunk_result == -2) break;
        if (chunk_result != 0) return chunk_result;
        if (chunk_id == 1) {
            found_directory = true;
            const uint64_t file_begin = data_offset;
            const uint64_t file_end = file_begin + compressed_size;
            if (file_begin < chunk_offset + chunk_size &&
                file_end > chunk_offset - 8) return -3;
            break;
        }
    }
    if (!found_directory) return -2;
    std::vector<unsigned char> compressed_bytes(compressed_size);
    uint32_t cursor = 0;
    while (cursor < compressed_size) {
        const uint32_t amount = (compressed_size - cursor > 65536u)
                                    ? 65536u : compressed_size - cursor;
        const int32_t read_result = st_xray_archive_read(
            handle, archive_index, static_cast<uint64_t>(data_offset) + cursor,
            compressed_bytes.data() + cursor, amount);
        if (read_result != 0) return read_result;
        cursor += amount;
    }
    return st_xray_lzo_decompress(compressed_bytes.data(), compressed_size,
                                  destination, destination_capacity, real_size);
}
int32_t st_xray_level_count(STXRayHandle handle) {
    if (!handle) return -1;
    return static_cast<int32_t>(static_cast<XRayBootstrap*>(handle)->looseLevels().size());
}
int32_t st_xray_level_name(STXRayHandle handle, uint32_t index, char* destination, uint32_t capacity) {
    if (!handle || !destination || capacity == 0) return -1;
    const auto& levels = static_cast<XRayBootstrap*>(handle)->looseLevels();
    if (index >= levels.size()) return -2;
    const std::string& name = levels[index];
    if (name.size() + 1 > capacity) return -3;
    std::memcpy(destination, name.c_str(), name.size() + 1);
    return 0;
}
int32_t st_xray_level_chunk_count(STXRayHandle handle, uint32_t level_index) {
    if (!handle) return -1;
    const auto* engine = static_cast<XRayBootstrap*>(handle);
    const auto& levels = engine->looseLevels();
    if (level_index >= levels.size()) return -1;
    const auto result = stalker::inspectLevel(engine->resourceRoot(), levels[level_index]);
    if (!result.geometryIndexed) return -2;
    return static_cast<int32_t>(result.geometryChunks.size());
}
int32_t st_xray_level_chunk_info(STXRayHandle handle, uint32_t level_index,
                                 uint32_t chunk_index, uint32_t* chunk_id,
                                 uint64_t* payload_offset, uint32_t* payload_size,
                                 int32_t* compressed) {
    if (!handle || !chunk_id || !payload_offset || !payload_size || !compressed) return -1;
    const auto* engine = static_cast<XRayBootstrap*>(handle);
    const auto& levels = engine->looseLevels();
    if (level_index >= levels.size()) return -1;
    const auto result = stalker::inspectLevel(engine->resourceRoot(), levels[level_index]);
    if (!result.geometryIndexed) return -2;
    if (chunk_index >= result.geometryChunks.size()) return -1;
    const auto& chunk = result.geometryChunks[chunk_index];
    *chunk_id = chunk.id;
    *payload_offset = chunk.offset;
    *payload_size = chunk.size;
    *compressed = chunk.compressed ? 1 : 0;
    return 0;
}
int32_t st_xray_level_collision_info(STXRayHandle handle, uint32_t level_index,
                                      uint32_t* vertex_count, uint32_t* triangle_count,
                                      float* bounds_min_xyz, float* bounds_max_xyz) {
    if (!handle || !vertex_count || !triangle_count || !bounds_min_xyz || !bounds_max_xyz)
        return -1;
    const auto* engine = static_cast<XRayBootstrap*>(handle);
    const auto& levels = engine->looseLevels();
    if (level_index >= levels.size()) return -1;
    stalker::CFormHeader header;
    const auto path = engine->resourceRoot() + "/gamedata/levels/" +
                      levels[level_index] + "/level.cform";
    if (!stalker::readCFormHeader(path, header)) return -2;
    *vertex_count = header.vertexCount;
    *triangle_count = header.faceCount;
    for (int i = 0; i < 3; ++i) {
        bounds_min_xyz[i] = header.minimum[i];
        bounds_max_xyz[i] = header.maximum[i];
    }
    return 0;
}
void st_xray_shutdown(STXRayHandle handle) {
    if (handle) static_cast<XRayBootstrap*>(handle)->shutdown();
}
}
