# X-Ray to iOS: implementation checklist

**Status:** No native X-Ray port is present in this repository. The existing SwiftUI screen is a prototype, not gameplay.

## Source and licensing gate
- Identify the exact engine source revision matching the Android APK.
- Verify source licenses and third-party dependencies before redistribution.
- Do not commit proprietary original game data or extracted copyrighted assets.

## Native engine work
1. Build C/C++ engine libraries for arm64-apple-ios with Apple clang.
2. Replace Android JNI/Activity, input, asset manager, filesystem and lifecycle calls.
3. Audit graphics backend. Android OpenGL ES/Vulkan code cannot be assumed to work on iOS; implement a compatible Metal rendering path or other supported backend.
4. Port audio and touch/gamepad input.
5. Provide a stable C ABI or Objective-C++ adapter callable from Swift.
6. Validate loading and saving, shader compilation, performance and device memory.
7. Test with legally obtained Shadow of Chernobyl data and verify that the selected X-Ray fork actually supports that game.

## Signing and installation
- The GitHub workflow currently builds unsigned application bundles for compilation checks only.
- An unsigned .app ZIP is not an installable IPA.
- For an installable IPA, configure Apple code signing and provisioning for the device and export a signed iOS archive.
- Never upload Apple private signing keys or game assets to a public repository.

## Milestones
- [x] SwiftUI placeholder and touch-control mockup
- [x] XcodeGen configuration
- [x] CI build recipes for simulator and device
- [x] Add minimal Metal clear-pass surface (not X-Ray rendering)
- [x] Verify CI compilation succeeded (run #12; simulator and unsigned device builds)
- [ ] Port native X-Ray engine and render backend
- [ ] Integrate engine through bridge
- [ ] Load authorized game data and test on device
- [ ] Sign and export installable IPA

## Build verification and next step

Verified [run #12](https://github.com/soskamolisha/STALKER-iOS/actions/runs/42667557712) against commit `d32844ac50cd3eb7cf7939f5d0d8ce7cbf0f26ac`: both compilation steps and artifact upload succeeded. This is a compile check, not a device execution or gameplay test.

The exact source revision for `libxrsoc.so` and `libxrcop.so` remains unresolved. Before integrating an engine, obtain the source repository/archive and revision corresponding to APK 1.0.30, including the Android changes and dependency/build information. The project's published contact is [@Stalker_port](https://t.me/Stalker_port), linked from its [about page](https://stalker-mobile.ru/o-proekte). No message has been sent.

The publisher's [iPhone page](https://stalker-mobile.ru/stalker-na-iphone) says that it does not provide an iOS version. A separate port and real device validation remain necessary.
