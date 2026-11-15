#!/usr/bin/env bash
# Optional local Xcode build with a separately produced OpenXRay arm64 archive.
# Does not download proprietary game data, sign an IPA or start paid CI.
set -euo pipefail
if [[ $# -ne 1 ]]; then
  echo "Usage: bash scripts/build_with_openxray.sh /path/to/libOpenXRayCombinedProbe.a or artifact.zip" >&2
  exit 2
fi
if [[ ! -f "$1" ]]; then
  echo "Missing OpenXRay archive or artifact ZIP: $1" >&2
  exit 1
fi
input="$(cd "$(dirname "$1")" && pwd)/$(basename "$1")"
archive="$input"
# Refuse to perform a paid-platform build on an unsupported host.
command -v xcodebuild >/dev/null || { echo "Requires macOS with Xcode" >&2; exit 1; }
command -v xcodegen >/dev/null || { echo "Install xcodegen first" >&2; exit 1; }
if [[ "$input" == *.zip ]]; then
  python3 scripts/integrate_openxray_artifact.py "$input"
  archive="$(pwd)/build-openxray/engine/libOpenXRayCombinedProbe.a"
fi
test -s "$archive" || { echo "Missing or empty archive: $archive" >&2; exit 1; }
python3 scripts/verify_openxray_archive.py --require-resources "$archive"
# The archive path is passed through an Xcode build setting; whitespace can
# split -force_load into separate linker arguments and silently break linkage.
if [[ "$archive" == *[[:space:]]* ]]; then
  echo "OpenXRay archive path must not contain whitespace: $archive" >&2
  exit 1
fi
xcodegen generate
# The archive must be force-loaded because the bridge uses a weak symbol.
# The final link may still fail due to missing upstream libraries/frameworks.
xcodebuild -project STALKERiOSStarter.xcodeproj \
  -scheme STALKERiOSStarter -configuration Debug \
  -sdk iphoneos -destination 'generic/platform=iOS' \
  -derivedDataPath build-openxray CODE_SIGNING_ALLOWED=NO \
  OPENXRAY_ARCHIVE_LINK_FLAGS="-Wl,-force_load,$archive" build
# A successful link alone is insufficient: the launcher resolves these entry
# points with dlsym(RTLD_DEFAULT), so they must remain exported by the app.
app_binary="build-openxray/Build/Products/Debug-iphoneos/STALKERiOSStarter.app/STALKERiOSStarter"
test -s "$app_binary" || { echo "Missing linked iOS executable: $app_binary" >&2; exit 1; }
symbols="$(xcrun nm -gU "$app_binary")"
for symbol in _st_xray_embedded_run _st_xray_embedded_run_with_resources; do
  if ! grep -Eq "[[:space:]]T[[:space:]]${symbol}$" <<< "$symbols"; then
    echo "Missing exported runtime symbol: ${symbol}; dlsym cannot start the engine" >&2
    exit 1
  fi
done
echo "Both embedded X-Ray entry points are exported; gameplay still unverified."
echo "Linked iOS app at build-openxray/Build/Products/Debug-iphoneos/STALKERiOSStarter.app"
echo "A successful link does NOT establish playable OpenXRay on iPhone."
