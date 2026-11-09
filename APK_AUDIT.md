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

## Source provenance follow-up — 2026-11-10

Reinspected the supplied APK without executing its code. SHA-256:

```text
ab5eb7f61a081ea81ee71cf5280b1d4295fc8588dd84320ba76eeb420928e3a7
```

This matches the checksum on the publisher's [1.0.30 download page](https://stalker-mobile.ru/skachat) at inspection time.

- Embedded native source-path strings refer to separate `native/xray-16` and `native/xray-16-cop` trees. They are build-path evidence, not the source files themselves.
- No complete engine C/C++ source tree, engine build scripts, or URL identifying the matching engine fork was found in the inspected archive entries and embedded strings. The APK includes shader and script assets, which do not provide the missing native engine implementation.
- `assets/cop/coc/SOURCE.txt` attributes the Call of Chernobyl content and a compatibility patch. Its links do not identify the source used to build the two native engines.
- Two 40-character hexadecimal strings were found: `37d91e933c6f165283945b270063e101401d1340` in `libxrsoc.so`, and `cced8b0b48c28e11006cdf686be407f92763a006` in `libxrcop.so`. Their purpose is unconfirmed. Neither resolved as a commit in `OpenXRay/xray-16`, and GitHub commit search returned no matches. Do not treat them as verified source revisions.
- A public repository named `stalkermobile/openxray-mobile` exists, but its reported last push was in June 2022. Its connection to the November 2026 APK has not been established; the similar name is insufficient evidence.

**Required next input:** the corresponding source repository/archive, source revision, and build/dependency instructions from the Android port's maintainer. Public searches and APK strings have not established these. This is an unresolved source-provenance result, not proof that the source is unavailable everywhere.
