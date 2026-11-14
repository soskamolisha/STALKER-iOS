#include <cassert>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
extern "C" int32_t st_xray_probe_shadow_resources(const char*, uint32_t*, uint64_t*, uint32_t*);
extern "C" int32_t st_xray_probe_first_archive_chunks(const char*, uint32_t*);
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
    // Truncated payload must be rejected rather than accepted as a chunk.
    { std::ofstream out(archive, std::ios::binary | std::ios::trunc);
      const unsigned char invalid[] = {1,0,0,0,10,0,0,0,'x'};
      out.write(reinterpret_cast<const char*>(invalid), sizeof(invalid)); }
    assert(st_xray_probe_first_archive_chunks(root, &chunks) == -4);
    assert(chunks == 0);
    assert(std::remove(archive.c_str()) == 0);
    assert(rmdir(root) == 0);
    return 0;
}
