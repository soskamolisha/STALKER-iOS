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

## Integration implications
1. `STArchiveStore` already imports archives and loose `gamedata`, plus optional `fsgame.ltx` and `ios_args.txt`.
2. `STXRayLauncherBridge` has a guarded resource-aware engine call, but the provided `MH_EXECUTE` cannot satisfy it.
3. **Do not** bundle the standalone `StalkerMobile` executable as if it were a library. This will not enable gameplay inside the launcher.
4. To produce a single playable IPA, build an iOS ARM64 engine library from compatible source and implement `st_xray_embedded_run_with_resources(command_line, resource_root)`; validate renderer, lifecycle, resource paths, and on-device launch.
5. The supplied binary and game archives remain user-owned and are **not** committed to this repository.

Status: inspected metadata only; no playable merged IPA or verified iOS build.
