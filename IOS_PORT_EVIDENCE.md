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

## Verified iPhoneOS ARM64 X-Ray engine compilation (2026-11-11)

- [Full OpenXRay iOS compilation run 43013395733](https://github.com/soskamolisha/STALKER-iOS/actions/runs/43013395733): successful, including `xrGame` and the linked `xr_3da` executable.
- [Engine bundle inspection run 43021340872](https://github.com/soskamolisha/STALKER-iOS/actions/runs/43021340872): `file` identified `xr_3da` as a 64-bit ARM64 Mach-O executable; `codesign --verify` reported no signature.
- [Unsigned engine IPA packaging run 43025040350](https://github.com/soskamolisha/STALKER-iOS/actions/runs/43025040350): successful, artifact [openxray-engine-unsigned-test-ipa](https://github.com/soskamolisha/STALKER-iOS/actions/runs/43025040350/artifacts/13860577733). The bundle identifier is `com.stalkermobile.openxray.engine`; `unzip -t` passed.

**Scope limitation:** The packaged executable is an **engine-only test artifact**, not the SwiftUI trilogy launcher. No code-signing, device launch, game asset loading, renderer operation, or gameplay has been verified. The launcher still uses `UnavailableEngine` and does not link the OpenXRay runtime. Do not describe the test IPA as a playable S.T.A.L.K.E.R. port.

**Integration requirements:** Build the engine as an embeddable iOS library/framework or adapt the launcher into the engine's application entry point; a second `xr_3da.app` executable cannot simply be copied into a SwiftUI app and launched as a subprocess on iOS. Establish UIKit/SDL lifecycle, renderer compatibility, filesystem mapping for imported original game data, touch input, audio, and Apple signing/provisioning. Verify the engine edition against each game independently: xray-16 does not automatically establish Shadow of Chernobyl compatibility.
