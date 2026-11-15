#include "STXRayBootstrap.h"
#include "XRayBootstrap.hpp"
#include "XRayLevelInspection.hpp"
#include "XRayCFormHeader.hpp"
#include <new>
#include <cstring>
#include <limits>
#include <cmath>
#include <vector>
#include "Upstream/minilzo.h"
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
        size_t received = 0;
        while (received < sizeof(header)) {
            const ssize_t count = pread(fd, header + received, sizeof(header) - received,
                                        static_cast<off_t>(offset + received));
            if (count < 0 && errno == EINTR) continue;
            if (count <= 0) { close(fd); return -3; }
            received += static_cast<size_t>(count);
        }
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
int32_t st_xray_validate_directory_records(const void* bytes, uint32_t length) {
    if (!bytes || length == 0 || length > 64u * 1024u * 1024u) return -3;
    const auto* data = static_cast<const unsigned char*>(bytes);
    uint32_t position = 0;
    int32_t count = 0;
    while (position < length) {
        if (length - position < 2) return -3;
        const uint32_t record_size = static_cast<uint32_t>(data[position]) |
                                     (static_cast<uint32_t>(data[position + 1]) << 8);
        if (record_size < 17 || record_size > 4096 ||
            record_size > length - position - 2) return -3;
        const uint32_t name_size = record_size - 16;
        const auto* name = data + position + 14;
        if (name_size == 0 || name[0] == '/' || name[0] == 92) return -3;
        for (uint32_t i = 0; i < name_size; ++i) {
            const unsigned char ch = name[i];
            if (ch == 0 || ch == 92 || ch == ':' || ch < 32) return -3;
            if (ch == '/' && (i == 0 || i + 1 == name_size || name[i - 1] == '/'))
                return -3;
            const bool segment_start = i == 0 || name[i - 1] == '/';
            if (segment_start && ch == '.' &&
                (i + 1 == name_size || name[i + 1] == '/' ||
                 (i + 1 < name_size && name[i + 1] == '.' &&
                  (i + 2 == name_size || name[i + 2] == '/'))))
                return -3;
        }
        position += record_size + 2;
        if (count >= 65536) return -3;
        ++count;
    }
    return count;
}
int32_t st_xray_archive_directory_auto_decrypt(STXRayHandle handle,
                                                 uint32_t archive_index,
                                                 void* destination, uint32_t capacity,
                                                 uint32_t* actual_size, int32_t* key_used) {
    if (!handle || !destination || !actual_size || !key_used || capacity == 0 ||
        capacity > 64u * 1024u * 1024u) return -1;
    *actual_size = 0;
    *key_used = -2;
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
        std::vector<unsigned char> original(chunk_size);
        uint32_t cursor = 0;
        while (cursor < chunk_size) {
            const uint32_t amount = chunk_size - cursor > 65536u ? 65536u : chunk_size - cursor;
            const int32_t read_result = st_xray_archive_read(
                handle, archive_index, payload_offset + cursor, original.data() + cursor, amount);
            if (read_result != 0) return read_result;
            cursor += amount;
        }
        bool decoded_invalid_directory = false;
        for (int32_t key = -1; key <= 1; ++key) {
            std::vector<unsigned char> bytes = original;
            if (key >= 0 && st_xray_directory_decrypt(bytes.data(), chunk_size, key) != 0)
                return -3;
            lzo_uint decoded = capacity;
            const int result = lzo1x_decompress_safe(
                bytes.data(), chunk_size, static_cast<unsigned char*>(destination),
                &decoded, nullptr);
            if (result == LZO_E_OK && decoded != 0 && decoded <= capacity) {
                if (st_xray_validate_directory_records(destination,
                                                       static_cast<uint32_t>(decoded)) >= 0) {
                    *actual_size = static_cast<uint32_t>(decoded);
                    *key_used = key;
                    return 0;
                }
                decoded_invalid_directory = true;
            }
        }
        // -4: no key produced valid LZO; -5: LZO succeeded but records are invalid.
        return decoded_invalid_directory ? -5 : -4;
    }
    return -2;
}
int32_t st_xray_archive_directory_auto_decompress(STXRayHandle handle,
                                                    uint32_t archive_index,
                                                    void* destination, uint32_t capacity,
                                                    uint32_t* actual_size) {
    if (!handle || !destination || !actual_size || capacity == 0 ||
        capacity > 64u * 1024u * 1024u) return -1;
    *actual_size = 0;
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
        // miniLZO's safe decoder reports the actual decompressed byte count.
        lzo_uint decoded = capacity;
        const int status = lzo1x_decompress_safe(
            bytes.data(), chunk_size, static_cast<unsigned char*>(destination),
            &decoded, nullptr);
        if (status != LZO_E_OK) return -4;
        if (decoded == 0 || decoded > capacity ||
            st_xray_validate_directory_records(destination, static_cast<uint32_t>(decoded)) < 0) return -3;
        *actual_size = static_cast<uint32_t>(decoded);
        return 0;
    }
    return -2;
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
        const int32_t decode_result = st_xray_lzo_decompress(
            bytes.data(), chunk_size, destination, capacity, expected_size);
        if (decode_result != 0) return decode_result;
        return st_xray_validate_directory_records(destination, expected_size) >= 0 ? 0 : -3;
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
        if (compressed) {
            // Decode the real directory using the existing safe LZO + key
            // auto-detection path. Cap memory to avoid unbounded allocations
            // on iOS; callers can distinguish oversized directories (-4).
            constexpr uint32_t max_directory = 8u * 1024u * 1024u;
            std::vector<unsigned char> decoded(max_directory);
            uint32_t actual = 0;
            int32_t key = -2;
            const int32_t status = st_xray_archive_directory_auto_decrypt(
                handle, archive_index, decoded.data(), max_directory, &actual, &key);
            if (status != 0) return status;
            return st_xray_validate_directory_records(decoded.data(), actual);
        }
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
        if (compressed) {
            // Use the same bounded, validated LZO/key path as directory_count.
            // Do not expose paths from a partially decoded directory.
            constexpr uint32_t max_directory = 8u * 1024u * 1024u;
            std::vector<unsigned char> decoded(max_directory);
            uint32_t actual = 0;
            int32_t key = -2;
            const int32_t status = st_xray_archive_directory_auto_decrypt(
                handle, archive_index, decoded.data(), max_directory, &actual, &key);
            if (status != 0) return status;
            const int32_t entries = st_xray_validate_directory_records(decoded.data(), actual);
            if (entries < 0) return entries;
            if (entry_index >= static_cast<uint32_t>(entries)) return -2;
            uint32_t cursor = 0;
            for (uint32_t i = 0; i <= entry_index; ++i) {
                const uint32_t length = static_cast<uint32_t>(decoded[cursor]) |
                                        (static_cast<uint32_t>(decoded[cursor + 1]) << 8);
                if (i == entry_index) {
                    const uint32_t name_length = length - 16;
                    if (name_length == 0 || name_length + 1 > name_capacity) return -3;
                    const unsigned char* bytes = decoded.data() + cursor;
                    const unsigned char* path = bytes + 14;
                    if (std::memchr(path, 0, name_length)) return -3;
                    for (uint32_t j = 0; j < name_length; ++j) {
                        const char ch = static_cast<char>(path[j]);
                        if (ch == ':' || static_cast<unsigned char>(ch) < 32 ||
                            ch == '\\' || (ch == '/' && (j == 0 || j + 1 == name_length)))
                            return -3;
                        const bool start = j == 0 || path[j - 1] == '/';
                        if (start && ch == '.' &&
                            (j + 1 == name_length || path[j + 1] == '/' ||
                             (j + 1 < name_length && path[j + 1] == '.' &&
                              (j + 2 == name_length || path[j + 2] == '/'))))
                            return -3;
                    }
                    std::memcpy(name, path, name_length);
                    name[name_length] = 0;
                    auto le32 = [](const unsigned char* p) -> uint32_t {
                        return static_cast<uint32_t>(p[0]) |
                               (static_cast<uint32_t>(p[1]) << 8) |
                               (static_cast<uint32_t>(p[2]) << 16) |
                               (static_cast<uint32_t>(p[3]) << 24);
                    };
                    *size_real = le32(bytes + 2);
                    *size_compressed = le32(bytes + 6);
                    *crc = le32(bytes + 10);
                    *data_offset = le32(path + name_length);
                    return 0;
                }
                cursor += length + 2;
            }
            return -2;
        }
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
// X-Ray assets are typically addressed case-insensitively. Limit folding to
// ASCII so arbitrary high-bit bytes in archive names are not reinterpreted.
static bool st_xray_asset_path_equal(const char* left, const char* right, size_t size) {
    for (size_t i = 0; i < size; ++i) {
        unsigned char a = static_cast<unsigned char>(left[i]);
        unsigned char b = static_cast<unsigned char>(right[i]);
        if (a >= 'A' && a <= 'Z') a = static_cast<unsigned char>(a + ('a' - 'A'));
        if (b >= 'A' && b <= 'Z') b = static_cast<unsigned char>(b + ('a' - 'A'));
        if (a != b) return false;
    }
    return true;
}
int32_t st_xray_archive_find_file(STXRayHandle handle, uint32_t archive_index,
                                   const char* relative_path) {
    if (!handle || !relative_path) return -1;
    const size_t length = strnlen(relative_path, 4097);
    if (length == 0 || length > 4096 || relative_path[0] == '/') return -1;
    for (size_t i = 0; i < length; ++i) {
        const char ch = relative_path[i];
        if (ch == static_cast<char>(92) || ch == ':' || static_cast<unsigned char>(ch) < 32 ||
            (ch == '/' && (i == 0 || i + 1 == length || relative_path[i - 1] == '/')))
            return -1;
        if ((i == 0 || relative_path[i - 1] == '/') && ch == '.' &&
            (i + 1 == length || relative_path[i + 1] == '/' ||
             (i + 1 < length && relative_path[i + 1] == '.' &&
              (i + 2 == length || relative_path[i + 2] == '/'))))
            return -1;
    }
    // Locate the directory before decoding; do not decompress the same
    // compressed index once for counting and again for path lookup.
    uint32_t directory_chunk = 0, directory_size = 0;
    uint64_t directory_offset = 0;
    int32_t compressed = 0;
    bool has_directory = false;
    for (uint32_t chunk = 0; chunk < 64; ++chunk) {
        const int32_t status = st_xray_archive_chunk_info(
            handle, archive_index, chunk, &directory_chunk, &directory_offset,
            &directory_size, &compressed);
        if (status == -2) break;
        if (status != 0) return status;
        if (directory_chunk == 1) { has_directory = true; break; }
    }
    if (!has_directory) return -2;
    if (compressed) {
        constexpr uint32_t max_directory = 8u * 1024u * 1024u;
        std::vector<unsigned char> decoded(max_directory);
        uint32_t actual = 0;
        int32_t key = -2;
        const int32_t status = st_xray_archive_directory_auto_decrypt(
            handle, archive_index, decoded.data(), max_directory, &actual, &key);
        if (status != 0) return status;
        const int32_t count = st_xray_validate_directory_records(decoded.data(), actual);
        if (count < 0) return count;
        uint32_t cursor = 0;
        for (int32_t index = 0; index < count; ++index) {
            const uint32_t record_length = static_cast<uint32_t>(decoded[cursor]) |
                                           (static_cast<uint32_t>(decoded[cursor + 1]) << 8);
            const uint32_t name_length = record_length - 16;
            if (name_length == length &&
                st_xray_asset_path_equal(
                    reinterpret_cast<const char*>(decoded.data() + cursor + 14),
                    relative_path, name_length))
                return index;
            cursor += record_length + 2;
        }
        return -2;
    }
    const int32_t count = st_xray_archive_directory_count(handle, archive_index);
    if (count < 0) return count;
    // Directory entries are validated before exposing names.
    for (int32_t index = 0; index < count; ++index) {
        char name[4097] {};
        uint32_t real = 0, packed = 0, crc = 0, offset = 0;
        const int32_t status = st_xray_archive_directory_entry(
            handle, archive_index, static_cast<uint32_t>(index),
            name, sizeof(name), &real, &packed, &crc, &offset);
        if (status != 0) return status;
        if (std::strlen(name) == length && st_xray_asset_path_equal(name, relative_path, length)) return index;
    }
    return -2;
}
int32_t st_xray_find_resource(STXRayHandle handle, const char* relative_path,
                              uint32_t* archive_index, uint32_t* entry_index) {
    if (!archive_index || !entry_index) return -1;
    *archive_index = 0;
    *entry_index = 0;
    if (!handle || !relative_path) return -1;
    // Validate the path before scanning archives, including an empty set.
    // Otherwise malformed paths could incorrectly be reported as missing.
    const size_t length = strnlen(relative_path, 4097);
    if (length == 0 || length > 4096 || relative_path[0] == '/') return -1;
    for (size_t i = 0; i < length; ++i) {
        const char ch = relative_path[i];
        if (ch == static_cast<char>(92) || ch == ':' ||
            static_cast<unsigned char>(ch) < 32 ||
            (ch == '/' && (i == 0 || i + 1 == length || relative_path[i - 1] == '/')))
            return -1;
        if ((i == 0 || relative_path[i - 1] == '/') && ch == '.' &&
            (i + 1 == length || relative_path[i + 1] == '/' ||
             (i + 1 < length && relative_path[i + 1] == '.' &&
              (i + 2 == length || relative_path[i + 2] == '/'))))
            return -1;
    }
    const int32_t count = st_xray_archive_count(handle);
    if (count < 0) return count;
    for (int32_t index = count; index > 0; --index) {
        const int32_t entry = st_xray_archive_find_file(
            handle, static_cast<uint32_t>(index - 1), relative_path);
        if (entry >= 0) {
            *archive_index = static_cast<uint32_t>(index - 1);
            *entry_index = static_cast<uint32_t>(entry);
            return 0;
        }
        // Missing files are expected; malformed archive indexes are not.
        if (entry != -2) return entry;
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
    bool found_directory = false;
    for (uint32_t i = 0; i < 64; ++i) {
        const int32_t result = st_xray_archive_chunk_info(handle, archive_index, i,
                                                           &chunk_id, &chunk_offset,
                                                           &chunk_size, &chunk_compressed);
        if (result == -2) break;
        if (result != 0) return result;
        if (chunk_id == 1) {
            found_directory = true;
            const uint64_t file_begin = static_cast<uint64_t>(data_offset) + relative_offset;
            const uint64_t file_end = file_begin + size;
            const uint64_t directory_begin = chunk_offset - 8;
            const uint64_t directory_end = chunk_offset + chunk_size;
            if (size != 0 && file_begin < directory_end && file_end > directory_begin) return -3;
            break;
        }
    }
    if (!found_directory) return -2;
    return st_xray_archive_read(handle, archive_index,
                                static_cast<uint64_t>(data_offset) + relative_offset,
                                destination, size);
}
int32_t st_xray_archive_file_decompress(STXRayHandle handle, uint32_t archive_index,
                                         uint32_t entry_index, void* destination,
                                         uint32_t destination_capacity) {
    if (!handle || !destination) return -1;
    if (destination_capacity == 0 || destination_capacity > 64u * 1024u * 1024u) return -3;
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
int32_t st_xray_packed_resource_info(STXRayHandle handle, const char* relative_path,
                                     uint32_t* decoded_size, uint32_t* stored_size) {
    if (!decoded_size || !stored_size) return -1;
    *decoded_size = 0;
    *stored_size = 0;
    if (!handle || !relative_path) return -1;
    uint32_t archive = 0, entry = 0;
    const int32_t found = st_xray_find_resource(handle, relative_path, &archive, &entry);
    if (found != 0) return found;
    char name[4097] {};
    uint32_t crc = 0, offset = 0;
    return st_xray_archive_directory_entry(
        handle, archive, entry, name, sizeof(name),
        decoded_size, stored_size, &crc, &offset);
}
int32_t st_xray_packed_level_info(STXRayHandle handle, const char* level_name,
                                  uint32_t* geometry_size, uint32_t* collision_size) {
    if (!geometry_size || !collision_size) return -1;
    *geometry_size = 0;
    *collision_size = 0;
    if (!handle || !level_name) return -1;
    const size_t length = strnlen(level_name, 256);
    if (length == 0 || length > 128 || (length == 1 && level_name[0] == '.') ||
        (length == 2 && level_name[0] == '.' && level_name[1] == '.'))
        return -1;
    for (size_t i = 0; i < length; ++i) {
        const unsigned char ch = static_cast<unsigned char>(level_name[i]);
        if (!((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
              (ch >= '0' && ch <= '9') || ch == '_' || ch == '-' || ch == '.'))
            return -1;
    }
    const std::string base = std::string("levels/") + level_name + "/";
    uint32_t stored = 0;
    const int32_t geometry = st_xray_packed_resource_info(
        handle, (base + "level").c_str(), geometry_size, &stored);
    if (geometry != 0 && geometry != -2) return geometry;
    const int32_t collision = st_xray_packed_resource_info(
        handle, (base + "level.cform").c_str(), collision_size, &stored);
    if (collision != 0 && collision != -2) return collision;
    return geometry == 0 || collision == 0 ? 0 : -2;
}
int32_t st_xray_load_packed_resource(STXRayHandle handle, const char* relative_path,
                                     void* destination, uint32_t capacity,
                                     uint32_t* actual_size) {
    if (!actual_size) return -1;
    *actual_size = 0;
    if (!handle || !relative_path) return -1;
    uint32_t archive = 0, entry = 0;
    const int32_t found = st_xray_find_resource(handle, relative_path, &archive, &entry);
    if (found != 0) return found;
    char name[4097] {};
    uint32_t real = 0, stored = 0, crc = 0, offset = 0;
    const int32_t metadata = st_xray_archive_directory_entry(
        handle, archive, entry, name, sizeof(name), &real, &stored, &crc, &offset);
    if (metadata != 0) return metadata;
    *actual_size = real;
    constexpr uint32_t max_resource = 64u * 1024u * 1024u;
    if (real > max_resource || real > capacity) return -3;
    // An archive may be replaced or truncated after the catalog was read.
    // Each read rechecks archive bounds; compressed reads additionally stage
    // decoded bytes before writing to the caller's destination.
    if (real == 0) {
        // Even empty entries must have a valid in-archive data offset.
        if (stored != 0 || offset < 8) return -3;
        return st_xray_archive_file_read(handle, archive, entry, 0, nullptr, 0);
    }
    if (!destination) return -1;
    if (real != stored) {
        // Decode into temporary memory: corrupt LZO must not leave the caller
        // with a partially overwritten game resource.
        std::vector<unsigned char> decoded(real);
        const int32_t status = st_xray_archive_file_decompress(
            handle, archive, entry, decoded.data(), real);
        if (status != 0) return status;
        std::memcpy(destination, decoded.data(), real);
        return 0;
    }
    // Stage plain files as well: a truncated or replaced archive must not
    // expose partially loaded level data to the caller.
    std::vector<unsigned char> staged(real);
    uint32_t cursor = 0;
    while (cursor < real) {
        const uint32_t amount = real - cursor > 65536u ? 65536u : real - cursor;
        const int32_t result = st_xray_archive_file_read(
            handle, archive, entry, cursor, staged.data() + cursor, amount);
        if (result != 0) return result;
        cursor += amount;
    }
    std::memcpy(destination, staged.data(), real);
    return 0;
}
int32_t st_xray_load_packed_level_file(STXRayHandle handle, const char* level_name,
                                        int32_t member, void* destination,
                                        uint32_t capacity, uint32_t* actual_size) {
    if (!actual_size) return -1;
    *actual_size = 0;
    if (member != 0 && member != 1) return -1;
    uint32_t geometry_size = 0, collision_size = 0;
    const int32_t probe = st_xray_packed_level_info(
        handle, level_name, &geometry_size, &collision_size);
    if (probe != 0) return probe;
    const std::string relative_path = std::string("levels/") + level_name +
                                      (member == 0 ? "/level" : "/level.cform");
    return st_xray_load_packed_resource(
        handle, relative_path.c_str(), destination, capacity, actual_size);
}
static uint32_t memory_le32(const unsigned char* p) {
    return uint32_t(p[0]) | (uint32_t(p[1]) << 8) |
           (uint32_t(p[2]) << 16) | (uint32_t(p[3]) << 24);
}
int32_t st_xray_memory_chunk_count(const void* bytes, uint32_t size) {
    if (!bytes || size == 0 || size > 64u * 1024u * 1024u) return -1;
    const auto* p = static_cast<const unsigned char*>(bytes);
    uint32_t cursor = 0;
    int32_t count = 0;
    while (cursor < size) {
        if (size - cursor < 8 || count >= 65536) return -3;
        const uint32_t payload = memory_le32(p + cursor + 4);
        if (payload > size - cursor - 8) return -3;
        cursor += 8 + payload;
        ++count;
    }
    return count;
}
int32_t st_xray_memory_chunk_info(const void* bytes, uint32_t size,
                                  uint32_t index, uint32_t* chunk_id,
                                  uint32_t* payload_offset, uint32_t* payload_size,
                                  int32_t* compressed) {
    if (!chunk_id || !payload_offset || !payload_size || !compressed) return -1;
    const int32_t count = st_xray_memory_chunk_count(bytes, size);
    if (count < 0) return count;
    if (index >= static_cast<uint32_t>(count)) return -2;
    const auto* p = static_cast<const unsigned char*>(bytes);
    uint32_t cursor = 0;
    for (uint32_t i = 0; i < index; ++i)
        cursor += 8 + memory_le32(p + cursor + 4);
    const uint32_t raw_id = memory_le32(p + cursor);
    *chunk_id = raw_id & 0x7fffffffu;
    *compressed = (raw_id & 0x80000000u) != 0 ? 1 : 0;
    *payload_offset = cursor + 8;
    *payload_size = memory_le32(p + cursor + 4);
    return 0;
}
int32_t st_xray_memory_cform_info(const void* bytes, uint32_t size,
                                  uint32_t* vertices, uint32_t* triangles,
                                  float* minimum_xyz, float* maximum_xyz) {
    if (!bytes || !vertices || !triangles || !minimum_xyz || !maximum_xyz) return -1;
    if (size < 36 || size > 64u * 1024u * 1024u) return -3;

    const auto* p = static_cast<const unsigned char*>(bytes);
    const uint32_t version = memory_le32(p);
    // CForm v4 is the supported X-Ray collision layout for this bridge.
    if (version != 4) return -3;
    const uint32_t vertex_count = memory_le32(p + 4);
    const uint32_t face_count = memory_le32(p + 8);
    const uint64_t required = 36ull + uint64_t(vertex_count) * 12ull +
                              uint64_t(face_count) * 16ull;
    if (required > size) return -3;
    float low[3] {}, high[3] {};
    for (int axis = 0; axis < 3; ++axis) {
        const uint32_t a = memory_le32(p + 12 + axis * 4);
        const uint32_t b = memory_le32(p + 24 + axis * 4);
        std::memcpy(&low[axis], &a, sizeof(float));
        std::memcpy(&high[axis], &b, sizeof(float));
        if (!std::isfinite(low[axis]) || !std::isfinite(high[axis]) ||
            low[axis] > high[axis]) return -3;
    }
    // Validate referenced collision vertices before passing the mesh to a renderer.
    // Each vertex is three IEEE-754 floats; each triangle begins with three
    // little-endian 32-bit vertex indices followed by material metadata.
    for (uint32_t i = 0; i < vertex_count; ++i) {
        const unsigned char* vertex = p + 36ull + uint64_t(i) * 12ull;
        for (int axis = 0; axis < 3; ++axis) {
            const uint32_t bits = memory_le32(vertex + axis * 4);
            float coordinate = 0.0f;
            std::memcpy(&coordinate, &bits, sizeof(coordinate));
            if (!std::isfinite(coordinate)) return -3;
        }
    }
    const uint64_t faces_offset = 36ull + uint64_t(vertex_count) * 12ull;
    for (uint32_t i = 0; i < face_count; ++i) {
        const unsigned char* face = p + faces_offset + uint64_t(i) * 16ull;
        for (int corner = 0; corner < 3; ++corner)
            if (memory_le32(face + corner * 4) >= vertex_count) return -3;
    }
    *vertices = vertex_count;
    *triangles = face_count;
    std::memcpy(minimum_xyz, low, sizeof(low));
    std::memcpy(maximum_xyz, high, sizeof(high));
    return 0;
}
int32_t st_xray_memory_cform_mesh(const void* bytes, uint32_t size,
                                   float* xyz, uint32_t vertex_capacity,
                                   uint32_t* indices, uint32_t triangle_capacity,
                                   uint32_t* vertex_count, uint32_t* triangle_count) {
    if (!vertex_count || !triangle_count) return -1;
    *vertex_count = 0;
    *triangle_count = 0;
    // Invalid caller buffers must be rejected before inspecting game data.
    if ((vertex_capacity != 0 && !xyz) ||
        (triangle_capacity != 0 && !indices)) return -1;
    uint32_t vertices = 0, triangles = 0;
    float minimum[3] {}, maximum[3] {};
    const int32_t valid = st_xray_memory_cform_info(
        bytes, size, &vertices, &triangles, minimum, maximum);
    if (valid != 0) return valid;
    // Bound the prototype viewer's memory and prevent oversized GPU uploads.
    if (vertices > 50000 || triangles > 50000 ||
        vertices > vertex_capacity || triangles > triangle_capacity) return -3;
    if ((vertices != 0 && !xyz) || (triangles != 0 && !indices)) return -1;
    const auto* p = static_cast<const unsigned char*>(bytes);
    for (uint32_t i = 0; i < vertices; ++i) {
        const unsigned char* vertex = p + 36ull + uint64_t(i) * 12ull;
        for (int axis = 0; axis < 3; ++axis) {
            const uint32_t bits = memory_le32(vertex + axis * 4);
            std::memcpy(xyz + i * 3 + axis, &bits, sizeof(float));
        }
    }
    const uint64_t faces_offset = 36ull + uint64_t(vertices) * 12ull;
    for (uint32_t i = 0; i < triangles; ++i) {
        const unsigned char* face = p + faces_offset + uint64_t(i) * 16ull;
        for (int corner = 0; corner < 3; ++corner)
            indices[i * 3 + corner] = memory_le32(face + corner * 4);
    }
    *vertex_count = vertices;
    *triangle_count = triangles;
    return 0;
}
int32_t st_xray_packed_level_chunk_count(STXRayHandle handle, const char* level_name) {
    uint32_t geometry = 0, collision = 0;
    const int32_t status = st_xray_packed_level_info(
        handle, level_name, &geometry, &collision);
    if (status != 0) return status;
    if (geometry == 0) return -2;
    if (geometry > 64u * 1024u * 1024u) return -3;
    std::vector<unsigned char> bytes(geometry);
    uint32_t actual = 0;
    const int32_t loaded = st_xray_load_packed_level_file(
        handle, level_name, 0, bytes.data(), geometry, &actual);
    if (loaded != 0) return loaded;
    if (actual != geometry) return -3;
    return st_xray_memory_chunk_count(bytes.data(), actual);
}
int32_t st_xray_packed_level_chunk_info(STXRayHandle handle, const char* level_name,
                                         uint32_t index, uint32_t* chunk_id,
                                         uint32_t* payload_offset, uint32_t* payload_size,
                                         int32_t* compressed) {
    if (!chunk_id || !payload_offset || !payload_size || !compressed) return -1;
    uint32_t geometry = 0, collision = 0;
    const int32_t status = st_xray_packed_level_info(
        handle, level_name, &geometry, &collision);
    if (status != 0) return status;
    if (geometry == 0) return -2;
    if (geometry > 64u * 1024u * 1024u) return -3;
    std::vector<unsigned char> bytes(geometry);
    uint32_t actual = 0;
    const int32_t loaded = st_xray_load_packed_level_file(
        handle, level_name, 0, bytes.data(), geometry, &actual);
    if (loaded != 0) return loaded;
    if (actual != geometry) return -3;
    return st_xray_memory_chunk_info(bytes.data(), actual, index,
                                     chunk_id, payload_offset, payload_size, compressed);
}
int32_t st_xray_packed_level_chunk_read(STXRayHandle handle, const char* level_name,
                                         uint32_t index, void* destination,
                                         uint32_t capacity, uint32_t* actual_size) {
    if (!actual_size) return -1;
    *actual_size = 0;
    uint32_t geometry = 0, collision = 0;
    const int32_t status = st_xray_packed_level_info(
        handle, level_name, &geometry, &collision);
    if (status != 0) return status;
    if (geometry == 0) return -2;
    if (geometry > 64u * 1024u * 1024u) return -3;
    std::vector<unsigned char> bytes(geometry);
    uint32_t actual = 0;
    const int32_t loaded = st_xray_load_packed_level_file(
        handle, level_name, 0, bytes.data(), geometry, &actual);
    if (loaded != 0) return loaded;
    if (actual != geometry) return -3;
    uint32_t chunk_id = 0, offset = 0, payload_size = 0;
    int32_t compressed = 0;
    const int32_t info = st_xray_memory_chunk_info(
        bytes.data(), actual, index, &chunk_id, &offset, &payload_size, &compressed);
    if (info != 0) return info;
    *actual_size = payload_size;
    if (payload_size > capacity) return -3;
    if (payload_size == 0) return 0;
    if (!destination) return -1;
    std::memcpy(destination, bytes.data() + offset, payload_size);
    return 0;
}
int32_t st_xray_packed_level_chunk_decode(STXRayHandle handle, const char* level_name,
                                           uint32_t index, void* destination,
                                           uint32_t capacity, uint32_t expected_size,
                                           uint32_t* actual_size) {
    if (!actual_size) return -1;
    *actual_size = 0;
    if (expected_size > 64u * 1024u * 1024u) return -3;
    if (expected_size > capacity) return -3;
    if (expected_size != 0 && !destination) return -1;
    uint32_t geometry = 0, collision = 0;
    const int32_t status = st_xray_packed_level_info(
        handle, level_name, &geometry, &collision);
    if (status != 0) return status;
    if (geometry == 0) return -2;
    if (geometry > 64u * 1024u * 1024u) return -3;
    std::vector<unsigned char> bytes(geometry);
    uint32_t loaded_size = 0;
    const int32_t loaded = st_xray_load_packed_level_file(
        handle, level_name, 0, bytes.data(), geometry, &loaded_size);
    if (loaded != 0) return loaded;
    if (loaded_size != geometry) return -3;
    uint32_t chunk_id = 0, offset = 0, stored_size = 0;
    int32_t compressed = 0;
    const int32_t info = st_xray_memory_chunk_info(
        bytes.data(), loaded_size, index,
        &chunk_id, &offset, &stored_size, &compressed);
    if (info != 0) return info;
    if (!compressed) {
        *actual_size = stored_size;
        if (expected_size != stored_size || capacity < stored_size) return -3;
        if (stored_size == 0) return 0;
        if (!destination) return -1;
        std::memcpy(destination, bytes.data() + offset, stored_size);
        return 0;
    }
    if (expected_size == 0 || expected_size > capacity || stored_size == 0) return -3;
    if (!destination) return -1;
    std::vector<unsigned char> decoded(expected_size);
    const int32_t decode_status = st_xray_lzo_decompress(
        bytes.data() + offset, stored_size, decoded.data(), expected_size, expected_size);
    if (decode_status != 0) return decode_status;
    std::memcpy(destination, decoded.data(), expected_size);
    *actual_size = expected_size;
    return 0;
}
int32_t st_xray_packed_level_collision_info(STXRayHandle handle, const char* level_name,
                                            uint32_t* vertices, uint32_t* triangles,
                                            float* minimum_xyz, float* maximum_xyz) {
    if (!vertices || !triangles || !minimum_xyz || !maximum_xyz) return -1;
    *vertices = 0;
    *triangles = 0;
    if (!handle || !level_name || !*level_name) return -1;
    uint32_t geometry = 0, collision = 0;
    const int32_t status = st_xray_packed_level_info(
        handle, level_name, &geometry, &collision);
    if (status != 0) return status;
    if (collision == 0) return -2;
    if (collision > 64u * 1024u * 1024u) return -3;
    std::vector<unsigned char> bytes(collision);
    uint32_t actual = 0;
    const int32_t loaded = st_xray_load_packed_level_file(
        handle, level_name, 1, bytes.data(), collision, &actual);
    if (loaded != 0) return loaded;
    if (actual != collision) return -3;
    return st_xray_memory_cform_info(
        bytes.data(), actual, vertices, triangles, minimum_xyz, maximum_xyz);
}
int32_t st_xray_packed_level_collision_mesh(STXRayHandle handle, const char* level_name,
                                            float* xyz, uint32_t vertex_capacity,
                                            uint32_t* indices, uint32_t triangle_capacity,
                                            uint32_t* vertex_count, uint32_t* triangle_count) {
    if (!vertex_count || !triangle_count) return -1;
    *vertex_count = 0;
    *triangle_count = 0;
    if (!handle || !level_name || !*level_name) return -1;
    if ((vertex_capacity != 0 && !xyz) ||
        (triangle_capacity != 0 && !indices)) return -1;
    uint32_t geometry = 0, collision = 0;
    const int32_t status = st_xray_packed_level_info(
        handle, level_name, &geometry, &collision);
    if (status != 0) return status;
    if (collision == 0) return -2;
    if (collision > 64u * 1024u * 1024u) return -3;
    std::vector<unsigned char> bytes(collision);
    uint32_t actual = 0;
    const int32_t loaded = st_xray_load_packed_level_file(
        handle, level_name, 1, bytes.data(), collision, &actual);
    if (loaded != 0) return loaded;
    if (actual != collision) return -3;
    return st_xray_memory_cform_mesh(
        bytes.data(), actual, xyz, vertex_capacity, indices,
        triangle_capacity, vertex_count, triangle_count);
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
