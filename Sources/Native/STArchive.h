#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

// Small C API for the existing X-Ray chunk scanner. No extraction or decompression.
typedef struct STArchiveSummary {
    uint32_t chunk_count;
    uint32_t compressed_count;
    uint64_t payload_bytes;
} STArchiveSummary;

// 0: scanned, 1: invalid arguments, 2: cannot open, 3: malformed framing.
// A successful scan does not prove this file is a valid game archive.
int32_t st_archive_inspect(const char *path, STArchiveSummary *out_summary);

#ifdef __cplusplus
}
#endif
