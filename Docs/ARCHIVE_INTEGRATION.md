# Archive inventory integration (v12)

The Swift implementation is at `Sources/Archive/STArchiveInventory.swift`. It is read-only and **does not parse or decompress X-Ray archives**.

Example call from the app after a user selects a folder (on iOS, handle security-scoped access):

```swift
let granted = folderURL.startAccessingSecurityScopedResource()
defer { if granted { folderURL.stopAccessingSecurityScopedResource() } }
let report = try STArchiveInventory.scan(directory: folderURL)
print(report.archives.map { "\($0.filename): \($0.bytes) bytes" })
print(report.missingNumbers)
print(report.warnings)
```

Do not treat a present gamedata.db0 as proof of valid game data. This scanner does not verify the archive directory, LZO streams, or engine compatibility.

Tests are in `Tests/STArchiveInventoryTests.swift` and require a configured XCTest target. They have **not** been run in GitHub Actions as part of this change. Next: wire the scanner into the actual import view, configure XCTest, then verify CI before attempting native X-Ray loading.

## iOS folder-picker screen

`Sources/Archive/STArchiveImportView.swift` is a standalone SwiftUI screen using
`.fileImporter(allowedContentTypes: [.folder])` and security-scoped access.

To expose it in the existing launcher navigation, add a navigation destination
or a sheet to the actual launcher view (do not replace the existing menu):

```swift
@State private var showingArchiveDiagnostics = false
Button("Check game archives") { showingArchiveDiagnostics = true }
    .sheet(isPresented: $showingArchiveDiagnostics) {
        STArchiveImportView()
    }
```

This is **not** a persistent import. iOS may revoke access when the picker
closes; a future implementation should coordinate a safe copy into the app
container (or persist a security-scoped bookmark where supported) before
attempting gameplay. The scanner reads the selected folder only while its
security scope is active. It does not recursively scan child folders.

The presence of a `gamedata.db0` file does not imply valid X-Ray assets.
This view has not been compiled by CI or tested on an iPhone yet.
