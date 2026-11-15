#include <cassert>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <vector>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
extern "C" int32_t st_xray_probe_shadow_resources(const char*, uint32_t*, uint64_t*, uint32_t*);
extern "C" int32_t st_xray_probe_first_archive_chunks(const char*, uint32_t*);
extern "C" int32_t st_xray_probe_first_archive_chunk_info(const char*, uint32_t, uint32_t*, uint32_t*, int32_t*);
extern "C" int32_t st_xray_probe_first_archive_chunk_read(const char*, uint32_t, void*, uint32_t, uint32_t*);
extern "C" int32_t st_xray_probe_first_archive_chunk_crc32(const char*, uint32_t, uint32_t*);
extern "C" uint32_t st_openxray_crc32(const void* data, uint32_t size) {
    const auto* bytes = static_cast<const unsigned char*>(data);
    uint32_t sum = 0;
    for (uint32_t i = 0; i < size; ++i) sum += bytes[i];
    return sum; // Test double only: iOS links the real upstream CRC32.
}
extern "C" int32_t st_xray_probe_first_archive_directory(const char*, uint32_t*, uint32_t*, int32_t*);
extern "C" int32_t st_xray_probe_first_archive_directory_read(const char*, void*, uint32_t, uint32_t*);
extern "C" int32_t st_xray_probe_first_archive_directory_crc32(const char*, uint32_t*);
extern "C" int32_t st_xray_probe_first_archive_directory_entries(const char*, uint32_t*);
extern "C" int32_t st_xray_probe_first_archive_directory_name(const char*, uint32_t, char*, uint32_t);
extern "C" int32_t st_xray_probe_first_archive_name(const char*, char*, uint32_t);
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
    char selectedArchive[64] = {};
    assert(st_xray_probe_first_archive_name(root, selectedArchive, sizeof(selectedArchive)) == 0);
    assert(std::string(selectedArchive) == "gamedata.db0");
    assert(st_xray_probe_first_archive_name(root, selectedArchive, 2) == -6);
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
    uint32_t crc = 0;
    assert(st_xray_probe_first_archive_chunk_crc32(root, 0, &crc) == 0);
    assert(crc == 294);
    assert(st_xray_probe_first_archive_chunk_crc32(root, 0, nullptr) == -1);
    assert(st_xray_probe_first_archive_chunk_crc32(root, 1, &crc) == -2);
    assert(crc == 0);
    assert(st_xray_probe_first_archive_chunk_read(root, 1, payload, 3, &actual) == -2);
    assert(st_xray_probe_first_archive_chunk_info(root, 1, &id, &size, &compressed) == -2);
    // Compressed X-Ray chunks must never be exposed as decoded raw bytes.
    { std::ofstream out(archive, std::ios::binary | std::ios::trunc);
      const unsigned char compressedChunk[] = {
          1,0,0,128, 3,0,0,0, 'a','b','c'
      };
      out.write(reinterpret_cast<const char*>(compressedChunk), sizeof(compressedChunk)); }
    assert(st_xray_probe_first_archive_chunk_info(root, 0, &id, &size, &compressed) == 0);
    assert(id == 1 && size == 3 && compressed == 1);
    actual = 99;
    assert(st_xray_probe_first_archive_chunk_read(root, 0, payload, sizeof(payload), &actual) == -5);
    assert(actual == 0);
    // A single well-formed directory entry with a safe relative filename.
    { std::ofstream out(archive, std::ios::binary | std::ios::trunc);
      const unsigned char directoryRecord[] = {
          1,0,0,0, 22,0,0,0,
          20,0, 0,0,0,0,0,0,0,0,0,0,0,0,
          't','e','s','t', 0,0,0,0
      };
      out.write(reinterpret_cast<const char*>(directoryRecord), sizeof(directoryRecord)); }
    uint32_t directoryEntries = 99;
    assert(st_xray_probe_first_archive_directory_entries(root, &directoryEntries) == 0);
    assert(directoryEntries == 1);
    char directoryName[16] = {};
    assert(st_xray_probe_first_archive_directory_name(root, 0, directoryName, sizeof(directoryName)) == 0);
    assert(std::string(directoryName) == "test");
    assert(st_xray_probe_first_archive_directory_name(root, 1, directoryName, sizeof(directoryName)) == -2);
    assert(st_xray_probe_first_archive_directory_name(root, 0, directoryName, 2) == -6);
    // Two valid directory entries must be enumerated in stable archive order.
    { std::ofstream out(archive, std::ios::binary | std::ios::trunc);
      const unsigned char twoEntries[] = {
          1,0,0,0, 48,0,0,0,
          20,0, 0,0,0,0,0,0,0,0,0,0,0,0,
          'f','i','r','s', 0,0,0,0,
          24,0, 0,0,0,0,0,0,0,0,0,0,0,0,
          's','e','c','o','n','d','/','x', 0,0,0,0
      };
      out.write(reinterpret_cast<const char*>(twoEntries), sizeof(twoEntries)); }
    assert(st_xray_probe_first_archive_directory_entries(root, &directoryEntries) == 0);
    assert(directoryEntries == 2);
    assert(st_xray_probe_first_archive_directory_name(root, 0, directoryName, sizeof(directoryName)) == 0);
    assert(std::string(directoryName) == "firs");
    assert(st_xray_probe_first_archive_directory_name(root, 1, directoryName, sizeof(directoryName)) == 0);
    assert(std::string(directoryName) == "second/x");
    assert(st_xray_probe_first_archive_directory_name(root, 2, directoryName, sizeof(directoryName)) == -2);
    // Malformed short directory records must not be accepted as file names.
    { std::ofstream out(archive, std::ios::binary | std::ios::trunc);
      const unsigned char malformedDirectory[] = {1,0,0,0,3,0,0,0,'a','b','c'};
      out.write(reinterpret_cast<const char*>(malformedDirectory), sizeof(malformedDirectory)); }
    assert(st_xray_probe_first_archive_directory_entries(root, &directoryEntries) == -7);
    // A valid first entry must not hide malformed trailing directory data.
    { std::ofstream out(archive, std::ios::binary | std::ios::trunc);
      const unsigned char mixedDirectory[] = {
          1,0,0,0, 33,0,0,0,
          20,0, 0,0,0,0,0,0,0,0,0,0,0,0,
          't','e','s','t', 0,0,0,0,
          2,0,0,0, 3,0,0,0, 'x','y','z'
      };
      out.write(reinterpret_cast<const char*>(mixedDirectory), sizeof(mixedDirectory)); }
    assert(st_xray_probe_first_archive_directory_entries(root, &directoryEntries) == -7);
    assert(directoryEntries == 0);
    assert(st_xray_probe_first_archive_directory_name(root, 0, directoryName, sizeof(directoryName)) == -7);

    // Reject parent-directory traversal even when the record is structurally valid.
    { std::ofstream out(archive, std::ios::binary | std::ios::trunc);
      const unsigned char traversal[] = {
          1,0,0,0, 26,0,0,0,
          24,0, 0,0,0,0,0,0,0,0,0,0,0,0,
          '.','.','/','e','v','i','l','x', 0,0,0,0
      };
      out.write(reinterpret_cast<const char*>(traversal), sizeof(traversal)); }
    assert(st_xray_probe_first_archive_directory_entries(root, &directoryEntries) == -7);
    assert(directoryEntries == 0);
    assert(st_xray_probe_first_archive_directory_name(root, 0, directoryName, sizeof(directoryName)) == -7);

    // Reject Windows drive-qualified archive paths.
    { std::ofstream out(archive, std::ios::binary | std::ios::trunc);
      const unsigned char drivePath[] = {
          1,0,0,0, 26,0,0,0,
          24,0, 0,0,0,0,0,0,0,0,0,0,0,0,
          'C',':','/','e','v','i','l','x', 0,0,0,0
      };
      out.write(reinterpret_cast<const char*>(drivePath), sizeof(drivePath)); }
    assert(st_xray_probe_first_archive_directory_entries(root, &directoryEntries) == -7);
    assert(directoryEntries == 0);

    // A path with an empty segment must be rejected.
    { std::ofstream out(archive, std::ios::binary | std::ios::trunc);
      const unsigned char emptySegment[] = {
          1,0,0,0, 26,0,0,0,
          24,0, 0,0,0,0,0,0,0,0,0,0,0,0,
          'd','i','r','/','/','f','i','l', 0,0,0,0
      };
      out.write(reinterpret_cast<const char*>(emptySegment), sizeof(emptySegment)); }
    assert(st_xray_probe_first_archive_directory_entries(root, &directoryEntries) == -7);
    assert(directoryEntries == 0);
    // Two adjacent chunks must retain their own offsets and payload boundaries.
    { std::ofstream out(archive, std::ios::binary | std::ios::trunc);
      const unsigned char twoChunks[] = {
          1,0,0,0,3,0,0,0,'a','b','c',
          2,0,0,0,4,0,0,0,'d','e','f','g'
      };
      out.write(reinterpret_cast<const char*>(twoChunks), sizeof(twoChunks)); }
    assert(st_xray_probe_first_archive_chunks(root, &chunks) == 0);
    assert(chunks == 2);
    uint32_t directoryIndex = 99, directorySize = 99;
    int32_t directoryCompressed = -1;
    assert(st_xray_probe_first_archive_directory(root, &directoryIndex, &directorySize, &directoryCompressed) == 0);
    assert(directoryIndex == 0 && directorySize == 3 && directoryCompressed == 0);
    char directoryPayload[4] = {};
    uint32_t directoryActual = 99;
    assert(st_xray_probe_first_archive_directory_read(root, directoryPayload, 2, &directoryActual) == -6);
    assert(directoryActual == 0);
    assert(st_xray_probe_first_archive_directory_read(root, directoryPayload, 4, &directoryActual) == 0);
    assert(directoryActual == 3 && std::string(directoryPayload, 3) == "abc");
    assert(st_xray_probe_first_archive_directory_crc32(root, &crc) == 0 && crc == 294);
    assert(st_xray_probe_first_archive_chunk_info(root, 1, &id, &size, &compressed) == 0);
    assert(id == 2 && size == 4 && compressed == 0);
    assert(st_xray_probe_first_archive_chunk_read(root, 1, payload, 4, &actual) == 0);
    assert(actual == 4 && std::string(payload, 4) == "defg");
    assert(st_xray_probe_first_archive_chunk_crc32(root, 1, &crc) == 0);
    assert(crc == 406);
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
    assert(st_xray_probe_first_archive_chunk_crc32(root, 0, &crc) == 0);
    assert(crc == 0);
    // Reject a partial trailing chunk header.
    { std::ofstream out(archive, std::ios::binary | std::ios::trunc);
      const unsigned char trailing[] = {3,0,0,0,0,0,0,0,1,2,3};
      out.write(reinterpret_cast<const char*>(trailing), sizeof(trailing)); }
    assert(st_xray_probe_first_archive_chunks(root, &chunks) == -4);
    assert(chunks == 0);
    // Enforce the 64 KiB payload ceiling even when the caller has a larger buffer.
    { std::ofstream out(archive, std::ios::binary | std::ios::trunc);
      const unsigned char header[] = {4,0,0,0,1,0,1,0};
      out.write(reinterpret_cast<const char*>(header), sizeof(header));
      const std::string oversized(65537, 'x');
      out.write(oversized.data(), oversized.size()); }
    assert(st_xray_probe_first_archive_chunks(root, &chunks) == 0);
    assert(chunks == 1);
    std::vector<char> largePayload(65537);
    assert(st_xray_probe_first_archive_chunk_read(root, 0, largePayload.data(), 65537, &actual) == -6);
    assert(actual == 0);
    assert(st_xray_probe_first_archive_chunk_crc32(root, 0, &crc) == -6);
    assert(crc == 0);
    // Directory discovery must search past other chunks, not assume index zero.
    { std::ofstream out(archive, std::ios::binary | std::ios::trunc);
      const unsigned char delayedDirectory[] = {
          7,0,0,0,2,0,0,0,'x','y',
          1,0,0,128,3,0,0,0,'a','b','c'
      };
      out.write(reinterpret_cast<const char*>(delayedDirectory), sizeof(delayedDirectory)); }
    assert(st_xray_probe_first_archive_directory(root, &directoryIndex, &directorySize, &directoryCompressed) == 0);
    assert(directoryIndex == 1 && directorySize == 3 && directoryCompressed == 1);
    assert(st_xray_probe_first_archive_directory_read(root, directoryPayload, 4, &directoryActual) == -5);
    assert(directoryActual == 0);
    assert(st_xray_probe_first_archive_directory_crc32(root, &crc) == -5 && crc == 0);
    // Missing directory is distinct from an unreadable or malformed archive.
    { std::ofstream out(archive, std::ios::binary | std::ios::trunc);
      const unsigned char noDirectory[] = {7,0,0,0,1,0,0,0,'x'};
      out.write(reinterpret_cast<const char*>(noDirectory), sizeof(noDirectory)); }
    assert(st_xray_probe_first_archive_directory(root, &directoryIndex, &directorySize, &directoryCompressed) == -2);
    assert(directoryIndex == 0 && directorySize == 0 && directoryCompressed == 0);
    assert(st_xray_probe_first_archive_directory_read(root, directoryPayload, 4, &directoryActual) == -2);
    assert(directoryActual == 0);
    assert(st_xray_probe_first_archive_directory_crc32(root, &crc) == -2 && crc == 0);
    // A compressed chunk must be indexed but not returned as raw decoded data.
    { std::ofstream out(archive, std::ios::binary | std::ios::trunc);
      const unsigned char compressedHeader[] = {1,0,0,128,3,0,0,0,'a','b','c'};
      out.write(reinterpret_cast<const char*>(compressedHeader), sizeof(compressedHeader)); }
    assert(st_xray_probe_first_archive_chunks(root, &chunks) == 0);
    assert(chunks == 1);
    assert(st_xray_probe_first_archive_chunk_info(root, 0, &id, &size, &compressed) == 0);
    assert(id == 1 && size == 3 && compressed == 1);
    assert(st_xray_probe_first_archive_directory(root, &directoryIndex, &directorySize, &directoryCompressed) == 0);
    assert(directoryIndex == 0 && directorySize == 3 && directoryCompressed == 1);
    assert(st_xray_probe_first_archive_chunk_read(root, 0, payload, 3, &actual) == -5);
    assert(actual == 0);
    assert(st_xray_probe_first_archive_chunk_crc32(root, 0, &crc) == -5);
    assert(crc == 0);
    // Truncated payload must be rejected rather than accepted as a chunk.
    { std::ofstream out(archive, std::ios::binary | std::ios::trunc);
      const unsigned char invalid[] = {1,0,0,0,10,0,0,0,'x'};
      out.write(reinterpret_cast<const char*>(invalid), sizeof(invalid)); }
    assert(st_xray_probe_first_archive_chunks(root, &chunks) == -4);
    assert(chunks == 0);
    assert(st_xray_probe_first_archive_chunk_info(root, 0, &id, &size, &compressed) == -4);
    assert(st_xray_probe_first_archive_chunk_crc32(root, 0, &crc) == -4);
    assert(crc == 0);
    assert(std::remove(archive.c_str()) == 0);
    const std::string target = std::string(root) + "/target.bin";
    { std::ofstream out(target, std::ios::binary); out << "outside"; }
    assert(symlink(target.c_str(), archive.c_str()) == 0);
    assert(st_xray_probe_first_archive_chunks(root, &chunks) == -3);
    assert(st_xray_probe_first_archive_chunk_crc32(root, 0, &crc) == -3);
    assert(crc == 0);
    assert(std::remove(archive.c_str()) == 0);
    assert(std::remove(target.c_str()) == 0);
    assert(rmdir(root) == 0);
    return 0;
}
