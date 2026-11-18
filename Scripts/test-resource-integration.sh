#!/usr/bin/env bash
set -euo pipefail
PROJECT_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
TEST_ROOT="$(mktemp -d)"
trap 'rm -rf "$TEST_ROOT"' EXIT
mkdir -p "$TEST_ROOT/Sources/STALKERiOSStarter" "$TEST_ROOT/Tests/ResourceTests"
for source in XRayGameResourcePaths.swift GameResourceImporter.swift GameResourceScanner.swift XRayArchiveInspector.swift \
              Archive/STArchiveInventory.swift Archive/STArchiveStore.swift; do
    cp "$PROJECT_ROOT/Sources/$source" "$TEST_ROOT/Sources/STALKERiOSStarter/"
done
for source in GameResourceIntegrationTests.swift STArchiveInventoryTests.swift; do
    cp "$PROJECT_ROOT/Tests/$source" "$TEST_ROOT/Tests/ResourceTests/"
done
cat > "$TEST_ROOT/Package.swift" <<'SWIFT'
// swift-tools-version: 5.9
import PackageDescription
let package = Package(
    name: "ResourceIntegration",
    platforms: [.macOS(.v13)],
    targets: [
        .target(name: "STALKERiOSStarter"),
        .testTarget(name: "ResourceTests", dependencies: ["STALKERiOSStarter"])
    ]
)
SWIFT
swift test --package-path "$TEST_ROOT"
