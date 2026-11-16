#!/usr/bin/env bash
# Build the portable LZO1X decoder for iPhone arm64 on a macOS/Xcode runner.
# The upstream archive is fetched at build time; no game assets are bundled.
set -euo pipefail

: "${LZO_VERSION:=2.10}"
: "${IOS_DEPLOYMENT_TARGET:=17.0}"
: "${BUILD_DIR:=${PWD}/build/minilzo-ios}"
mkdir -p "$BUILD_DIR"
BUILD_DIR="$(cd "$BUILD_DIR" && pwd)"
ARCHIVE="$BUILD_DIR/lzo-${LZO_VERSION}.tar.gz"
SOURCE_DIR="$BUILD_DIR/lzo-${LZO_VERSION}"

if [[ "$(uname -s)" != "Darwin" ]]; then
  echo "This script requires macOS with Xcode installed" >&2
  exit 1
fi
if [[ ! -f "$SOURCE_DIR/minilzo/minilzo.c" ]]; then
  curl --fail --location --retry 3 --output "$ARCHIVE" \
    "https://www.oberhumer.com/opensource/lzo/download/lzo-${LZO_VERSION}.tar.gz"
  tar -xzf "$ARCHIVE" -C "$BUILD_DIR"
fi
if [[ ! -f "$SOURCE_DIR/minilzo/minilzo.h" ]]; then
  echo "Upstream minilzo.h missing" >&2
  exit 1
fi

SDK="$(xcrun --sdk iphoneos --show-sdk-path)"
CC="$(xcrun --sdk iphoneos --find clang)"
AR="$(xcrun --sdk iphoneos --find ar)"
"$CC" -arch arm64 -isysroot "$SDK" \
  -miphoneos-version-min="$IOS_DEPLOYMENT_TARGET" -O2 -fPIC \
  -I"$SOURCE_DIR/minilzo" \
  -c "$SOURCE_DIR/minilzo/minilzo.c" -o "$BUILD_DIR/minilzo.o"
"$AR" rcs "$BUILD_DIR/libminilzo.a" "$BUILD_DIR/minilzo.o"
echo "LZO library: $BUILD_DIR/libminilzo.a"
echo "For xcodebuild set:"
echo "  ST_LZO_HEADER_PATH=$SOURCE_DIR/minilzo"
echo "  ST_LZO_DEFINES=ST_HAVE_LZO1X=1"
echo "  ST_LZO_LINK_FLAGS=$BUILD_DIR/libminilzo.a"
