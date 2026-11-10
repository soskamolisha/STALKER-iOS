// Compile a real, pinned OpenXRay C++ math header on the host and for arm64-apple-ios.
// The upstream header is fetched by the workflow, not vendored into this repository.
// This is a compiler/ABI probe only: it does not contain the X-Ray game engine.
#include <cstdint>
#include <limits>

using u16 = std::uint16_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;

// OpenXRay normally defines VERIFY2 in xrCore; provide only its local contract
// so this one standalone header can be compiled before the full engine is ported.
#define VERIFY2(condition, message) do { if (!(condition)) __builtin_trap(); } while (0)
#include "fast_lc16.hpp"

// A minimal, C-callable symbol can later be linked from an iOS app.
// It is intentionally NOT presented as a working game engine.
extern "C" std::uint32_t st_openxray_math_sample(std::uint32_t seed) {
    fast_lc16 generator(seed);
    std::uint32_t rolling = 0;
    for (int i = 0; i < 256; ++i)
        rolling = rolling * 33U + generator();
    return rolling;
}

#ifdef STALKER_PROBE_TEST_MAIN
int main() {
    if (st_openxray_math_sample(123U) != st_openxray_math_sample(123U)) return 1;
    if (st_openxray_math_sample(123U) == st_openxray_math_sample(124U)) return 2;
    fast_lc16 generator(1U);
    if (generator.min() != 0U || generator.max() != 65535U) return 3;
    return 0;
}
#endif // STALKER_PROBE_TEST_MAIN
