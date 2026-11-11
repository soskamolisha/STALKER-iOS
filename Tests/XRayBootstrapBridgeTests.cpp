#include "../Sources/Native/STXRayBootstrap.h"
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>
#include <sys/stat.h>
#include <cstring>
#include <unistd.h>

int main() {
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
    char entryName[32] {};
    uint32_t realSize = 0, compressedSize = 0, entryCRC = 0, dataOffset = 0;
    assert(st_xray_archive_directory_entry(handle, 0, 0, entryName, sizeof(entryName),
                                           &realSize, &compressedSize, &entryCRC, &dataOffset) == 0);
    assert(std::strcmp(entryName, "test.ltx") == 0);
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
    assert(st_xray_archive_file_read(handle, 0, 0, 2, extracted, 3) == -3);
    // A corrupt directory record must not redirect file reads into metadata.
    {
        std::fstream file(archive, std::ios::binary | std::ios::in | std::ios::out);
        file.seekp(30); // pointer field in the first record
        const unsigned char badOffset[4] = {12, 0, 0, 0};
        file.write(reinterpret_cast<const char*>(badOffset), 4);
    }
    assert(st_xray_archive_file_read(handle, 0, 0, 0, extracted, 4) == -3);
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
