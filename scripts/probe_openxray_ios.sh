#!/usr/bin/env bash
# First real upstream iOS cross-compilation probe; no paid CI or game assets.
set -euo pipefail
if [[ "$(uname -s)" != Darwin ]]; then
  echo "iOS SDK requires macOS/Xcode; Linux ARM64 is not an iOS binary" >&2
  exit 2
fi
for tool in xcrun cmake git; do
  command -v "$tool" >/dev/null || { echo "Missing $tool" >&2; exit 2; }
done
sdk="$(xcrun --sdk iphoneos --show-sdk-path)"
clang="$(xcrun --sdk iphoneos --find clang)"
clangxx="$(xcrun --sdk iphoneos --find clang++)"
root="${OPENXRAY_SOURCE_DIR:-build-openxray/upstream}"
if [[ ! -f "$root/CMakeLists.txt" ]]; then
  mkdir -p "$(dirname "$root")"
  git clone --depth 1 --recurse-submodules --branch dev https://github.com/OpenXRay/xray-16.git "$root"
fi
# This probe intentionally does not claim that upstream currently supports iOS.
# Capture the first concrete missing dependency/platform incompatibility.
cmake -S "$root" -B build-openxray/ios-probe -G Xcode \
  -DCMAKE_SYSTEM_NAME=iOS \
  -DCMAKE_OSX_SYSROOT="$sdk" \
  -DCMAKE_OSX_ARCHITECTURES=arm64 \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=17.0 \
  -DCMAKE_C_COMPILER="$clang" \
  -DCMAKE_CXX_COMPILER="$clangxx" \
  -DBUILD_SHARED_LIBS=OFF \
  -DMEMORY_ALLOCATOR=standard \
  -DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY
echo "OpenXRay iOS configure succeeded; this is NOT proof of a linked or playable engine."
echo "Next: cmake --build build-openxray/ios-probe --config Release --parallel 3"
