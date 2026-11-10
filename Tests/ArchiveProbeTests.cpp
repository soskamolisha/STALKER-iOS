#include "../Sources/Native/STArchive.h"
#include <cassert>
#include <cstdio>
#include <fstream>
#include <string>
#include <unistd.h>

static void append32(std::string& s, uint32_t value) {
    for (int i = 0; i < 4; ++i) s.push_back(static_cast<char>(value >> (i * 8)));
}
static void chunk(std::string& s, uint32_t id, const std::string& body) {
    append32(s, id);
    append32(s, static_cast<uint32_t>(body.size()));
    s += body;
}
static void writeFile(const std::string& path, const std::string& content) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    assert(out.good());
    out.write(content.data(), static_cast<std::streamsize>(content.size()));
    assert(out.good());
}
int main() {
    assert(st_archive_inspect(nullptr, nullptr) == 1);
    STArchiveSummary summary{};
    assert(st_archive_inspect("/nonexistent/stalker-test.db", &summary) == 2);
    const auto path = "/tmp/stalker-archive-probe-" + std::to_string(getpid()) + ".db";
    std::string content;
    chunk(content, 7, "hello");
    chunk(content, 0x80000002U, "compressed");
    writeFile(path, content);
    assert(st_archive_inspect(path.c_str(), &summary) == 0);
    assert(summary.chunk_count == 2);
    assert(summary.compressed_count == 1);
    assert(summary.payload_bytes == 15);
    writeFile(path, content.substr(0, content.size()-1));
    assert(st_archive_inspect(path.c_str(), &summary) == 3);
    assert(summary.chunk_count == 0);
    writeFile(path, "");
    assert(st_archive_inspect(path.c_str(), &summary) == 3);
    assert(std::remove(path.c_str()) == 0);
}
