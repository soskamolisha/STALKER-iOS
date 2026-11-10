#include "STXRayBootstrap.h"
#include "XRayBootstrap.hpp"
#include "XRayLevelInspection.hpp"
#include <new>
#include <cstring>

using stalker::XRayBootstrap;
extern "C" {
STXRayHandle st_xray_create(void) {
    return new (std::nothrow) XRayBootstrap();
}
void st_xray_destroy(STXRayHandle handle) {
    delete static_cast<XRayBootstrap*>(handle);
}
int32_t st_xray_initialize(STXRayHandle handle, int32_t edition) {
    if (!handle || edition < 0 || edition > 2) return -1;
    return static_cast<XRayBootstrap*>(handle)->initialize(
        static_cast<stalker::GameEdition>(edition)) ? 0 : -2;
}
int32_t st_xray_attach_resources(STXRayHandle handle, const char* utf8_path) {
    if (!handle || !utf8_path) return -1;
    return static_cast<XRayBootstrap*>(handle)->attachResources(utf8_path) ? 0 : -2;
}
int32_t st_xray_state(STXRayHandle handle) {
    if (!handle) return -1;
    return static_cast<int32_t>(static_cast<XRayBootstrap*>(handle)->state());
}
int32_t st_xray_has_loose_data(STXRayHandle handle) {
    return handle && static_cast<XRayBootstrap*>(handle)->hasLooseGameData() ? 1 : 0;
}
int32_t st_xray_has_packed_data(STXRayHandle handle) {
    return handle && static_cast<XRayBootstrap*>(handle)->hasPackedGameData() ? 1 : 0;
}
int32_t st_xray_level_count(STXRayHandle handle) {
    if (!handle) return -1;
    return static_cast<int32_t>(static_cast<XRayBootstrap*>(handle)->looseLevels().size());
}
int32_t st_xray_level_name(STXRayHandle handle, uint32_t index, char* destination, uint32_t capacity) {
    if (!handle || !destination || capacity == 0) return -1;
    const auto& levels = static_cast<XRayBootstrap*>(handle)->looseLevels();
    if (index >= levels.size()) return -2;
    const std::string& name = levels[index];
    if (name.size() + 1 > capacity) return -3;
    std::memcpy(destination, name.c_str(), name.size() + 1);
    return 0;
}
int32_t st_xray_level_chunk_count(STXRayHandle handle, uint32_t level_index) {
    if (!handle) return -1;
    const auto* engine = static_cast<XRayBootstrap*>(handle);
    const auto& levels = engine->looseLevels();
    if (level_index >= levels.size()) return -1;
    const auto result = stalker::inspectLevel(engine->resourceRoot(), levels[level_index]);
    if (!result.geometryIndexed) return -2;
    return static_cast<int32_t>(result.geometryChunks.size());
}
int32_t st_xray_level_chunk_info(STXRayHandle handle, uint32_t level_index,
                                 uint32_t chunk_index, uint32_t* chunk_id,
                                 uint64_t* payload_offset, uint32_t* payload_size,
                                 int32_t* compressed) {
    if (!handle || !chunk_id || !payload_offset || !payload_size || !compressed) return -1;
    const auto* engine = static_cast<XRayBootstrap*>(handle);
    const auto& levels = engine->looseLevels();
    if (level_index >= levels.size()) return -1;
    const auto result = stalker::inspectLevel(engine->resourceRoot(), levels[level_index]);
    if (!result.geometryIndexed) return -2;
    if (chunk_index >= result.geometryChunks.size()) return -1;
    const auto& chunk = result.geometryChunks[chunk_index];
    *chunk_id = chunk.id;
    *payload_offset = chunk.offset;
    *payload_size = chunk.size;
    *compressed = chunk.compressed ? 1 : 0;
    return 0;
}
void st_xray_shutdown(STXRayHandle handle) {
    if (handle) static_cast<XRayBootstrap*>(handle)->shutdown();
}
}
