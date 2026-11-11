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
