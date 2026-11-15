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
