#include "../Sources/Native/ArchiveChunks.hpp"
#include <cassert>
#include <sstream>
#include <string>

static void append32(std::string& s, uint32_t n) {
    for (int i = 0; i < 4; ++i) s.push_back(static_cast<char>(n >> (i * 8)));
}
static void chunk(std::string& s, uint32_t id, const std::string& bytes) {
    append32(s, id); append32(s, static_cast<uint32_t>(bytes.size())); s += bytes;
}
static bool rejected(const std::string& bytes) {
    std::istringstream stream(bytes);
    try { stalker::scanArchive(stream); return false; }
    catch (const std::runtime_error&) { return true; }
}
int main() {
    std::string data;
    chunk(data, 0, "data"); chunk(data, 0x80000001U, "compressed");
    chunk(data, 666, "header");
    std::istringstream stream(data);
    const auto chunks = stalker::scanArchive(stream);
    assert(chunks.size() == 3);
    assert(chunks[0].offset == 8 && chunks[0].size == 4 && !chunks[0].compressed);
    assert(chunks[1].id == 1 && chunks[1].offset == 20 && chunks[1].compressed);
    assert(chunks[2].id == 666);
    assert(rejected(""));
    for (size_t n = 1; n < 12; ++n) assert(rejected(data.substr(0, n)));
    assert(rejected(data + "x"));
    std::string oversized; append32(oversized, 1); append32(oversized, 0xffffffffU);
    assert(rejected(oversized));
    std::string zero; chunk(zero, 1, "");
    std::istringstream zeroStream(zero);
    assert(stalker::scanArchive(zeroStream).front().size == 0);
    std::string many;
    for (int i = 0; i < 4097; ++i) chunk(many, 0, "");
    assert(rejected(many));
}
