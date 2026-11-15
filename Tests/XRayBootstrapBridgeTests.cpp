#include "../Sources/Native/STXRayBootstrap.h"
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>
#include <vector>
#include <cstdint>
#include <sys/stat.h>
#include <cstring>
#include <unistd.h>
#include "../Sources/Native/Upstream/minilzo.h"

int main() {
    // Inspect synthetic decoded geometry without filesystem access.
    const unsigned char geometry[] = {
        7,0,0,0, 2,0,0,0, 0xAA,0xBB,
        8,0,0,0x80, 1,0,0,0, 0xCC
    };
    assert(st_xray_memory_chunk_count(geometry, sizeof(geometry)) == 2);
    // 100 distinct archive layouts: 1..100 chunks with alternating compressed
    // flags and varying payload lengths. Validate every index and its bounds.
    for (uint32_t chunkTotal = 1; chunkTotal <= 100; ++chunkTotal) {
        std::vector<unsigned char> stream;
        std::vector<uint32_t> expectedOffsets;
        std::vector<uint32_t> expectedSizes;
        for (uint32_t j = 0; j < chunkTotal; ++j) {
            const uint32_t payload = j % 7;
            const uint32_t rawID = (j + 1) | ((j % 2) ? 0x80000000u : 0u);
            expectedOffsets.push_back(static_cast<uint32_t>(stream.size()) + 8);
            expectedSizes.push_back(payload);
            for (uint32_t shift = 0; shift < 32; shift += 8)
                stream.push_back(static_cast<unsigned char>(rawID >> shift));
            for (uint32_t shift = 0; shift < 32; shift += 8)
                stream.push_back(static_cast<unsigned char>(payload >> shift));
            stream.insert(stream.end(), payload, static_cast<unsigned char>(j));
        }
        assert(st_xray_memory_chunk_count(stream.data(),
            static_cast<uint32_t>(stream.size())) == static_cast<int32_t>(chunkTotal));
        for (uint32_t j = 0; j < chunkTotal; ++j) {
            uint32_t id = 0, offset = 0, length = 0;
            int32_t isCompressed = -1;
            assert(st_xray_memory_chunk_info(stream.data(),
                static_cast<uint32_t>(stream.size()), j,
                &id, &offset, &length, &isCompressed) == 0);
            assert(id == j + 1 && offset == expectedOffsets[j] &&
                   length == expectedSizes[j] && isCompressed == static_cast<int32_t>(j % 2));
        }
        uint32_t id = 0, offset = 0, length = 0;
        int32_t isCompressed = -1;
        assert(st_xray_memory_chunk_info(stream.data(),
            static_cast<uint32_t>(stream.size()), chunkTotal,
            &id, &offset, &length, &isCompressed) == -2);
        // Truncated last chunk header or payload must never be accepted.
        assert(st_xray_memory_chunk_count(stream.data(),
            static_cast<uint32_t>(stream.size() - 1)) == -3);
    }

    uint32_t memoryID = 0, memoryOffset = 0, memorySize = 0;
    int32_t memoryCompressed = -1;
    assert(st_xray_memory_chunk_info(geometry, sizeof(geometry), 0,
        &memoryID, &memoryOffset, &memorySize, &memoryCompressed) == 0);
    assert(memoryID == 7 && memoryOffset == 8 && memorySize == 2 && memoryCompressed == 0);
    assert(st_xray_memory_chunk_info(geometry, sizeof(geometry), 1,
        &memoryID, &memoryOffset, &memorySize, &memoryCompressed) == 0);
    assert(memoryID == 8 && memoryOffset == 18 && memorySize == 1 && memoryCompressed == 1);
    assert(st_xray_memory_chunk_info(geometry, sizeof(geometry), 2,
        &memoryID, &memoryOffset, &memorySize, &memoryCompressed) == -2);
    assert(st_xray_memory_chunk_count(geometry, sizeof(geometry) - 1) == -3);
    assert(st_xray_memory_chunk_count(nullptr, 0) == -1);
    // Boundary cases: empty input, short headers, oversized payload claims,
    // maximum valid chunk count and the first count beyond the safety limit.
    const unsigned char shortHeader[7] = {1,0,0,0,0,0,0};
    assert(st_xray_memory_chunk_count(shortHeader, sizeof(shortHeader)) == -3);
    const unsigned char oversizedPayload[8] = {1,0,0,0,255,255,255,255};
    assert(st_xray_memory_chunk_count(oversizedPayload, sizeof(oversizedPayload)) == -3);
    std::vector<unsigned char> maxChunks(65536u * 8u, 0);
    for (uint32_t j = 0; j < 65536u; ++j) maxChunks[j * 8u] = 1;
    assert(st_xray_memory_chunk_count(maxChunks.data(),
        static_cast<uint32_t>(maxChunks.size())) == 65536);
    maxChunks.insert(maxChunks.end(), {1,0,0,0,0,0,0,0});
    assert(st_xray_memory_chunk_count(maxChunks.data(),
        static_cast<uint32_t>(maxChunks.size())) == -3);

    // Deterministic mutation sweep over valid chunk streams. No crashes or
    // successful out-of-bounds chunk offsets are permitted.
    uint32_t mutationSeed = 0x1234abcdu;
    for (uint32_t iteration = 0; iteration < 256; ++iteration) {
        std::vector<unsigned char> mutated(32, 0);
        for (uint32_t chunk = 0; chunk < 4; ++chunk) mutated[chunk * 8] = 1;
        mutationSeed = mutationSeed * 1664525u + 1013904223u;
        const uint32_t position = mutationSeed % static_cast<uint32_t>(mutated.size());
        mutationSeed = mutationSeed * 1664525u + 1013904223u;
        mutated[position] ^= static_cast<unsigned char>(mutationSeed >> 24);
        const int32_t count = st_xray_memory_chunk_count(
            mutated.data(), static_cast<uint32_t>(mutated.size()));
        assert(count == -3 || (count >= 1 && count <= 4));
        if (count > 0) {
            for (int32_t j = 0; j < count; ++j) {
                uint32_t id = 0, offset = 0, length = 0;
                int32_t isCompressed = 0;
                assert(st_xray_memory_chunk_info(mutated.data(),
                    static_cast<uint32_t>(mutated.size()), static_cast<uint32_t>(j),
                    &id, &offset, &length, &isCompressed) == 0);
                assert(offset <= mutated.size() && length <= mutated.size() - offset);
            }
        }
    }
    const unsigned char collision[] = {
        4,0,0,0, 1,0,0,0, 0,0,0,0,
        0,0,0,0, 0,0,0,0, 0,0,0,0,
        0,0,128,63, 0,0,128,63, 0,0,128,63,
        0,0,0,0, 0,0,0,0, 0,0,0,0
    };
    uint32_t memoryVertices = 0, memoryFaces = 0;
    float memoryMin[3] {}, memoryMax[3] {};
    assert(st_xray_memory_cform_info(collision, sizeof(collision),
        &memoryVertices, &memoryFaces, memoryMin, memoryMax) == 0);
    assert(memoryVertices == 1 && memoryFaces == 0 && memoryMax[0] == 1.0f);
    unsigned char unsupportedCollision[sizeof(collision)] {};
    std::memcpy(unsupportedCollision, collision, sizeof(collision));
    unsupportedCollision[0] = 5;
    assert(st_xray_memory_cform_info(unsupportedCollision, sizeof(unsupportedCollision),
        &memoryVertices, &memoryFaces, memoryMin, memoryMax) == -3);

    std::vector<unsigned char> trailingCollision(collision, collision + sizeof(collision));
    trailingCollision.push_back(0xCC);
    assert(st_xray_memory_cform_info(trailingCollision.data(),
        static_cast<uint32_t>(trailingCollision.size()),
        &memoryVertices, &memoryFaces, memoryMin, memoryMax) == 0);
    assert(st_xray_memory_cform_info(collision, 35,
        &memoryVertices, &memoryFaces, memoryMin, memoryMax) == -3);
    assert(st_xray_memory_cform_info(nullptr, 0,
        &memoryVertices, &memoryFaces, memoryMin, memoryMax) == -1);
    // A complete triangle references three vertices by zero-based index.
    // Reject out-of-range references without changing the previous result.
    std::vector<unsigned char> triangleCollision(collision, collision + sizeof(collision));
    triangleCollision[8] = 1; // one 16-byte triangle
    triangleCollision.insert(triangleCollision.end(), 16, 0);
    assert(st_xray_memory_cform_info(triangleCollision.data(),
        static_cast<uint32_t>(triangleCollision.size()),
        &memoryVertices, &memoryFaces, memoryMin, memoryMax) == 0);
    assert(memoryVertices == 1 && memoryFaces == 1);
    float meshXYZ[3] = {-1.0f,-1.0f,-1.0f};
    uint32_t meshIndices[3] = {99,99,99};
    uint32_t meshVertices = 0, meshTriangles = 0;
    assert(st_xray_memory_cform_mesh(triangleCollision.data(),
        static_cast<uint32_t>(triangleCollision.size()), meshXYZ, 1,
        meshIndices, 1, &meshVertices, &meshTriangles) == 0);
    assert(meshVertices == 1 && meshTriangles == 1);
    assert(meshXYZ[0] == 0.0f && meshIndices[0] == 0 &&
           meshIndices[1] == 0 && meshIndices[2] == 0);
    meshXYZ[0] = 123.0f;
    assert(st_xray_memory_cform_mesh(triangleCollision.data(),
        static_cast<uint32_t>(triangleCollision.size()), meshXYZ, 0,
        meshIndices, 1, &meshVertices, &meshTriangles) == -3);
    assert(meshVertices == 0 && meshTriangles == 0 && meshXYZ[0] == 123.0f);
    const uint32_t faceStart = 36 + 12;
    triangleCollision[faceStart] = 1; // vertex index 1 is out of range
    memoryVertices = 99;
    memoryFaces = 99;
    assert(st_xray_memory_cform_info(triangleCollision.data(),
        static_cast<uint32_t>(triangleCollision.size()),
        &memoryVertices, &memoryFaces, memoryMin, memoryMax) == -3);
    assert(memoryVertices == 99 && memoryFaces == 99);
    triangleCollision[faceStart] = 0;
    triangleCollision[faceStart + 8] = 1; // third corner also checked
    assert(st_xray_memory_cform_info(triangleCollision.data(),
        static_cast<uint32_t>(triangleCollision.size()),
        &memoryVertices, &memoryFaces, memoryMin, memoryMax) == -3);
    triangleCollision[faceStart + 8] = 0;
    assert(st_xray_memory_cform_info(triangleCollision.data(),
        static_cast<uint32_t>(triangleCollision.size()),
        &memoryVertices, &memoryFaces, memoryMin, memoryMax) == 0);
    assert(memoryVertices == 1 && memoryFaces == 1);


    assert(st_xray_initialize(nullptr, 0) == -1);
    STXRayHandle handle = st_xray_create();
    assert(handle);
    assert(st_xray_state(handle) == 0);
    assert(st_xray_initialize(handle, 3) == -1);
    assert(st_xray_initialize(handle, 2) == 0);
    assert(st_xray_state(handle) == 1);
    char folder[] = "/tmp/xray-bridge-XXXXXX";
    char* path = mkdtemp(folder);
    assert(path);
    assert(st_xray_attach_resources(handle, path) == -2);
    const std::string emptyData = std::string(path) + "/gamedata";
    assert(mkdir(emptyData.c_str(), 0700) == 0);
    assert(st_xray_attach_resources(handle, path) == -2);
    const std::string emptyFile = emptyData + "/empty.txt";
    { std::ofstream file(emptyFile); }
    assert(st_xray_attach_resources(handle, path) == -2);
    assert(std::remove(emptyFile.c_str()) == 0);
    assert(rmdir(emptyData.c_str()) == 0);
    const std::string archive = std::string(path) + "/gamedata.db0";
    const std::string backupLike = std::string(path) + "/gamedata.db.backup";
    { std::ofstream file(backupLike); file << "not an archive"; }
    { std::ofstream file(archive); }
    assert(st_xray_attach_resources(handle, path) == -2);
    { std::ofstream file(archive); file << "fixture"; }
    const std::string secondArchive = std::string(path) + "/gamedata.db1";
    { std::ofstream file(secondArchive); file << "fixture2"; }
    assert(st_xray_attach_resources(handle, path) == 0);
    assert(st_xray_state(handle) == 2);
    assert(st_xray_has_packed_data(handle) == 1);
    assert(st_xray_archive_count(handle) == 2);
    // Asset resolver must reject unsafe paths before touching archive data.
    uint32_t foundArchive = 99, foundEntry = 99, requiredSize = 99;
    // Ten additional boundary cases for the archive lookup contract.
    assert(st_xray_archive_find_file(handle, 0, "") == -1);
    assert(st_xray_archive_find_file(handle, 0, ".") == -1);
    assert(st_xray_archive_find_file(handle, 0, "..") == -1);
    assert(st_xray_archive_find_file(handle, 0, "a/.") == -1);
    assert(st_xray_archive_find_file(handle, 0, "a/..") == -1);
    assert(st_xray_archive_find_file(handle, 0, "a//b") == -1);
    assert(st_xray_archive_find_file(handle, 0, "a/") == -1);
    assert(st_xray_archive_find_file(handle, 0, "a:b") == -1);
    assert(st_xray_archive_find_file(handle, 0, "a\\\\b") == -1);
    assert(st_xray_archive_find_file(handle, 0, "a/../b") == -1);
    uint32_t rejectedArchive = 99, rejectedEntry = 99;
    assert(st_xray_find_resource(handle, "../escape", &rejectedArchive, &rejectedEntry) == -1);
    assert(rejectedArchive == 0 && rejectedEntry == 0);
    assert(st_xray_find_resource(handle, "/absolute", &rejectedArchive, &rejectedEntry) == -1);
    assert(st_xray_find_resource(handle, "a//b", &rejectedArchive, &rejectedEntry) == -1);
    assert(st_xray_find_resource(handle, "a/./b", &rejectedArchive, &rejectedEntry) == -1);
    assert(st_xray_find_resource(handle, "a/../b", &rejectedArchive, &rejectedEntry) == -1);
    assert(st_xray_find_resource(handle, "a:b", &rejectedArchive, &rejectedEntry) == -1);
    assert(st_xray_archive_find_file(handle, 0, "../escape") == -1);
    assert(st_xray_archive_find_file(handle, 0, "/absolute") == -1);
    assert(st_xray_archive_find_file(handle, 0, "levels//bad") == -1);
    assert(st_xray_archive_find_file(handle, 0, "levels/./bad") == -1);
    assert(st_xray_archive_find_file(handle, 0, "levels/../bad") == -1);
    assert(st_xray_archive_find_file(handle, 0, "C:/game") == -1);
    assert(st_xray_find_resource(handle, "../escape", &foundArchive, &foundEntry) == -1);
    assert(st_xray_find_resource(handle, "levels/../bad", &foundArchive, &foundEntry) == -1);
    assert(st_xray_find_resource(handle, "levels/file", nullptr, &foundEntry) == -1);
    assert(st_xray_load_packed_resource(handle, "../escape", nullptr, 0, &requiredSize) == -1);
    assert(st_xray_load_packed_resource(handle, "levels/file", nullptr, 0, nullptr) == -1);
    uint32_t resetSize = 123;
    assert(st_xray_load_packed_resource(nullptr, "levels/file", nullptr, 0, &resetSize) == -1);
    assert(resetSize == 0);
    assert(st_xray_load_packed_resource(handle, nullptr, nullptr, 0, &resetSize) == -1);
    assert(resetSize == 0);
    // Directory record counts are capped to fit the native entry-index API.
    {
        const unsigned char record[] = {
            17,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 'a', 0,0,0,0
        };
        std::string oversized;
        oversized.reserve(sizeof(record) * 65537u);
        for (uint32_t i = 0; i < 65537u; ++i)
            oversized.append(reinterpret_cast<const char*>(record), sizeof(record));
        assert(st_xray_validate_directory_records(
            oversized.data(), static_cast<uint32_t>(oversized.size())) == -3);
    }
    assert(st_xray_archive_bytes(handle) == 15);
    char archiveName[32] = {};
    assert(st_xray_archive_name(handle, 0, archiveName, sizeof(archiveName)) == 0);
    assert(std::strcmp(archiveName, "gamedata.db0") == 0);
    assert(st_xray_archive_name(handle, 0, archiveName, 4) == -3);
    assert(st_xray_archive_name(handle, 1, archiveName, sizeof(archiveName)) == 0);
    assert(std::strcmp(archiveName, "gamedata.db1") == 0);
    assert(st_xray_archive_name(handle, 2, archiveName, sizeof(archiveName)) == -2);
    assert(st_xray_archive_name(handle, 0, nullptr, 0) == -1);
    char archiveBytes[8] = {};
    assert(st_xray_archive_read(handle, 0, 0, archiveBytes, 7) == 0);
    assert(std::memcmp(archiveBytes, "fixture", 7) == 0);
    assert(st_xray_archive_read(handle, 1, 0, archiveBytes, 8) == 0);
    assert(std::memcmp(archiveBytes, "fixture2", 8) == 0);
    assert(st_xray_archive_read(handle, 0, 5, archiveBytes, 3) == -3);
    assert(st_xray_archive_read(handle, 0, 0, archiveBytes, 65537) == -1);
    assert(st_xray_archive_read(handle, 2, 0, archiveBytes, 1) == -2);
    assert(st_xray_archive_read(handle, 0, 7, nullptr, 0) == 0);
    const std::string replacement = std::string(path) + "/replacement.bin";
    { std::ofstream file(replacement); file << "replacement"; }
    assert(std::remove(archive.c_str()) == 0);
    assert(symlink(replacement.c_str(), archive.c_str()) == 0);
    assert(st_xray_archive_read(handle, 0, 0, archiveBytes, 1) == -3);
    assert(std::remove(archive.c_str()) == 0);
    { std::ofstream file(archive); file << "fixture"; }
    assert(std::remove(replacement.c_str()) == 0);
    // The file may change after inventory: every read must re-check current length.
    { std::ofstream file(archive, std::ios::binary | std::ios::trunc); file << "x"; }
    assert(st_xray_archive_read(handle, 0, 0, archiveBytes, 7) == -3);
    assert(st_xray_archive_read(handle, 0, 0, archiveBytes, 1) == 0);
    assert(archiveBytes[0] == 'x');
    { std::ofstream file(archive, std::ios::binary | std::ios::trunc); file << "fixture"; }
    // Genuine X-Ray archive framing: little-endian chunk ID and payload length.
    {
        std::ofstream file(archive, std::ios::binary | std::ios::trunc);
        const unsigned char bytes[] = {1,0,0,128, 3,0,0,0, 9,8,7,
                                       2,0,0,0, 2,0,0,0, 6,5};
        file.write(reinterpret_cast<const char*>(bytes), sizeof(bytes));
    }
    std::uint32_t archiveChunkID = 0, archiveChunkSize = 0;
    std::uint64_t archiveOffset = 0;
    std::int32_t archiveCompressed = 0;
    assert(st_xray_archive_chunk_info(handle, 0, 0, &archiveChunkID,
                                      &archiveOffset, &archiveChunkSize, &archiveCompressed) == 0);
    assert(archiveChunkID == 1 && archiveOffset == 8 &&
           archiveChunkSize == 3 && archiveCompressed == 1);
    assert(st_xray_archive_chunk_info(handle, 0, 1, &archiveChunkID,
                                      &archiveOffset, &archiveChunkSize, &archiveCompressed) == 0);
    assert(archiveChunkID == 2 && archiveOffset == 19 &&
           archiveChunkSize == 2 && archiveCompressed == 0);
    unsigned char chunkPayload[3] = {};
    assert(st_xray_archive_chunk_read(handle, 0, 0, 0, chunkPayload, 3) == 0);
    assert(chunkPayload[0] == 9 && chunkPayload[1] == 8 && chunkPayload[2] == 7);
    assert(st_xray_archive_chunk_read(handle, 0, 1, 1, chunkPayload, 1) == 0);
    assert(chunkPayload[0] == 5);
    assert(st_xray_archive_chunk_read(handle, 0, 0, 2, chunkPayload, 2) == -3);
    assert(st_xray_archive_chunk_read(handle, 0, 0, 0, chunkPayload, 65537) == -1);
    assert(st_xray_archive_chunk_info(handle, 0, 2, &archiveChunkID,
                                      &archiveOffset, &archiveChunkSize, &archiveCompressed) == -2);
    assert(st_xray_archive_chunk_info(handle, 0, 0, nullptr,
                                      &archiveOffset, &archiveChunkSize, &archiveCompressed) == -1);
    // Upstream X-Ray directory record: u16 length, three u32 fields,
    // raw filename bytes, then u32 absolute archive data offset.
    {
        std::ofstream file(archive, std::ios::binary | std::ios::trunc);
        const unsigned char directory[] = {
            1,0,0,0, 26,0,0,0,
            24,0, 4,0,0,0, 4,0,0,0, 0x78,0x56,0x34,0x12,
            't','e','s','t','.','l','t','x', 42,0,0,0
        };
        file.write(reinterpret_cast<const char*>(directory), sizeof(directory));
        const unsigned char padding[8] = {};
        file.write(reinterpret_cast<const char*>(padding), sizeof(padding));
        file.write("DATA", 4);
    }
    // Keep the second archive structurally valid but empty until the override test.
    { std::ofstream empty(secondArchive, std::ios::binary | std::ios::trunc);
      const unsigned char header[8] = {1,0,0,0, 0,0,0,0};
      empty.write(reinterpret_cast<const char*>(header), sizeof(header)); }
    char entryName[32] {};
    uint32_t realSize = 0, compressedSize = 0, entryCRC = 0, dataOffset = 0;
    assert(st_xray_archive_directory_entry(handle, 0, 0, entryName, sizeof(entryName),
                                           &realSize, &compressedSize, &entryCRC, &dataOffset) == 0);
    assert(std::strcmp(entryName, "test.ltx") == 0);
    assert(st_xray_archive_directory_count(handle, 0) == 1);
    // Reject a malicious parent-directory path without reading any file bytes.
    {
        std::fstream file(archive, std::ios::binary | std::ios::in | std::ios::out);
        file.seekp(22);
        file.write("../x.ltx", 8);
    }
    assert(st_xray_archive_directory_entry(handle, 0, 0, entryName, sizeof(entryName),
                                           &realSize, &compressedSize, &entryCRC, &dataOffset) == -3);
    {
        std::fstream file(archive, std::ios::binary | std::ios::in | std::ios::out);
        file.seekp(22);
        file.write("test.ltx", 8);
    }

    assert(realSize == 4 && compressedSize == 4 && entryCRC == 0x12345678 && dataOffset == 42);
    char extracted[4] = {};
    assert(st_xray_archive_file_read(handle, 0, 0, 0, extracted, 4) == 0);
    assert(std::memcmp(extracted, "DATA", 4) == 0);
    // Metadata probes must not decompress game assets or allocate their bodies.
    uint32_t probeReal = 99, probeStored = 99;
    assert(st_xray_packed_resource_info(handle, "test.ltx", &probeReal, &probeStored) == 0);
    assert(probeReal == 4 && probeStored == 4);
    assert(st_xray_packed_resource_info(handle, "TEST.LTX", &probeReal, &probeStored) == 0);
    assert(probeReal == 4 && probeStored == 4);
    assert(st_xray_packed_resource_info(handle, "missing.ltx", &probeReal, &probeStored) == -2);
    assert(probeReal == 0 && probeStored == 0);
    assert(st_xray_packed_resource_info(handle, "../escape", &probeReal, &probeStored) == -1);
    assert(st_xray_packed_resource_info(handle, "test.ltx", nullptr, &probeStored) == -1);
    uint32_t geometrySize = 99, collisionSize = 99;
    assert(st_xray_packed_level_info(handle, "zaton", &geometrySize, &collisionSize) == -2);
    assert(geometrySize == 0 && collisionSize == 0);
    assert(st_xray_packed_level_info(handle, "../zaton", &geometrySize, &collisionSize) == -1);
    assert(st_xray_packed_level_info(handle, "zaton/level", &geometrySize, &collisionSize) == -1);
    assert(st_xray_packed_level_info(handle, "", &geometrySize, &collisionSize) == -1);
    assert(st_xray_packed_level_info(handle, "zaton", nullptr, &collisionSize) == -1);
    assert(st_xray_packed_level_chunk_count(handle, "zaton") == -2);
    assert(st_xray_packed_level_chunk_count(handle, "../zaton") == -1);
    assert(st_xray_packed_level_chunk_count(handle, "") == -1);
    uint32_t packedVertices = 0, packedTriangles = 0;
    float packedMin[3] {}, packedMax[3] {};
    assert(st_xray_packed_level_collision_info(handle, "zaton",
        &packedVertices, &packedTriangles, packedMin, packedMax) == -2);
    assert(st_xray_packed_level_collision_info(handle, "../zaton",
        &packedVertices, &packedTriangles, packedMin, packedMax) == -1);
    assert(st_xray_packed_level_collision_info(handle, "zaton",
        nullptr, &packedTriangles, packedMin, packedMax) == -1);

    char levelBuffer[8] = {'U','N','T','O','U','C','H','D'};
    uint32_t levelBytes = 99;
    assert(st_xray_load_packed_level_file(
        handle, "zaton", 0, levelBuffer, sizeof(levelBuffer), &levelBytes) == -2);
    assert(levelBytes == 0 && std::memcmp(levelBuffer, "UNTOUCHD", 8) == 0);
    assert(st_xray_load_packed_level_file(
        handle, "zaton", 1, levelBuffer, sizeof(levelBuffer), &levelBytes) == -2);
    assert(st_xray_load_packed_level_file(
        handle, "zaton", 2, levelBuffer, sizeof(levelBuffer), &levelBytes) == -1);
    assert(st_xray_load_packed_level_file(
        handle, "../zaton", 0, levelBuffer, sizeof(levelBuffer), &levelBytes) == -1);
    assert(st_xray_load_packed_level_file(
        handle, "zaton/level", 0, levelBuffer, sizeof(levelBuffer), &levelBytes) == -1);
    assert(st_xray_load_packed_level_file(
        handle, "zaton", 0, levelBuffer, sizeof(levelBuffer), nullptr) == -1);

    assert(st_xray_archive_find_file(handle, 0, "test.ltx") == 0);
    assert(st_xray_archive_find_file(handle, 0, "TEST.LTX") == 0);
    assert(st_xray_archive_find_file(handle, 0, "Test.Ltx") == 0);
    assert(st_xray_archive_find_file(handle, 0, "missing.ltx") == -2);
    uint32_t resourceArchive = 99, resourceEntry = 99;
    assert(st_xray_find_resource(handle, "test.ltx", &resourceArchive, &resourceEntry) == 0);
    assert(resourceArchive == 0 && resourceEntry == 0);
    char loadedResource[4] {};
    uint32_t loadedSize = 0;
    assert(st_xray_load_packed_resource(
        handle, "test.ltx", loadedResource, sizeof(loadedResource), &loadedSize) == 0);
    assert(loadedSize == 4 && std::memcmp(loadedResource, "DATA", 4) == 0);
    // Later gamedata archives override earlier files with the same path.
    {
        std::ofstream overrideFile(secondArchive, std::ios::binary | std::ios::trunc);
        const unsigned char directory[] = {
            1,0,0,0, 26,0,0,0,
            24,0, 4,0,0,0, 4,0,0,0, 0,0,0,0,
            't','e','s','t','.','l','t','x', 42,0,0,0
        };
        overrideFile.write(reinterpret_cast<const char*>(directory), sizeof(directory));
        const unsigned char padding[8] {};
        overrideFile.write(reinterpret_cast<const char*>(padding), sizeof(padding));
        overrideFile.write("NEW!", 4);
    }
    assert(st_xray_find_resource(handle, "test.ltx", &resourceArchive, &resourceEntry) == 0);
    assert(resourceArchive == 1 && resourceEntry == 0);
    assert(st_xray_find_resource(handle, "TEST.LTX", &resourceArchive, &resourceEntry) == 0);
    assert(resourceArchive == 1 && resourceEntry == 0);
    assert(st_xray_load_packed_resource(
        handle, "test.ltx", loadedResource, sizeof(loadedResource), &loadedSize) == 0);
    assert(loadedSize == 4 && std::memcmp(loadedResource, "NEW!", 4) == 0);
    assert(st_xray_load_packed_resource(
        handle, "TEST.LTX", loadedResource, sizeof(loadedResource), &loadedSize) == 0);
    assert(loadedSize == 4 && std::memcmp(loadedResource, "NEW!", 4) == 0);
    // Restore a valid empty directory so later lookups fall back to db0.
    { std::ofstream restore(secondArchive, std::ios::binary | std::ios::trunc);
      const unsigned char header[8] = {1,0,0,0, 0,0,0,0};
      restore.write(reinterpret_cast<const char*>(header), sizeof(header)); }
    assert(st_xray_find_resource(handle, "test.ltx", &resourceArchive, &resourceEntry) == 0);
    assert(resourceArchive == 0 && resourceEntry == 0);
    assert(st_xray_load_packed_resource(
        handle, "test.ltx", loadedResource, 3, &loadedSize) == -3);
    assert(st_xray_load_packed_resource(
        handle, "test.ltx", nullptr, 0, &loadedSize) == -3);
    assert(loadedSize == 4);
    assert(st_xray_load_packed_resource(
        handle, "../test.ltx", loadedResource, sizeof(loadedResource), &loadedSize) == -1);
    assert(loadedSize == 0);
    assert(st_xray_load_packed_resource(
        handle, "missing.ltx", loadedResource, sizeof(loadedResource), &loadedSize) == -2);
    assert(loadedSize == 0);
    assert(st_xray_archive_file_read(handle, 0, 0, 2, extracted, 3) == -3);
    // A corrupt directory record must not redirect file reads into metadata.
    {
        std::fstream file(archive, std::ios::binary | std::ios::in | std::ios::out);
        file.seekp(30); // pointer field in the first record
        const unsigned char badOffset[4] = {12, 0, 0, 0};
        file.write(reinterpret_cast<const char*>(badOffset), 4);
    }
    assert(st_xray_archive_file_read(handle, 0, 0, 0, extracted, 4) == -3);
    char protectedBuffer[4] = {'S', 'A', 'F', 'E'};
    uint32_t protectedSize = 0;
    assert(st_xray_load_packed_resource(
        handle, "test.ltx", protectedBuffer, sizeof(protectedBuffer), &protectedSize) == -3);
    assert(protectedSize == 4);
    assert(std::memcmp(protectedBuffer, "SAFE", 4) == 0);
    {
        std::fstream file(archive, std::ios::binary | std::ios::in | std::ios::out);
        file.seekp(30);
        const unsigned char goodOffset[4] = {42, 0, 0, 0};
        file.write(reinterpret_cast<const char*>(goodOffset), 4);
    }

    // A compressed file must never be returned as if its bytes were decoded.
    {
        std::fstream file(archive, std::ios::binary | std::ios::in | std::ios::out);
        file.seekp(14); // compressed_size field of the first directory record
        const unsigned char compressedSize[4] = {3, 0, 0, 0};
        file.write(reinterpret_cast<const char*>(compressedSize), 4);
    }
    assert(st_xray_archive_file_read(handle, 0, 0, 0, extracted, 3) == -4);
    {
        std::fstream file(archive, std::ios::binary | std::ios::in | std::ios::out);
        file.seekp(14);
        const unsigned char restoredSize[4] = {4, 0, 0, 0};
        file.write(reinterpret_cast<const char*>(restoredSize), 4);
    }

    // End-to-end compressed file extraction from a synthetic X-Ray archive.
    {
        const unsigned char original[] = "compressed X-Ray resource test resource test";
        unsigned char packed[256] {};
        lzo_uint packed_size = sizeof(packed);
        void* work = std::malloc(LZO1X_1_MEM_COMPRESS);
        assert(work && lzo_init() == LZO_E_OK);
        assert(lzo1x_1_compress(original, sizeof(original), packed, &packed_size, work) == LZO_E_OK);
        std::free(work);
        std::ofstream file(archive, std::ios::binary | std::ios::trunc);
        const unsigned char header[] = {1,0,0,0, 26,0,0,0, 24,0};
        file.write(reinterpret_cast<const char*>(header), sizeof(header));
        const uint32_t real = sizeof(original);
        const uint32_t compressed = static_cast<uint32_t>(packed_size);
        const uint32_t checksum = 0;
        const uint32_t offset = 42;
        file.write(reinterpret_cast<const char*>(&real), 4);
        file.write(reinterpret_cast<const char*>(&compressed), 4);
        file.write(reinterpret_cast<const char*>(&checksum), 4);
        file.write("test.ltx", 8);
        file.write(reinterpret_cast<const char*>(&offset), 4);
        const unsigned char padding[8] {};
        file.write(reinterpret_cast<const char*>(padding), sizeof(padding));
        file.write(reinterpret_cast<const char*>(packed), packed_size);
        file.close();
        unsigned char decoded[sizeof(original)] {};
        assert(st_xray_archive_file_decompress(handle, 0, 0, decoded, sizeof(decoded)) == 0);
        assert(std::memcmp(decoded, original, sizeof(original)) == 0);
        uint32_t packedLoadedSize = 0;
        unsigned char packedLoaded[sizeof(original)] {};
        assert(st_xray_load_packed_resource(
            handle, "test.ltx", packedLoaded, sizeof(packedLoaded), &packedLoadedSize) == 0);
        assert(packedLoadedSize == sizeof(original));
        assert(st_xray_load_packed_resource(
            handle, "TEST.LTX", packedLoaded, sizeof(packedLoaded), &packedLoadedSize) == 0);
        assert(std::memcmp(packedLoaded, original, sizeof(original)) == 0);
        assert(st_xray_load_packed_resource(
            handle, "test.ltx", packedLoaded, 3, &packedLoadedSize) == -3);
        assert(packedLoadedSize == sizeof(original));
        assert(st_xray_archive_file_decompress(handle, 0, 0, decoded, 3) == -3);
        {
            std::fstream corrupt(archive, std::ios::binary | std::ios::in | std::ios::out);
            corrupt.seekp(42);
            const unsigned char bad = 0xff;
            corrupt.write(reinterpret_cast<const char*>(&bad), 1);
        }
        assert(st_xray_archive_file_decompress(handle, 0, 0, decoded, sizeof(decoded)) != 0);
        unsigned char untouched[sizeof(original)];
        std::memset(untouched, 0xa5, sizeof(untouched));
        uint32_t corruptSize = 0;
        assert(st_xray_load_packed_resource(
            handle, "test.ltx", untouched, sizeof(untouched), &corruptSize) != 0);
        assert(corruptSize == sizeof(original));
        for (unsigned char value : untouched) assert(value == 0xa5);
    }
    // Restore plain directory fixture for the compressed-directory test.
    {
        std::ofstream file(archive, std::ios::binary | std::ios::trunc);
        const unsigned char directory[] = {
            1,0,0,0, 26,0,0,0,
            24,0, 4,0,0,0, 4,0,0,0, 0x78,0x56,0x34,0x12,
            't','e','s','t','.','l','t','x', 42,0,0,0
        };
        file.write(reinterpret_cast<const char*>(directory), sizeof(directory));
        const unsigned char padding[8] {};
        file.write(reinterpret_cast<const char*>(padding), sizeof(padding));
        file.write("DATA", 4);
    }
    // End-to-end decoding of an LZO-compressed X-Ray directory chunk.
    {
        const unsigned char directory_record[] = {
            24,0, 4,0,0,0, 4,0,0,0, 0x78,0x56,0x34,0x12,
            't','e','s','t','.','l','t','x', 42,0,0,0
        };
        unsigned char packed[256] {};
        lzo_uint packed_size = sizeof(packed);
        void* work = std::malloc(LZO1X_1_MEM_COMPRESS);
        assert(work && lzo_init() == LZO_E_OK);
        assert(lzo1x_1_compress(directory_record, sizeof(directory_record),
                               packed, &packed_size, work) == LZO_E_OK);
        std::free(work);
        {
            std::ofstream file(archive, std::ios::binary | std::ios::trunc);
            const unsigned char header[] = {1,0,0,128};
            const uint32_t size = static_cast<uint32_t>(packed_size);
            file.write(reinterpret_cast<const char*>(header), sizeof(header));
            file.write(reinterpret_cast<const char*>(&size), 4);
            file.write(reinterpret_cast<const char*>(packed), packed_size);
        }
        unsigned char decoded[sizeof(directory_record)] {};
        assert(st_xray_archive_directory_decompress(handle, 0, decoded,
               sizeof(decoded), sizeof(directory_record)) == 0);
        assert(std::memcmp(decoded, directory_record, sizeof(directory_record)) == 0);
        assert(st_xray_archive_directory_decompress(handle, 0, decoded,
               sizeof(decoded), sizeof(directory_record) - 1) == -4);
        assert(st_xray_archive_directory_decompress(handle, 0, decoded,
               4, sizeof(directory_record)) == -1);
        uint32_t auto_size = 0;
        assert(st_xray_archive_directory_auto_decompress(
            handle, 0, decoded, sizeof(decoded), &auto_size) == 0);
        assert(auto_size == sizeof(directory_record));
        assert(std::memcmp(decoded, directory_record, auto_size) == 0);
        assert(st_xray_archive_directory_auto_decompress(
            handle, 0, decoded, 4, &auto_size) == -4);
        assert(auto_size == 0);
        int32_t key_used = -2;
        assert(st_xray_archive_directory_auto_decrypt(
            handle, 0, decoded, sizeof(decoded), &auto_size, &key_used) == 0);
        assert(key_used == -1 && auto_size == sizeof(directory_record));
        assert(std::memcmp(decoded, directory_record, auto_size) == 0);
        assert(st_xray_archive_directory_auto_decrypt(
            handle, 0, decoded, 4, &auto_size, &key_used) == -4);
        assert(key_used == -2 && auto_size == 0);
        // The production directory parser now supports compressed catalogs.
        assert(st_xray_archive_directory_count(handle, 0) == 1);
        char compressedName[32] {};
        uint32_t decodedReal = 0, decodedPacked = 0, decodedCRC = 0, decodedOffset = 0;
        assert(st_xray_archive_directory_entry(
            handle, 0, 0, compressedName, sizeof(compressedName),
            &decodedReal, &decodedPacked, &decodedCRC, &decodedOffset) == 0);
        assert(std::strcmp(compressedName, "test.ltx") == 0);
        assert(decodedReal == 4 && decodedPacked == 4 && decodedOffset == 42);
        assert(st_xray_archive_find_file(handle, 0, "test.ltx") == 0);
        assert(st_xray_archive_find_file(handle, 0, "TEST.LTX") == 0);
        assert(st_xray_archive_find_file(handle, 0, "Test.Ltx") == 0);
        assert(st_xray_archive_find_file(handle, 0, "missing.ltx") == -2);
        uint32_t locatedArchive = 99, locatedEntry = 99;
        assert(st_xray_find_resource(
            handle, "test.ltx", &locatedArchive, &locatedEntry) == 0);
        assert(locatedArchive == 0 && locatedEntry == 0);
        uint32_t required = 0;
        assert(st_xray_load_packed_resource(
            handle, "test.ltx", nullptr, 0, &required) == -3);
        assert(required == 4);
        assert(st_xray_load_packed_resource(
            handle, "missing.ltx", nullptr, 0, &required) == -2);
    }
    // Decoded catalog records must be validated before use.
    {
        unsigned char record[] = {
            24,0, 4,0,0,0, 4,0,0,0, 0,0,0,0,
            't','e','s','t','.','l','t','x', 42,0,0,0
        };
        assert(st_xray_validate_directory_records(record, sizeof(record)) == 1);
        assert(st_xray_validate_directory_records(record, sizeof(record) - 1) == -3);
        record[14] = '/';
        assert(st_xray_validate_directory_records(record, sizeof(record)) == -3);
        record[14] = 't';
        record[15] = 0;
        assert(st_xray_validate_directory_records(record, sizeof(record)) == -3);
    }
    // Test both legacy encryption keys against actual compressed directory payloads.
    for (int key = 0; key < 2; ++key) {
      for (int malformed = 0; malformed < 2; ++malformed) {
        unsigned char record[] = {
            24,0, 4,0,0,0, 4,0,0,0, 0x78,0x56,0x34,0x12,
            't','e','s','t','.','l','t','x', 42,0,0,0
        };
        if (malformed) record[14] = '/';
        unsigned char packed[256] {};
        lzo_uint packed_size = sizeof(packed);
        void* work = std::malloc(LZO1X_1_MEM_COMPRESS);
        assert(work && lzo_init() == LZO_E_OK);
        assert(lzo1x_1_compress(record, sizeof(record), packed, &packed_size, work) == LZO_E_OK);
        std::free(work);
        // Independent upstream-compatible encoder (inverse of the production decoder).
        uint32_t seed = key == 0 ? 6011979u : 20091958u;
        const auto next = [&seed](uint32_t range) -> uint32_t {
            seed = 0x08088405u * seed + 1u;
            return static_cast<uint32_t>((static_cast<uint64_t>(seed) * range) >> 32u);
        };
        unsigned char alphabet[256] {};
        for (unsigned i = 0; i < 256; ++i) alphabet[i] = static_cast<unsigned char>(i);
        for (int i = 0; i < (key == 0 ? 1024 : 2048); ++i) {
            const uint32_t j = next(256);
            uint32_t k = next(256);
            while (j == k) k = next(256);
            const unsigned char tmp = alphabet[j];
            alphabet[j] = alphabet[k];
            alphabet[k] = tmp;
        }
        seed = key == 0 ? 24031979u : 20031955u;
        for (lzo_uint i = 0; i < packed_size; ++i)
            packed[i] = alphabet[packed[i]] ^ static_cast<unsigned char>(next(256));
        {
            std::ofstream file(archive, std::ios::binary | std::ios::trunc);
            const unsigned char header[] = {1,0,0,128};
            const uint32_t size = static_cast<uint32_t>(packed_size);
            file.write(reinterpret_cast<const char*>(header), sizeof(header));
            file.write(reinterpret_cast<const char*>(&size), 4);
            file.write(reinterpret_cast<const char*>(packed), packed_size);
        }
        unsigned char decoded[sizeof(record)] {};
        uint32_t actual_size = 0;
        int32_t key_used = -2;
        const int32_t result = st_xray_archive_directory_auto_decrypt(
            handle, 0, decoded, sizeof(decoded), &actual_size, &key_used);
        if (malformed) {
            assert(result == -4 && actual_size == 0 && key_used == -2);
        } else {
            assert(result == 0 && key_used == key && actual_size == sizeof(record));
            assert(std::memcmp(decoded, record, sizeof(record)) == 0);
        }
      }
    }
    // Restore an uncompressed directory for subsequent corruption tests.
    {
        std::ofstream file(archive, std::ios::binary | std::ios::trunc);
        const unsigned char directory[] = {
            1,0,0,0, 26,0,0,0,
            24,0, 4,0,0,0, 4,0,0,0, 0x78,0x56,0x34,0x12,
            't','e','s','t','.','l','t','x', 42,0,0,0
        };
        file.write(reinterpret_cast<const char*>(directory), sizeof(directory));
        const unsigned char padding[8] {};
        file.write(reinterpret_cast<const char*>(padding), sizeof(padding));
        file.write("DATA", 4);
    }
    // X-Ray marks a compressed directory by setting the high bit of chunk ID.
    // Never interpret its compressed bytes as raw file records.
    {
        std::fstream file(archive, std::ios::binary | std::ios::in | std::ios::out);
        file.seekp(3);
        const unsigned char compressionFlag = 0x80;
        file.write(reinterpret_cast<const char*>(&compressionFlag), 1);
    }
    assert(st_xray_archive_directory_entry(handle, 0, 0, entryName, sizeof(entryName),
                                           &realSize, &compressedSize, &entryCRC, &dataOffset) == -4);
    assert(st_xray_archive_directory_count(handle, 0) == -4);
    {
        std::fstream file(archive, std::ios::binary | std::ios::in | std::ios::out);
        file.seekp(3);
        const unsigned char plainFlag = 0;
        file.write(reinterpret_cast<const char*>(&plainFlag), 1);
    }
    assert(st_xray_archive_directory_entry(handle, 0, 1, entryName, sizeof(entryName),
                                           &realSize, &compressedSize, &entryCRC, &dataOffset) == -2);
    // Reject malformed chunk sizes before attempting to access a payload.
    {
        std::ofstream file(archive, std::ios::binary | std::ios::trunc);
        const unsigned char malformed[] = {1,0,0,0, 255,255,255,127, 1};
        file.write(reinterpret_cast<const char*>(malformed), sizeof(malformed));
    }
    assert(st_xray_archive_chunk_info(handle, 0, 0, &archiveChunkID,
                                      &archiveOffset, &archiveChunkSize, &archiveCompressed) == -3);
    // Reject an incomplete eight-byte chunk header.
    {
        std::ofstream file(archive, std::ios::binary | std::ios::trunc);
        const unsigned char truncated[] = {1,0,0,0, 2,0,0};
        file.write(reinterpret_cast<const char*>(truncated), sizeof(truncated));
    }
    assert(st_xray_archive_chunk_info(handle, 0, 0, &archiveChunkID,
                                      &archiveOffset, &archiveChunkSize, &archiveCompressed) == -3);
    { std::ofstream file(archive, std::ios::binary | std::ios::trunc); file << "fixture"; }
    assert(st_xray_has_loose_data(handle) == 0);
    assert(st_xray_level_count(handle) == 0);
    assert(st_xray_level_chunk_count(handle, 0) == -1);
    assert(st_xray_level_name(handle, 0, nullptr, 0) == -1);
    st_xray_shutdown(handle);
    assert(st_xray_archive_count(handle) == 0);
    assert(st_xray_archive_bytes(handle) == 0);
    assert(st_xray_archive_name(handle, 0, archiveName, sizeof(archiveName)) == -2);
    assert(st_xray_initialize(handle, 2) == 0);
    const std::string data = std::string(path) + "/gamedata";
    const std::string levels = data + "/levels";
    const std::string zaton = levels + "/zaton";
    assert(mkdir(data.c_str(), 0700) == 0);
    assert(mkdir(levels.c_str(), 0700) == 0);
    assert(mkdir(zaton.c_str(), 0700) == 0);
    {
        std::ofstream file(zaton + "/level", std::ios::binary);
        const unsigned char bytes[] = {7,0,0,0,2,0,0,0,11,12};
        file.write(reinterpret_cast<const char*>(bytes), sizeof(bytes));
    }
    assert(st_xray_attach_resources(handle, path) == 0);
    assert(st_xray_level_count(handle) == 1);
    char levelName[16] = {};
    assert(st_xray_level_name(handle, 0, levelName, sizeof(levelName)) == 0);
    assert(std::strcmp(levelName, "zaton") == 0);
    assert(st_xray_level_name(handle, 0, levelName, 2) == -3);
    assert(st_xray_level_chunk_count(handle, 0) == 1);
    std::uint32_t chunkID = 0, chunkSize = 0;
    std::uint64_t offset = 0;
    std::int32_t compressed = -1;
    assert(st_xray_level_chunk_info(handle, 0, 0, &chunkID, &offset, &chunkSize, &compressed) == 0);
    assert(chunkID == 7 && offset == 8 && chunkSize == 2 && compressed == 0);
    assert(st_xray_level_chunk_info(handle, 0, 1, &chunkID, &offset, &chunkSize, &compressed) == -1);
    const std::string cform = zaton + "/level.cform";
    {
        std::ofstream file(cform, std::ios::binary);
        const unsigned char header[] = {
            4,0,0,0, 1,0,0,0, 0,0,0,0,
            0,0,0,0, 0,0,0,0, 0,0,0,0,
            0,0,128,63, 0,0,128,63, 0,0,128,63,
            0,0,0,0, 0,0,0,0, 0,0,0,0
        };
        file.write(reinterpret_cast<const char*>(header), sizeof(header));
    }
    std::uint32_t vertices = 0, triangles = 0;
    float minimum[3] = {}, maximum[3] = {};
    assert(st_xray_level_collision_info(handle, 0, &vertices, &triangles, minimum, maximum) == 0);
    assert(vertices == 1 && triangles == 0 && minimum[0] == 0.0f && maximum[0] == 1.0f);
    assert(st_xray_level_collision_info(handle, 1, &vertices, &triangles, minimum, maximum) == -1);
    // End-to-end packed level fixture: both geometry and collision data
    // are loaded from gamedata.db0 and inspected through the public bridge.
    {
        const unsigned char packedGeometry[] = {7,0,0,0, 2,0,0,0, 0xAA,0xBB};
        // A second geometry chunk uses the real minilzo compressor. It is
        // stored inside the archive and must survive the complete loader path.
        const unsigned char originalChunk[] = "X-Ray packed geometry LZO integration";
        unsigned char compressedChunk[256] {};
        lzo_uint compressedChunkSize = sizeof(compressedChunk);
        void* chunkWork = std::malloc(LZO1X_1_MEM_COMPRESS);
        assert(chunkWork && lzo_init() == LZO_E_OK);
        assert(lzo1x_1_compress(originalChunk, sizeof(originalChunk),
            compressedChunk, &compressedChunkSize, chunkWork) == LZO_E_OK);
        std::free(chunkWork);
        std::vector<unsigned char> packedGeometryData(
            packedGeometry, packedGeometry + sizeof(packedGeometry));
        const uint32_t compressedChunkID = 0x80000009u;
        const uint32_t compressedLength = static_cast<uint32_t>(compressedChunkSize);
        for (int shift = 0; shift < 32; shift += 8)
            packedGeometryData.push_back(static_cast<unsigned char>(compressedChunkID >> shift));
        for (int shift = 0; shift < 32; shift += 8)
            packedGeometryData.push_back(static_cast<unsigned char>(compressedLength >> shift));
        packedGeometryData.insert(packedGeometryData.end(),
            compressedChunk, compressedChunk + compressedChunkSize);
        const unsigned char packedCollision[] = {
            4,0,0,0, 1,0,0,0, 0,0,0,0,
            0,0,0,0, 0,0,0,0, 0,0,0,0,
            0,0,128,63, 0,0,128,63, 0,0,128,63,
            0,0,0,0, 0,0,0,0, 0,0,0,0
        };
        const std::string names[] = {"levels/zaton/level", "levels/zaton/level.cform"};
        std::vector<unsigned char> directory;
        auto append16 = [&](uint16_t value) {
            directory.push_back(static_cast<unsigned char>(value));
            directory.push_back(static_cast<unsigned char>(value >> 8));
        };
        auto append32 = [&](uint32_t value) {
            for (int shift = 0; shift < 32; shift += 8)
                directory.push_back(static_cast<unsigned char>(value >> shift));
        };
        const uint32_t directoryBytes = static_cast<uint32_t>(
            2 * (2 + 16) + names[0].size() + names[1].size());
        uint32_t fileOffset = 8 + directoryBytes;
        const uint32_t sizes[] = {static_cast<uint32_t>(packedGeometryData.size()), sizeof(packedCollision)};
        for (int index = 0; index < 2; ++index) {
            append16(static_cast<uint16_t>(16 + names[index].size()));
            append32(sizes[index]);
            append32(sizes[index]);
            append32(0);
            directory.insert(directory.end(), names[index].begin(), names[index].end());
            append32(fileOffset);
            fileOffset += sizes[index];
        }
        assert(directory.size() == directoryBytes);
        std::ofstream file(archive, std::ios::binary | std::ios::trunc);
        const unsigned char directoryHeader[] = {
            1,0,0,0,
            static_cast<unsigned char>(directoryBytes),
            static_cast<unsigned char>(directoryBytes >> 8),0,0
        };
        file.write(reinterpret_cast<const char*>(directoryHeader), sizeof(directoryHeader));
        file.write(reinterpret_cast<const char*>(directory.data()), directory.size());
        file.write(reinterpret_cast<const char*>(packedGeometryData.data()), packedGeometryData.size());
        file.write(reinterpret_cast<const char*>(packedCollision), sizeof(packedCollision));
        file.close();
        uint32_t geometryBytes = 0, collisionBytes = 0;
        assert(st_xray_packed_level_info(handle, "zaton", &geometryBytes, &collisionBytes) == 0);
        assert(geometryBytes == packedGeometryData.size());
        assert(collisionBytes == sizeof(packedCollision));
        assert(st_xray_packed_level_chunk_count(handle, "zaton") == 2);
        uint32_t packedChunkID = 0, packedChunkOffset = 0, packedChunkSize = 0;
        int32_t packedChunkCompressed = -1;
        assert(st_xray_packed_level_chunk_info(handle, "zaton", 0,
            &packedChunkID, &packedChunkOffset, &packedChunkSize, &packedChunkCompressed) == 0);
        unsigned char payload[2] = {0,0};
        uint32_t payloadBytes = 99;
        assert(st_xray_packed_level_chunk_read(handle, "zaton", 0,
            payload, sizeof(payload), &payloadBytes) == 0);
        assert(payloadBytes == 2 && payload[0] == 0xAA && payload[1] == 0xBB);
        uint32_t decodedChunkBytes = 99;
        unsigned char decodedChunk[2] = {0,0};
        assert(st_xray_packed_level_chunk_decode(handle, "zaton", 0,
            decodedChunk, sizeof(decodedChunk), 2, &decodedChunkBytes) == 0);
        assert(decodedChunkBytes == 2 && decodedChunk[0] == 0xAA && decodedChunk[1] == 0xBB);
        decodedChunk[0] = 0xEE;
        assert(st_xray_packed_level_chunk_decode(handle, "zaton", 0,
            decodedChunk, sizeof(decodedChunk), 1, &decodedChunkBytes) == -3);
        assert(decodedChunk[0] == 0xEE);
        assert(st_xray_packed_level_chunk_decode(handle, "zaton", 0,
            decodedChunk, 1, 2, &decodedChunkBytes) == -3);
        assert(st_xray_packed_level_chunk_decode(handle, "zaton", 2,
            decodedChunk, sizeof(decodedChunk), 2, &decodedChunkBytes) == -2);
        assert(st_xray_packed_level_chunk_decode(handle, "zaton", 0,
            decodedChunk, sizeof(decodedChunk), 2, nullptr) == -1);

        payload[0] = 0xEE;
        assert(st_xray_packed_level_chunk_read(handle, "zaton", 0,
            payload, 1, &payloadBytes) == -3);
        assert(payloadBytes == 2 && payload[0] == 0xEE);
        assert(st_xray_packed_level_chunk_read(handle, "zaton", 2,
            payload, sizeof(payload), &payloadBytes) == -2);
        assert(payloadBytes == 0);
        assert(st_xray_packed_level_chunk_read(handle, "../zaton", 0,
            payload, sizeof(payload), &payloadBytes) == -1);
        assert(st_xray_packed_level_chunk_read(handle, "zaton", 0,
            payload, sizeof(payload), nullptr) == -1);
        assert(st_xray_packed_level_chunk_read(handle, "zaton", 0,
            nullptr, 2, &payloadBytes) == -1);
        uint32_t compressedID = 0, compressedOffset = 0, compressedSize = 0;
        int32_t compressedFlag = 0;
        assert(st_xray_packed_level_chunk_info(handle, "zaton", 1,
            &compressedID, &compressedOffset, &compressedSize, &compressedFlag) == 0);
        assert(compressedID == 9 && compressedFlag == 1 &&
               compressedSize == compressedChunkSize);
        unsigned char expandedChunk[sizeof(originalChunk)] {};
        uint32_t expandedSize = 0;
        assert(st_xray_packed_level_chunk_decode(handle, "zaton", 1,
            expandedChunk, sizeof(expandedChunk), sizeof(originalChunk), &expandedSize) == 0);
        assert(expandedSize == sizeof(originalChunk));
        expandedChunk[0] = 0xEE;
        assert(st_xray_packed_level_chunk_decode(handle, "zaton", 1,
            expandedChunk, sizeof(expandedChunk) - 1, sizeof(originalChunk), &expandedSize) == -3);
        assert(expandedSize == 0 && expandedChunk[0] == 0xEE);
        assert(st_xray_packed_level_chunk_decode(handle, "zaton", 1,
            nullptr, sizeof(originalChunk), sizeof(originalChunk), &expandedSize) == -1);
        assert(expandedSize == 0);
        assert(st_xray_packed_level_chunk_decode(handle, "zaton", 1,
            expandedChunk, sizeof(expandedChunk), 64u * 1024u * 1024u + 1u,
            &expandedSize) == -3);
        assert(expandedSize == 0);
        assert(st_xray_packed_level_chunk_decode(handle, "zaton", 1,
            expandedChunk, sizeof(expandedChunk), sizeof(originalChunk), &expandedSize) == 0);

        assert(std::memcmp(expandedChunk, originalChunk, expandedSize) == 0);
        expandedChunk[0] = 0xEE;
        assert(st_xray_packed_level_chunk_decode(handle, "zaton", 1,
            expandedChunk, sizeof(expandedChunk), sizeof(originalChunk) - 1, &expandedSize) != 0);
        assert(expandedChunk[0] == 0xEE);
        // Truncate the compressed chunk body in the archive and verify
        // decompression failure cannot overwrite the caller's buffer.
        {
            std::fstream corrupt(archive, std::ios::binary | std::ios::in | std::ios::out);
            corrupt.seekp(8 + directoryBytes + sizeof(packedGeometry) + 8);
            std::vector<unsigned char> broken(compressedChunkSize, 0);
            corrupt.write(reinterpret_cast<const char*>(broken.data()), broken.size());
        }
        expandedChunk[0] = 0xEE;
        assert(st_xray_packed_level_chunk_decode(handle, "zaton", 1,
            expandedChunk, sizeof(expandedChunk), sizeof(originalChunk), &expandedSize) != 0);
        assert(expandedChunk[0] == 0xEE && expandedSize == 0);
        {
            std::fstream restore(archive, std::ios::binary | std::ios::in | std::ios::out);
            restore.seekp(8 + directoryBytes + sizeof(packedGeometry) + 8);
            restore.write(reinterpret_cast<const char*>(compressedChunk), compressedChunkSize);
        }
        assert(st_xray_packed_level_chunk_decode(handle, "zaton", 1,
            expandedChunk, sizeof(expandedChunk), sizeof(originalChunk), &expandedSize) == 0);
        assert(std::memcmp(expandedChunk, originalChunk, expandedSize) == 0);

        assert(packedChunkID == 7 && packedChunkOffset == 8 &&
               packedChunkSize == 2 && packedChunkCompressed == 0);
        assert(st_xray_packed_level_chunk_info(handle, "zaton", 2,
            &packedChunkID, &packedChunkOffset, &packedChunkSize, &packedChunkCompressed) == -2);
        assert(st_xray_packed_level_chunk_info(handle, "zaton", 0,
            nullptr, &packedChunkOffset, &packedChunkSize, &packedChunkCompressed) == -1);

        uint32_t packedVertices = 0, packedTriangles = 0;
        float packedMinimum[3] {}, packedMaximum[3] {};
        assert(st_xray_packed_level_collision_info(handle, "zaton",
            &packedVertices, &packedTriangles, packedMinimum, packedMaximum) == 0);
        assert(packedVertices == 1 && packedTriangles == 0 && packedMaximum[0] == 1.0f);
        // Packed DB archive -> decoded CForm -> renderer-ready vertex buffer.
        float packedXYZ[3] = {99.0f,99.0f,99.0f};
        uint32_t packedIndices[3] = {77,77,77};
        uint32_t outputVertices = 999, outputTriangles = 999;
        assert(st_xray_packed_level_collision_mesh(handle, "zaton",
            packedXYZ, 1, packedIndices, 1, &outputVertices, &outputTriangles) == 0);
        assert(outputVertices == 1 && outputTriangles == 0 && packedXYZ[0] == 0.0f);
        packedXYZ[0] = 99.0f;
        assert(st_xray_packed_level_collision_mesh(handle, "zaton",
            packedXYZ, 0, packedIndices, 1, &outputVertices, &outputTriangles) == -3);
        assert(outputVertices == 0 && outputTriangles == 0 && packedXYZ[0] == 99.0f);
        assert(st_xray_packed_level_collision_mesh(handle, "../zaton",
            packedXYZ, 1, packedIndices, 1, &outputVertices, &outputTriangles) == -1);
        assert(st_xray_packed_level_collision_mesh(handle, "zaton",
            packedXYZ, 1, packedIndices, 1, nullptr, &outputTriangles) == -1);

        // Invalid vertex coordinates and triangle indices must be rejected.
        {
            std::fstream corrupt(archive, std::ios::binary | std::ios::in | std::ios::out);
            const auto collisionOffset = 8 + directoryBytes + packedGeometryData.size();
            corrupt.seekp(collisionOffset + 36);
            const unsigned char nanVertex[4] = {0,0,192,127};
            corrupt.write(reinterpret_cast<const char*>(nanVertex), 4);
        }
        assert(st_xray_packed_level_collision_info(handle, "zaton",
            &packedVertices, &packedTriangles, packedMinimum, packedMaximum) == -3);
        {
            std::fstream restore(archive, std::ios::binary | std::ios::in | std::ios::out);
            const auto collisionOffset = 8 + directoryBytes + packedGeometryData.size();
            restore.seekp(collisionOffset + 36);
            const unsigned char zero[4] = {0,0,0,0};
            restore.write(reinterpret_cast<const char*>(zero), 4);
        }
        assert(st_xray_packed_level_collision_info(handle, "zaton",
            &packedVertices, &packedTriangles, packedMinimum, packedMaximum) == 0);

        std::vector<unsigned char> loadedGeometry(packedGeometryData.size());
        uint32_t loadedSize = 0;
        assert(st_xray_load_packed_level_file(handle, "zaton", 0,
            loadedGeometry.data(), static_cast<uint32_t>(loadedGeometry.size()), &loadedSize) == 0);
        assert(loadedSize == packedGeometryData.size());
        assert(std::memcmp(loadedGeometry.data(), packedGeometryData.data(), loadedSize) == 0);
        assert(st_xray_load_packed_level_file(handle, "zaton", 0,
            loadedGeometry.data(), 1, &loadedSize) == -3);
        assert(loadedSize == packedGeometryData.size());
        // Corrupt geometry chunk length: the packed inspector must reject it.
        {
            std::fstream corrupt(archive, std::ios::binary | std::ios::in | std::ios::out);
            corrupt.seekp(8 + directoryBytes + 4);
            const unsigned char badSize[4] = {255,255,255,127};
            corrupt.write(reinterpret_cast<const char*>(badSize), 4);
        }
        assert(st_xray_packed_level_chunk_count(handle, "zaton") == -3);
        // Restore geometry and corrupt collision bounds with a NaN.
        {
            std::fstream corrupt(archive, std::ios::binary | std::ios::in | std::ios::out);
            corrupt.seekp(8 + directoryBytes + 4);
            const unsigned char goodSize[4] = {2,0,0,0};
            corrupt.write(reinterpret_cast<const char*>(goodSize), 4);
            corrupt.seekp(8 + directoryBytes + packedGeometryData.size() + 12);
            const unsigned char nanBits[4] = {0,0,192,127};
            corrupt.write(reinterpret_cast<const char*>(nanBits), 4);
        }
        assert(st_xray_packed_level_chunk_count(handle, "zaton") == 2);
        assert(st_xray_packed_level_collision_info(handle, "zaton",
            &packedVertices, &packedTriangles, packedMinimum, packedMaximum) == -3);
    }
    st_xray_shutdown(handle);
    assert(st_xray_level_count(handle) == 0);
    assert(std::remove(cform.c_str()) == 0);
    assert(std::remove((zaton + "/level").c_str()) == 0);
    assert(rmdir(zaton.c_str()) == 0);
    assert(rmdir(levels.c_str()) == 0);
    assert(rmdir(data.c_str()) == 0);
    assert(st_xray_state(handle) == 0);
    st_xray_destroy(handle);
    assert(std::remove(archive.c_str()) == 0);
    assert(std::remove(secondArchive.c_str()) == 0);
    assert(std::remove(backupLike.c_str()) == 0);
    assert(rmdir(path) == 0);
}
