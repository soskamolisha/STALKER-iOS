# User-supplied Shadow of Chernobyl bin folder: binary inventory

Source: privately supplied `bin.zip`, inspected locally. **No proprietary
binaries are committed to this repository.**

The archive contains 77 ZIP entries, including macOS `__MACOSX/._*`
metadata and directories. Total uncompressed size is 23,938,752 bytes.

Observed components:
- `bin/XR_3DA.exe` — 1,636,424 bytes
- `bin/xrGame.dll` — 6,246,400 bytes
- `bin/xrCore.dll` — 856,064 bytes
- `bin/xrRender_R1.dll` — 438,272 bytes
- `bin/xrRender_R2.dll` — 540,672 bytes
- `bin/xrLUA.dll` — 290,816 bytes
- `bin/xrSound.dll` — 229,376 bytes
- `bin/xrParticles.dll` — 98,304 bytes
- `bin/xrCDB.dll` — 81,920 bytes
- `bin/xrXMLParser.dll` — 57,344 bytes
- `bin/ode.dll` — 196,608 bytes
- `bin/d3dx9_31.dll`, `bin/d3dx9_34.dll` — DirectX dependencies
- `bin/OpenAL32.dll` — audio dependency

PE header inspection of `XR_3DA.exe`, `xrCore.dll`, `xrGame.dll`,
and `xrRender_R1.dll` confirmed DOS `MZ` signature, `PE\\0\\0`
signature and COFF machine `0x014c` (Intel 386 / 32-bit x86).

## Implications for iOS

These are compiled Windows x86 binaries, **not source code**, and cannot be
linked into an arm64 iOS app. A working iOS port requires compatible,
legally usable X-Ray source and replacement/porting of Windows and DirectX
interfaces, plus platform-specific input, audio, and packaging.

This inventory helps identify modules and dependencies but does not prove
that any X-Ray game loop runs on iOS. The `bin` archive also does not
establish the internal layout of `gamedata.db0`.

Next useful engineering task: map the portable engine modules and their
platform dependencies against the available X-Ray source tree; do not copy
the user-supplied commercial binaries into the public repository.
