// Standalone native regression tests for structural chunk parsing.
// Build on macOS/Linux: c++ -std=c++17 -Wall -Wextra -Werror
//   Tests/STChunksRegression.cpp Sources/Native/STChunks.cpp -o /tmp/stchunks-test
// Run: /tmp/stchunks-test
#include "../Sources/Native/STChunks.h"
#include "../Sources/Native/STLZOAdapter.h"
#include <cassert>
#include <cstdint>
#include <vector>
#if defined(ST_HAVE_LZO1X) && ST_HAVE_LZO1X
#if __has_include(<lzo/lzo1x.h>)
#include <lzo/lzo1x.h>
#elif __has_include("minilzo.h")
#include "minilzo.h"
#else
#error "LZO regression requires lzo1x.h or minilzo.h"
#endif
#endif

static void append32(std::vector<uint8_t>& data, uint32_t value) {
    for (unsigned i = 0; i < 4; ++i)
        data.push_back(static_cast<uint8_t>(value >> (8 * i)));
}
static std::vector<uint8_t> chunk(uint32_t id, const std::vector<uint8_t>& payload) {
    std::vector<uint8_t> bytes;
    append32(bytes, id);
    append32(bytes, static_cast<uint32_t>(payload.size()));
    bytes.insert(bytes.end(), payload.begin(), payload.end());
    return bytes;
}
static int32_t copyDecoder(const uint8_t* input, uint32_t inputSize,
                           uint8_t* output, uint32_t capacity,
                           uint32_t* written, void*) {
    if (inputSize > capacity) return -1;
    for (uint32_t i = 0; i < inputSize; ++i) output[i] = input[i];
    *written = inputSize;
    return 0;
}

int main() {
#if defined(ST_HAVE_LZO1X) && ST_HAVE_LZO1X
    assert(st_lzo1x_available() == 1);
#else
    assert(st_lzo1x_available() == 0);
#endif
    uint32_t invalidDecoded = 123;
    assert(st_chunk_decode(nullptr, 0, 0, nullptr, 0,
                           &invalidDecoded, nullptr, nullptr) == -1);
    assert(invalidDecoded == 0);
    uint32_t invalidLzoSize = 123;
    assert(st_lzo1x_decode(nullptr, 1, nullptr, 0, &invalidLzoSize, nullptr) == -1);
    assert(invalidLzoSize == 0);
    // The high bit indicates compression; structural scanning must not
    // mistake it for an ordinary chunk ID or silently decompress payloads.
    const auto compressed = chunk(0x8000002Au, {1, 2, 3});
    uint32_t count = 0, first = 0, size = 0;
    assert(st_chunk_scan(compressed.data(), static_cast<uint32_t>(compressed.size()),
                         &count, &first, &size) == 0);
    assert(count == 1 && first == 42 && size == 3);
    uint32_t id = 0, offset = 0, payloadSize = 0, flag = 0;
    assert(st_chunk_at(compressed.data(), static_cast<uint32_t>(compressed.size()), 0,
                       &id, &offset, &payloadSize, &flag) == 0);
    assert(id == 42 && offset == 8 && payloadSize == 3 && flag == 1);
    uint32_t children = 0, childId = 0;
    assert(st_chunk_scan_children(compressed.data(), static_cast<uint32_t>(compressed.size()),
                                  0, &children, &childId) == -4);
    uint8_t output[3] = {};
    assert(st_chunk_copy_slice(compressed.data(), static_cast<uint32_t>(compressed.size()),
                               0, 0, 3, output, sizeof(output)) == -4);

    // Uncompressed payloads can be copied without a decompressor.
    const auto plain = chunk(7, {10, 20, 30});
    assert(st_chunk_copy_slice(plain.data(), static_cast<uint32_t>(plain.size()),
                               0, 1, 2, output, sizeof(output)) == 0);
    assert(output[0] == 20 && output[1] == 30);
    assert(st_chunk_copy_slice(plain.data(), static_cast<uint32_t>(plain.size()),
                               0, 2, 2, output, sizeof(output)) == -5);

    // A declared payload larger than available bytes must be rejected.
    auto truncated = plain;
    truncated[4] = 100;
    assert(st_chunk_scan(truncated.data(), static_cast<uint32_t>(truncated.size()),
                         &count, &first, &size) == -1);
    assert(st_chunk_at(truncated.data(), static_cast<uint32_t>(truncated.size()), 0,
                       &id, &offset, &payloadSize, &flag) == -1);

    // An uncompressed parent may contain nested chunks.
    const auto child = chunk(19, {0xAB});
    const auto parent = chunk(9, child);
    assert(st_chunk_scan_children(parent.data(), static_cast<uint32_t>(parent.size()),
                                  0, &children, &childId) == 0);
    assert(children == 1 && childId == 19);

    // A malformed nested chunk is rejected without reading beyond its parent.
    const auto malformedParent = chunk(9, {0x01, 0x02, 0x03});
    assert(st_chunk_scan_children(malformedParent.data(),
                                  static_cast<uint32_t>(malformedParent.size()),
                                  0, &children, &childId) == -1);

    // The API must distinguish absent chunks from corrupt data.
    assert(st_chunk_at(plain.data(), static_cast<uint32_t>(plain.size()), 1,
                       &id, &offset, &payloadSize, &flag) == -3);

    // A zero-byte slice is valid even with a null destination.
    assert(st_chunk_copy_slice(plain.data(), static_cast<uint32_t>(plain.size()),
                               0, 3, 0, nullptr, 0) == 0);
    assert(st_chunk_copy_slice(plain.data(), static_cast<uint32_t>(plain.size()),
                               0, 0, 2, output, 1) == -6);
    // Decoder integration: compressed payloads are passed through a bounded
    // callback, never silently interpreted as plain bytes.
    uint32_t decodedSize = 99;
    assert(st_chunk_decode(compressed.data(), static_cast<uint32_t>(compressed.size()),
                           0, output, sizeof(output), &decodedSize, nullptr, nullptr) == -4);
    assert(decodedSize == 0);
    assert(st_chunk_decode(compressed.data(), static_cast<uint32_t>(compressed.size()),
                           0, output, sizeof(output), &decodedSize, copyDecoder, nullptr) == 0);
    assert(decodedSize == 3 && output[0] == 1 && output[2] == 3);
    assert(st_chunk_decode(compressed.data(), static_cast<uint32_t>(compressed.size()),
                           0, output, 1, &decodedSize, copyDecoder, nullptr) == -7);
    assert(decodedSize == 0);
    assert(st_chunk_decode(plain.data(), static_cast<uint32_t>(plain.size()),
                           0, output, sizeof(output), &decodedSize, nullptr, nullptr) == 0);
    assert(decodedSize == 3 && output[0] == 10);
    assert(st_chunk_decode_lzo(plain.data(), static_cast<uint32_t>(plain.size()),
                               0, output, sizeof(output), &decodedSize) == 0);
    assert(decodedSize == 3 && output[2] == 30);
    // Without an explicitly linked LZO implementation, the adapter must
    // report unavailable rather than pretending the input is decoded.
#if !defined(ST_HAVE_LZO1X) || !ST_HAVE_LZO1X
    decodedSize = 99;
    assert(st_lzo1x_decode(compressed.data() + 8, 3, output,
                           sizeof(output), &decodedSize, nullptr) == -4);
    assert(decodedSize == 0);
    assert(st_chunk_decode_lzo(compressed.data(), static_cast<uint32_t>(compressed.size()),
                               0, output, sizeof(output), &decodedSize) == -4);
    assert(decodedSize == 0);
#endif
#if defined(ST_HAVE_LZO1X) && ST_HAVE_LZO1X
    // Real LZO1X round-trip: generate a valid compressed payload, pass it
    // through the chunk boundary, and verify byte-for-byte output.
    assert(lzo_init() == LZO_E_OK);
    const std::vector<uint8_t> original(512, 0x5A);
    std::vector<uint8_t> compressedPayload(original.size() + original.size() / 16 + 64 + 3);
    std::vector<lzo_align_t> work((LZO1X_1_MEM_COMPRESS + sizeof(lzo_align_t) - 1) / sizeof(lzo_align_t));
    lzo_uint compressedLength = static_cast<lzo_uint>(compressedPayload.size());
    assert(lzo1x_1_compress(original.data(), static_cast<lzo_uint>(original.size()),
                           compressedPayload.data(), &compressedLength,
                           work.data()) == LZO_E_OK);
    compressedPayload.resize(static_cast<size_t>(compressedLength));
    const auto lzoChunk = chunk(0x8000002Au, compressedPayload);
    std::vector<uint8_t> restored(original.size());
    decodedSize = 0;
    assert(st_chunk_decode(lzoChunk.data(), static_cast<uint32_t>(lzoChunk.size()),
                           0, restored.data(), static_cast<uint32_t>(restored.size()),
                           &decodedSize, st_lzo1x_decode, nullptr) == 0);
    assert(decodedSize == original.size() && restored == original);
    decodedSize = 0;
    assert(st_chunk_decode_lzo(lzoChunk.data(), static_cast<uint32_t>(lzoChunk.size()),
                               0, restored.data(), static_cast<uint32_t>(restored.size()),
                               &decodedSize) == 0);
    assert(decodedSize == original.size() && restored == original);
    decodedSize = 99;
    assert(st_chunk_decode(lzoChunk.data(), static_cast<uint32_t>(lzoChunk.size()),
                           0, restored.data(), 16, &decodedSize,
                           st_lzo1x_decode, nullptr) == -7);
    assert(decodedSize == 0);
    // A truncated compressed stream must never produce a successful decode.
    auto brokenLzoChunk = lzoChunk;
    brokenLzoChunk.pop_back();
    // Keep the chunk framing valid while corrupting the compressed payload.
    const uint32_t brokenSize = static_cast<uint32_t>(brokenLzoChunk.size() - 8);
    for (unsigned byte = 0; byte < 4; ++byte)
        brokenLzoChunk[4 + byte] = static_cast<uint8_t>(brokenSize >> (8 * byte));
    decodedSize = 99;
    assert(st_chunk_decode(brokenLzoChunk.data(),
                           static_cast<uint32_t>(brokenLzoChunk.size()),
                           0, restored.data(), static_cast<uint32_t>(restored.size()),
                           &decodedSize, st_lzo1x_decode, nullptr) == -7);
    assert(decodedSize == 0);
#endif
    return 0;
}
