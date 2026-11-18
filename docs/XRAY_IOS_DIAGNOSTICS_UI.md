# Native file diagnostics UI integration

The read-only diagnostics UI is available in `Sources/XRayChunkDiagnosticView.swift`.
The opt-in launcher control is `Sources/XRayDiagnosticsAccess.swift`.

To enable it, apply the modifier to the **existing** launcher's root SwiftUI view:

```swift
LauncherRootView()
    .xrayDiagnosticsAccess()
```

Do not create a second `@main` application or replace the existing launcher.
The control opens a sheet with a file picker; it reads at most 4 MiB plus
one byte to detect oversized inputs. It scans **generic candidate chunks**,
not actual `gamedata.db0` archive directories. Do not label a successful
scan as proof of an X-Ray-compatible archive or working game engine.

The native bridge is declared in `Sources/Native/Bridge.h`, backed by
`Sources/Native/Portable/ChunkReaderC.cpp`. Native test executables
live in `tests/native` so their `main()` functions do not enter the
iOS app target.

**Verification required:** compile the iOS app and test the picker on
a signed physical-device build. No gameplay claim follows from these tests.

## Launcher wiring (implemented)

The actual app entry point is `Sources/StalkerStarterApp.swift`, which
presents `TrilogyLauncherView`. The launcher now contains an **Inspect .db
archive** button under its Diagnostics & Tests section. The button opens
`XRayChunkDiagnosticView` in a dismissible sheet; no extra modifier is
required. `XRayDiagnosticsAccess.swift` remains an optional integration
helper and is not applied to the launcher.

The .db diagnostic reads up to 32 bytes at each end of the selected archive,
reports its size and allows sharing the text. It does **not** list or extract
files inside the archive. Generic chunk diagnostics are limited to 4 MiB.

**Important:** The repository also contains older, separate native X-Ray
resource and archive probes. A successful probe or chunk scan must not be
presented as proof that the game engine, renderer, or gameplay works.

Verify the iOS starter workflow after this integration, then test the
file picker and sheet dismissal on a signed device build.
