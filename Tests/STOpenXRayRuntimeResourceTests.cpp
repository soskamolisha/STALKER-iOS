#include "../Sources/Native/STOpenXRayRuntime.h"
#include <cassert>
#include <cstring>
#include <stdexcept>

// Mock only: verifies resource-root forwarding and the shared runtime guard.
// This does not build or run the real OpenXRay engine.
static int nested_result = 0;
static const char* expected_root = "/tmp/imported-shadow";
extern "C" int st_xray_embedded_run_with_resources(const char* args, const char* root) {
    assert(root && std::strcmp(root, expected_root) == 0);
    if (std::strcmp(args, "-nested") == 0) {
        nested_result = st_openxray_runtime_run_with_resources("", root);
        return 19;
    }
    if (std::strcmp(args, "-throw") == 0) throw std::runtime_error("mock exception");
    return 11;
}

int main() {
    assert(st_openxray_runtime_resources_linked() == 1);
    assert(st_openxray_runtime_run_with_resources(nullptr, expected_root) == -2);
    assert(st_openxray_runtime_run_with_resources("", nullptr) == -2);
    assert(st_openxray_runtime_run_with_resources("", "") == -2);
    assert(st_openxray_runtime_run_with_resources("-nested", expected_root) == 19);
    assert(nested_result == -3);
    assert(st_openxray_runtime_run_with_resources("-throw", expected_root) == -4);
    assert(st_openxray_runtime_run_with_resources("", expected_root) == 11);
}
