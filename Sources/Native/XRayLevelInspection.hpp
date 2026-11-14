#pragma once
#include "XRayChunkIndex.hpp"
#include "XRayLevelFiles.hpp"
#include <string>
#include <vector>

namespace stalker {
struct LevelInspection {
    LevelFiles files;
    bool geometryIndexed = false;
    std::vector<ChunkEntry> geometryChunks;
};
// Reads only unpacked level files, not packed gamedata.db archives.
LevelInspection inspectLevel(const std::string& root, const std::string& name);
}
