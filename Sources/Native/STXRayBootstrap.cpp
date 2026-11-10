#include "STXRayBootstrap.h"
#include "XRayBootstrap.hpp"
#include <new>

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
void st_xray_shutdown(STXRayHandle handle) {
    if (handle) static_cast<XRayBootstrap*>(handle)->shutdown();
}
}
