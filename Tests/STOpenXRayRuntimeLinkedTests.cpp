#include "../Sources/Native/STOpenXRayRuntime.h"
#include <cassert>
#include <cstring>
#include <stdexcept>

// Simulate the OpenXRay exported entry symbol. The real engine is not
// compiled or executed by this portable test.
static int nested_status = 0;
extern "C" int st_xray_embedded_run(const char* args) {
    // The upstream engine has process-global state. Reentrant launches must
    // fail before invoking the entry point a second time.
    if (std::strcmp(args, "-stalker-ios-test") == 0) {
        nested_status = st_openxray_runtime_run("-nested-launch");
        return 42;
    }
    if (std::strcmp(args, "-throw-test") == 0) throw std::runtime_error("mock failure");
    return 7;
}

int main() {
    assert(st_openxray_runtime_linked() == 1);
    assert(st_openxray_runtime_run(nullptr) == -2);
    assert(st_openxray_runtime_run("-stalker-ios-test") == 42);
    assert(nested_status == -3);
    // Verify the guard is released after the first engine call returns.
    assert(st_openxray_runtime_run("-throw-test") == -4);
    // The guard must also be released when an embedded entry throws.
    assert(st_openxray_runtime_run("-second-launch") == 7);
}
