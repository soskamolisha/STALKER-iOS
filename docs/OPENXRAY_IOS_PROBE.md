# First upstream OpenXRay source probe for iOS

**Scope:** a narrowly scoped test of native OpenXRay code compiled with the iPhone
SDK. This is not an X-Ray engine port, not a playable game and not a signed IPA.

## Candidate source, not established APK provenance

- Candidate: https://github.com/Eternal-Entropia/openxray-android
- Pinned revision: `1b84cfbc5efa3e9af942164e5a3c2dde43b77f51`
- Candidate declares SoC, CS and CoP support in its README. No evidence yet
  proves that this revision matches the libraries in stalker-mobile-1.0.30.apk.
- Upstream licenses and third-party attributions must be reviewed before any
  redistribution. Game data must be legally supplied separately and must not
  be committed to this public repository.

## What the automated proof checks

The workflow `.github/workflows/openxray-ios-probe.yml` downloads the
`src/xrCore/Math/fast_lc16.hpp` header from that **exact commit** and
compiles `Tests/OpenXRayMathProbe.cpp` (which includes that header):

1. Runs deterministic random-number smoke checks on the macOS host.
2. Cross-compiles the same real OpenXRay C++ component with the Apple iPhoneOS
   SDK and an `arm64-apple-ios17.0` target.
3. Packs the object into a static `.a` library and uploads it as a CI artifact.

This tests one reusable C++ engine component and an exported C ABI function.
It does **not** test xrCore as a whole, xrEngine, any renderer, APK loading,
iPhone execution, or gameplay. The provided `VERIFY2` shim is only for the
standalone header; it is not a replacement for OpenXRay's native services.

## Verified result (2026-11-10)

[GitHub Actions probe run #2](https://github.com/soskamolisha/STALKER-iOS/actions/runs/42787599487)
**passed** at commit `4e2007c51de09c9003c30e41bd2939b720a11fdf`:

- The workflow checked out the exact upstream OpenXRay revision.
- The native C++ test ran successfully on the macOS runner.
- The same source cross-compiled for `arm64-apple-ios17.0`, then was
  packed into `libOpenXRayMathProbe-ios.a` and uploaded as the
  `openxray-math-arm64-ios` artifact.

This is a **static library with one math component**, not a game executable,
renderer, signed IPA, or evidence of successful iPhone execution. The full
xrCore library and engine remain unported.

## Known subsequent work

- Port an actual xrCore subset and resolve its SDL2, LZO, filesystem and
  other target dependencies for iOS; test on a real device.
- Replace or port the rendering backend. The Android OpenGL ES implementation
  cannot simply be assumed to run on current iOS.
- Bridge lifecycle/input/audio and verify compatibility with legally owned
  Shadow of Chernobyl assets.
- Add signed IPA export and device installation only after actual game features
  exist and can be tested.

Earlier native SwiftUI/Metal triangle tests are independent of this probe.
