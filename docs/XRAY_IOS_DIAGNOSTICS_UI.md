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
