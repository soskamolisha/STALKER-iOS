#include <cassert>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
extern "C" int32_t st_xray_probe_shadow_resources(const char*, uint32_t*, uint64_t*, uint32_t*);
extern "C" int32_t st_xray_probe_first_archive_chunks(const char*, uint32_t*);
extern "C" int32_t st_xray_probe_first_archive_chunk_info(const char*, uint32_t, uint32_t*, uint32_t*, int32_t*);
extern "C" int32_t st_xray_probe_first_archive_chunk_read(const char*, uint32_t, void*, uint32_t, uint32_t*);
int main() {
    uint32_t count = 99, levels = 99;
    uint64_t bytes = 99;
    assert(st_xray_probe_shadow_resources(nullptr, &count, &bytes, &levels) == -1);
    char directory[] = "/tmp/stalker-native-probe-XXXXXX";
    char* root = mkdtemp(directory);
    assert(root);
    assert(st_xray_probe_shadow_resources(root, &count, &bytes, &levels) == -3);
    assert(count == 0 && bytes == 0 && levels == 0);
    const std::string archive = std::string(root) + "/gamedata.db0";
    { std::ofstream out(archive, std::ios::binary); out << "fixture"; }
    assert(st_xray_probe_shadow_resources(root, &count, &bytes, &levels) == 0);
    assert(count == 1 && bytes == 7 && levels == 0);
    uint32_t chunks = 99;
    assert(st_xray_probe_first_archive_chunks(root, &chunks) == -4);
    assert(chunks == 0);
    // A valid X-Ray top-level chunk: ID=1, payload size=3, "abc".
    { std::ofstream out(archive, std::ios::binary | std::ios::trunc);
      const unsigned char header[] = {1,0,0,0,3,0,0,0,'a','b','c'};
      out.write(reinterpret_cast<const char*>(header), sizeof(header)); }
    assert(st_xray_probe_first_archive_chunks(root, &chunks) == 0);
    assert(chunks == 1);
    uint32_t id = 0, size = 0;
    int32_t compressed = -1;
    assert(st_xray_probe_first_archive_chunk_info(root, 0, &id, &size, &compressed) == 0);
    assert(id == 1 && size == 3 && compressed == 0);
    char payload[4] = {};
    uint32_t actual = 99;
    assert(st_xray_probe_first_archive_chunk_read(root, 0, payload, 2, &actual) == -6);
    assert(actual == 0);
    assert(st_xray_probe_first_archive_chunk_read(root, 0, payload, 3, &actual) == 0);
    assert(actual == 3 && std::string(payload, 3) == "abc");
    assert(st_xray_probe_first_archive_chunk_read(root, 1, payload, 3, &actual) == -2);
    assert(st_xray_probe_first_archive_chunk_info(root, 1, &id, &size, &compressed) == -2);
    // Two adjacent chunks must retain their own offsets and payload boundaries.
    { std::ofstream out(archive, std::ios::binary | std::ios::trunc);
      const unsigned char twoChunks[] = {
          1,0,0,0,3,0,0,0,'a','b','c',
          2,0,0,0,4,0,0,0,'d','e','f','g'
      };
      out.write(reinterpret_cast<const char*>(twoChunks), sizeof(twoChunks)); }
    assert(st_xray_probe_first_archive_chunks(root, &chunks) == 0);
    assert(chunks == 2);
    assert(st_xray_probe_first_archive_chunk_info(root, 1, &id, &size, &compressed) == 0);
    assert(id == 2 && size == 4 && compressed == 0);
    assert(st_xray_probe_first_archive_chunk_read(root, 1, payload, 4, &actual) == 0);
    assert(actual == 4 && std::string(payload, 4) == "defg");
    assert(st_xray_probe_first_archive_chunk_read(root, 0, payload, 4, &actual) == 0);
    assert(actual == 3 && std::string(payload, 3) == "abc");
    // Empty payloads are legal, including with a null destination and zero capacity.
    { std::ofstream out(archive, std::ios::binary | std::ios::trunc);
      const unsigned char emptyChunk[] = {3,0,0,0,0,0,0,0};
      out.write(reinterpret_cast<const char*>(emptyChunk), sizeof(emptyChunk)); }
    assert(st_xray_probe_first_archive_chunks(root, &chunks) == 0);
    assert(chunks == 1);
    assert(st_xray_probe_first_archive_chunk_read(root, 0, nullptr, 0, &actual) == 0);
    assert(actual == 0);
    // Reject a partial trailing chunk header.
    { std::ofstream out(archive, std::ios::binary | std::ios::trunc);
      const unsigned char trailing[] = {3,0,0,0,0,0,0,0,1,2,3};
      out.write(reinterpret_cast<const char*>(trailing), sizeof(trailing)); }
    assert(st_xray_probe_first_archive_chunks(root, &chunks) == -4);
    assert(chunks == 0);
    // A compressed chunk must be indexed but not returned as raw decoded data.
    { std::ofstream out(archive, std::ios::binary | std::ios::trunc);
      const unsigned char compressedHeader[] = {1,0,0,128,3,0,0,0,'a','b','c'};
      out.write(reinterpret_cast<const char*>(compressedHeader), sizeof(compressedHeader)); }
    assert(st_xray_probe_first_archive_chunks(root, &chunks) == 0);
    assert(chunks == 1);
    assert(st_xray_probe_first_archive_chunk_info(root, 0, &id, &size, &compressed) == 0);
    assert(id == 1 && size == 3 && compressed == 1);
    assert(st_xray_probe_first_archive_chunk_read(root, 0, payload, 3, &actual) == -5);
    assert(actual == 0);
    // Truncated payload must be rejected rather than accepted as a chunk.
    { std::ofstream out(archive, std::ios::binary | std::ios::trunc);
      const unsigned char invalid[] = {1,0,0,0,10,0,0,0,'x'};
      out.write(reinterpret_cast<const char*>(invalid), sizeof(invalid)); }
    assert(st_xray_probe_first_archive_chunks(root, &chunks) == -4);
    assert(chunks == 0);
    assert(st_xray_probe_first_archive_chunk_info(root, 0, &id, &size, &compressed) == -4);
    assert(std::remove(archive.c_str()) == 0);
    const std::string target = std::string(root) + "/target.bin";
    { std::ofstream out(target, std::ios::binary); out << "outside"; }
    assert(symlink(target.c_str(), archive.c_str()) == 0);
    assert(st_xray_probe_first_archive_chunks(root, &chunks) == -3);
    assert(std::remove(archive.c_str()) == 0);
    assert(std::remove(target.c_str()) == 0);
    assert(rmdir(root) == 0);
    return 0;
}
