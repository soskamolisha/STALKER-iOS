# Verified PE import dependency map — user-supplied bin.zip

Inspected locally from the user's privately supplied `bin.zip` by reading
PE import descriptors. **No commercial binaries were uploaded to GitHub.**

## Direct imports observed

| Module | Selected imported DLLs |
|---|---|
| `XR_3DA.exe` | `xrSound.dll`, `xrLUA.dll`, `xrCDB.dll`, `xrCore.dll`, `DINPUT8.dll`, `d3dx9_34.dll`, `WINMM.dll`, `USER32.dll` |
| `xrGame.dll` | `xrSound.dll`, `xrXMLParser.dll`, `xrLUA.dll`, `xrNetServer.dll`, `xrCore.dll`, `ODE.dll`, `XR_3DA.exe` |
| `xrCore.dll` | `KERNEL32.dll`, `USER32.dll`, `comdlg32.dll`, `ADVAPI32.dll`, `ole32.dll`, `BugTrap.dll`, `WINMM.dll`, `dbghelp.dll` |
| `xrSound.dll` | `OpenAL32.dll`, `xrCDB.dll`, `xrCore.dll`, `EAX.DLL`, `DSOUND.dll` |
| `xrLUA.dll` | `xrCore.dll`, `KERNEL32.dll` |
| `xrRender_R1.dll` | `xrParticles.dll`, `xrCDB.dll`, `xrCore.dll`, `XR_3DA.exe`, `d3dx9_34.dll` |
| `xrRender_R2.dll` | `xrParticles.dll`, `xrCDB.dll`, `xrCore.dll`, `XR_3DA.exe`, `d3dx9_34.dll` |

All selected binaries also import Microsoft C/C++ runtime libraries
`MSVCP80.dll` and/or `MSVCR80.dll`. The table lists **direct PE imports
only**; it does not capture delay-loaded or runtime-loaded libraries.

## Practical iOS port order (proposal, not achieved milestones)

1. Establish an **arm64 iOS native executable** with logging and lifecycle
   tests. The existing SwiftUI launcher is not evidence of X-Ray runtime.
2. Port the minimal `xrCore` facilities using compatible, legally usable
   source code; replace Win32 process, file, threading, and diagnostics APIs.
3. Port required data/configuration and collision support (`xrXMLParser`,
   `xrCDB`) and validate with synthetic tests.
4. Port Lua integration, physics, audio, and gameplay modules, preserving
   required interfaces. Replace DirectSound/EAX with an iOS-compatible audio
   backend where needed.
5. Implement a Metal renderer instead of linking DirectX 9 DLLs.
6. Integrate the actual game loop, original user-provided game data,
   input controls, and on-device crash/performance diagnostics.

**Dependency caveat:** `xrGame.dll` and the renderers import
`XR_3DA.exe`; this means a simple independent-library port order is
insufficient. The executable exports/interfaces must be identified and
refactored. Imports show linkage requirements, not source availability.

## Current limits

The proprietary x86 DLLs cannot be linked into an iOS arm64 app. This map
does not establish successful archive decoding or gameplay on an iPhone.
