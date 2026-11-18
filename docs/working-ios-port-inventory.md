# Verified working iOS port: integration inventory

Source: user-supplied files inspected directly on 2026-11-17.
This document intentionally contains **no proprietary executable or game archives**.

## Binary
- File: `StalkerMobile`
- Size: **67,995,004 bytes**
- SHA-256: `71b14246edcb8f2b891c2dc055deda63cba94a93c26bb58ec6466d5a6945503c`
- Mach-O magic: `cffaedfe` (little-endian 64-bit)
- CPU: `0x0100000c` (ARM64)
- Mach-O filetype: `2` (`MH_EXECUTE`), **not** a linkable object or static archive
- Found embedded strings: `OpenXRay`, `xrEngine`, `SDL_main`, `ios_args.txt`, `fsgame.ltx`
- Literal `st_xray_embedded_run_with_resources` string: **not found** (absence of this string alone is not a complete symbol-table analysis)

## Info.plist
- `CFBundleExecutable = StalkerMobile`
- `CFBundleIdentifier = app.stalker.shadowofchernobyl`
- `MinimumOSVersion = 15.0`
- `UIRequiredDeviceCapabilities = [arm64, metal]`
- Landscape orientation, full screen; no custom URL scheme declared.

## Launch arguments
`ios_args.txt` contains exactly:
```
-soc -nointro -nosplash
```

## Filesystem configuration
`fsgame.ltx` sets:
- `$app_data_root$` -> `$fs_root$/appdata/`
- `$game_data$` -> `$fs_root$/gamedata/`
- `$game_levels$` -> `$game_data$/levels/`
- `$game_config$` -> `$game_data$/config/`
- `$game_saves$` -> `$app_data_root$/savedgames/`

## Observed package layout (from user description)
```
Payload/StalkerMobile.app/
  StalkerMobile
  Info.plist
  ios_args.txt
  fsgame.ltx
  Data/
    gamedata.db0, gamedata.db1, ...
    gamedata/...
```

## Verified Mach-O dynamic dependencies (llvm-objdump)

Directly inspected the supplied `StalkerMobile` Mach-O with
`llvm-objdump --macho --dylibs-used`. Its linked dynamic libraries include:

- UI / lifecycle: `UIKit`, `Foundation`, `CoreFoundation`, `CoreGraphics`, `QuartzCore`
- Rendering: `Metal`, `CoreVideo`, `IOSurface`
- Audio: `AVFoundation`, `AVFAudio`, `AudioToolbox`, `CoreAudio`
- Input / haptics: `GameController` (weak), `CoreMotion`, `CoreHaptics` (weak)
- Other: `CoreBluetooth`, `UniformTypeIdentifiers`, `libobjc.A`, `libSystem.B`, `libc++.1`

The load commands include `LC_MAIN` and `LC_BUILD_VERSION`.
No separately linked `xrEngine.dylib` or `OpenXRay.framework` appears in
the binary's **direct dynamic dependency list**. This is consistent with
static engine linkage, but does **not** prove how its source was built.
It also does not prove that all these frameworks must be added manually
to our launcher: dependencies must be determined from the eventual
engine library's undefined symbols and runtime needs.

**Actionable next check:** once an arm64 engine archive is available,
inspect `nm -u` and link diagnostics to identify framework requirements;
then validate app lifecycle and Metal rendering on an actual iPhone.
This binary inspection does not extract reusable engine object files.

## Integration implications
1. `STArchiveStore` already imports archives and loose `gamedata`, plus optional `fsgame.ltx` and `ios_args.txt`.
2. `STXRayLauncherBridge` has a guarded resource-aware engine call, but the provided `MH_EXECUTE` cannot satisfy it.
3. **Do not** bundle the standalone `StalkerMobile` executable as if it were a library. This will not enable gameplay inside the launcher.
4. To produce a single playable IPA, build an iOS ARM64 engine library from compatible source and implement `st_xray_embedded_run_with_resources(command_line, resource_root)`; validate renderer, lifecycle, resource paths, and on-device launch.
5. The supplied binary and game archives remain user-owned and are **not** committed to this repository.

Status: inspected metadata only; no playable merged IPA or verified iOS build.
