#include "XRayLevelInspection.hpp"
namespace stalker {
LevelInspection inspectLevel(const std::string& root, const std::string& name) {
    LevelInspection result;
    result.files = inspectLooseLevel(root, name);
    if (!result.files.hasLevel || !result.files.hasGeometry) return result;
    const std::string path = root + "/gamedata/levels/" + name + "/level";
    result.geometryIndexed = indexXRayChunks(path, result.geometryChunks);
    return result;
}
}
