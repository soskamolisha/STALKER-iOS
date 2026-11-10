#include "STArchive.h"
#include "ArchiveChunks.hpp"
#include <fstream>

extern "C" int32_t st_archive_inspect(const char *path, STArchiveSummary *out_summary) {
    if (!path || !out_summary) return 1;
    *out_summary = {};
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) return 2;
    try {
        const auto chunks = stalker::scanArchive(file);
        for (const auto& chunk : chunks) {
            ++out_summary->chunk_count;
            if (chunk.compressed) ++out_summary->compressed_count;
            out_summary->payload_bytes += chunk.size;
        }
    } catch (const std::exception&) {
        *out_summary = {};
        return 3;
    }
    return 0;
}
