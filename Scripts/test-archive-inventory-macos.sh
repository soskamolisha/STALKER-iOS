#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT
cp "$ROOT/Sources/Archive/STArchiveInventory.swift" "$TMP/STArchiveInventory.swift"
cat > "$TMP/main.swift" <<'SWIFT'
import Foundation

let root = FileManager.default.temporaryDirectory.appendingPathComponent(UUID().uuidString)
try FileManager.default.createDirectory(at: root, withIntermediateDirectories: true)
defer { try? FileManager.default.removeItem(at: root) }
func put(_ name: String, _ bytes: [UInt8]) throws {
    try Data(bytes).write(to: root.appendingPathComponent(name))
}
try put("gamedata.db0", [0x50, 0x4B, 0x03, 0x04])
var report = try STArchiveInventory.scan(directory: root)
precondition(!report.hasBaseArchive)
precondition(report.warnings.contains { $0.contains("ZIP content") && $0.contains("gamedata.db0") })
try put("gamedata.db00", [1, 2, 3])
report = try STArchiveInventory.scan(directory: root)
precondition(report.hasBaseArchive && report.archives.count == 1)
precondition(!report.isEngineReady)
try put("gamedata.db2", [1])
report = try STArchiveInventory.scan(directory: root)
precondition(report.missingNumbers == [1])
print("Archive inventory macOS smoke tests passed")
SWIFT
swiftc "$TMP/STArchiveInventory.swift" "$TMP/main.swift" -o "$TMP/archive-test"
"$TMP/archive-test"
