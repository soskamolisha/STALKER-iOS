#include "../Sources/Native/STOpenXRayRuntime.h"
#include <cassert>
int main() {
    assert(st_openxray_runtime_linked() == 0);
    assert(st_openxray_runtime_run(nullptr) == -2);
    assert(st_openxray_runtime_run("") == -1);
}
