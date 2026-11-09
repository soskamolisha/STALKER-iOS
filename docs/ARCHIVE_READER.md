# Archive reader milestone

`Sources/Native/ArchiveChunks.hpp` is an original C++17 structural scanner for seekable X-Ray chunk streams. It reports IDs, payload offsets, lengths and compression flags, rejects truncated or out-of-bounds chunks, and limits chunk count. It reads only eight-byte headers and skips payloads, so archive size does not determine memory allocation.

Protocol references inspected at OpenXRay revision `ddb78a5cfbe6719f4b5abe371e3fdeed0f3481fc`:
- [LocatorAPI.cpp](https://github.com/OpenXRay/xray-16/blob/ddb78a5cfbe6719f4b5abe371e3fdeed0f3481fc/src/xrCore/LocatorAPI.cpp): outer chunk traversal and file-table chunk ID 1.
- [FS.h](https://github.com/OpenXRay/xray-16/blob/ddb78a5cfbe6719f4b5abe371e3fdeed0f3481fc/src/xrCore/FS.h): high-bit compression marker and optional header ID 666.

This is not a complete .db extractor or a signature detector. Arbitrary bytes can satisfy the framing. No encryption, LZ decompression, directory parsing, CRC verification, file extraction, or level loading is implemented. No copyrighted game data is included. No real game archives have been tested.

Synthetic tests exercise compressed flags, multiple chunks, zero-length payloads, truncated headers and payloads, oversized lengths, trailing bytes and the chunk-count bound. The scanner is not yet exposed in Swift UI. Next: decoding and bounding the directory payload, then checking it against a user-supplied original game archive.
