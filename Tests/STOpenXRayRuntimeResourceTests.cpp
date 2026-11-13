#include "../Sources/Native/STOpenXRayRuntime.h"
#include <cassert>
#include <cstring>
#include <stdexcept>
#include <unistd.h>
#include <cstdlib>

// Mock only: verifies resource-root forwarding and the shared runtime guard.
// This does not build or run the real OpenXRay engine.
static int nested_result = 0;
static int cross_entry_result = 0;
extern "C" int st_xray_embedded_run(const char*) {
    cross_entry_result = st_openxray_runtime_run_with_resources("", "/tmp");
    return 23;
}
static const char* expected_root = "/tmp";
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
    assert(st_openxray_runtime_run_with_resources("", "/this-directory-must-not-exist-stalker") == -6);
    char temporary[] = "/tmp/stalker-resource-link-XXXXXX";
    const int fd = mkstemp(temporary);
    assert(fd >= 0);
    assert(close(fd) == 0);
    assert(unlink(temporary) == 0);
    assert(symlink("/tmp", temporary) == 0);
    assert(st_openxray_runtime_run_with_resources("", temporary) == -6);
    assert(unlink(temporary) == 0);
    assert(st_openxray_runtime_run("-legacy") == 23);
    assert(cross_entry_result == -3);
    assert(st_openxray_runtime_run_with_resources("-nested", expected_root) == 19);
    assert(nested_result == -3);
    assert(st_openxray_runtime_run_with_resources("-throw", expected_root) == -4);
    assert(st_openxray_runtime_run_with_resources("", expected_root) == 11);
}
