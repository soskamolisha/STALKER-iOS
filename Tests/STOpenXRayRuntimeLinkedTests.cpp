#include "../Sources/Native/STOpenXRayRuntime.h"
#include <cassert>
#include <cstring>

// Simulate the OpenXRay exported entry symbol. The real engine is not
// compiled or executed by this portable test.
extern "C" int st_xray_embedded_run(const char* args) {
    return std::strcmp(args, "-stalker-ios-test") == 0 ? 42 : 7;
}
int main() {
    assert(st_openxray_runtime_linked() == 1);
    assert(st_openxray_runtime_run(nullptr) == -2);
    assert(st_openxray_runtime_run("-stalker-ios-test") == 42);
}
