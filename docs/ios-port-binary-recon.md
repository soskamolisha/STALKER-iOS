# Existing iOS Shadow of Chernobyl port: binary reconnaissance

Source: user-provided `StalkerMobile(1)` executable (not redistributed here).

## Verified by llvm-objdump and strings on the supplied file

- Mach-O `MH_EXECUTE`, ARM64, PIE, approximately 65 MiB.
- Embedded paths reference `/home/nxvvsx1234/xray-ios-soc/src/xrCore/` and `src/xrEngine/`.
- `fsgame.ltx`, `openxray.ltx`, `OpenXRay` and `xrEngine/x_ray.cpp` appear in string data.
- Dynamic dependencies include UIKit, Metal, GameController, AVFoundation, CoreAudio, IOSurface and libc++.
- A global `_SDL_main` symbol is visible.
- The required launcher ABI `st_xray_embedded_run_with_resources` was **not found** in the examined symbol listing.

## Integration implications

The supplied binary is a standalone iOS executable, **not** an iOS static library or a set of reconstructable C++ sources. It cannot be force-loaded as a `.a` archive, nor can its `SDL_main` simply be called from a separate signed app. No game assets or proprietary executable are committed.

1. Locate legally available source for the iOS fork (`xray-ios-soc`) or obtain its source from its author.
2. Build its engine as an iOS arm64 static library, exposing `st_xray_embedded_run_with_resources(const char*, const char*)` and using the launcher's resource root.
3. Preserve the port's Metal/SDL lifecycle, renderer, input, audio and platform dependencies.
4. Verify with `scripts/verify_openxray_archive.py`, then integrate with `scripts/build_with_openxray.sh`.
5. Test on device. Linkage alone is not evidence of gameplay.

No public GitHub repository named `xray-ios-soc` was found in the repository search performed during this investigation. That is not proof the source is unavailable elsewhere.
