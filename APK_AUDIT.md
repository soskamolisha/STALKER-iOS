# Android APK audit — stalker-mobile-1.0.30.apk

Inspected the user-provided APK ZIP structure (no APK binaries copied into this repository).

- ZIP entries: **4,004**
- Android native ABI: **arm64-v8a only**
- Native libraries:
  - `libskinview.so`
  - `libopenal.so`
  - `libxrcop.so`
  - `libSDL2.so`
  - `libxrsoc.so`
  - `libc++_shared.so`
- Android bytecode: `classes.dex`
- Embedded resources include `assets/anomaly/...` overlays, configuration and scripts.

## Porting implications
The Android ELF `.so` files cannot be linked into an iOS application even though both devices use ARM64. iOS requires Mach-O binaries built for Apple's platform SDK. Android `classes.dex` cannot execute natively on iOS.

The `libxrsoc.so` and `libxrcop.so` names suggest distinct game-specific engine components, but their actual source revisions and iOS compatibility are **not established** by filenames alone.

Next engineering milestone: identify corresponding *licensed source code* for each native component, compile a minimal X-Ray engine target for iOS, and verify its renderer. No native X-Ray engine has been built yet.

Do not publish proprietary game resources, APK libraries, or signing credentials to this public repository.
