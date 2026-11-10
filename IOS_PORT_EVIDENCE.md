# iOS OpenXRay port: evidence from user-supplied files

This is an interoperability investigation, **not** a claim that the executable or its shaders are reusable in this repository. Do not commit proprietary game archives or extracted third-party code without checking rights and license obligations.

## Observed files

- `StalkerMobile`: 64-bit ARM64 Mach-O executable, about 68 MB (decimal). Binary strings include `OpenXRay`, `SDL_Init(flags) == 0`, `xrEngine`, `openxray.ltx`, and a build path ending in `xray-ios-soc/src/xrEngine/x_ray.cpp`.
- `ios_args.txt`: `-soc -nointro -nosplash`. This is direct evidence for the Shadow of Chernobyl launch mode only; `-cs` and `-cop` are **unverified guesses** for other games.
- `Data/`: `fsgame.ltx`, `gamedata/`, and packed archives `gamedata.db0` through `gamedata.dbd` (as shown in the user's directory listing).
- `Data/gamedata/`: `config`, `scripts`, `shaders`, `textures`. No loose `levels` directory was shown; levels may be in packed archives.
- `gl.zip`: 447 entries (200 `.ps`, 87 `.vs`, 102 `.h`, 58 `.s`).
- `gl_r1.zip`: 529 entries (229 `.ps`, 153 `.vs`, 105 `.h`, 42 `.s`).
- Sample shader `gl/deffer_detail_s_flat.vs` contains `#include "common.h"`, `uniform highp float4`, and an `_main(v_detail v)` entry. This is engine-specific shader source and cannot simply be compiled as Metal Shading Language.

## Engineering implications

1. Investigate appropriately licensed OpenXRay source and identify its iOS/SDL platform integration. A compiled third-party Mach-O is not a substitute for source or a redistributable engine.
2. Keep the import layout compatible with both `gamedata/` and `gamedata.db*` archives; archive detection alone does **not** decode or mount them.
3. Inspect shader preprocessor, `gl` and `gl_r1` renderer variants, and the actual GL-to-iOS graphics backend before choosing a Metal conversion path. Linking Metal does not prove that game rendering uses Metal.
4. Keep game launch arguments edition-specific and verified against engine source. The `-soc` flag is observed; other flags need verification.
5. Do not claim playable gameplay or a successful device build until CI and real-device tests prove it.

## Next validation steps

- Identify a public source repository and its license/revision, if available.
- Test archive table-of-contents parsing with small, legally supplied samples.
- Implement engine startup behind an explicit feature gate once an iOS-compatible engine is available.
