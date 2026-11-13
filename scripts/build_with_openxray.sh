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
if [[ "$input" == *.zip ]]; then
  python3 scripts/integrate_openxray_artifact.py "$input"
  archive="$(pwd)/build-openxray/engine/libOpenXRayCombinedProbe.a"
fi
test -s "$archive" || { echo "Missing or empty archive: $archive" >&2; exit 1; }
python3 scripts/verify_openxray_archive.py "$archive"
command -v xcodegen >/dev/null || { echo "Install xcodegen first" >&2; exit 1; }
command -v xcodebuild >/dev/null || { echo "Requires macOS with Xcode" >&2; exit 1; }
xcodegen generate
# The archive must be force-loaded because the bridge uses a weak symbol.
# The final link may still fail due to missing upstream libraries/frameworks.
xcodebuild -project STALKERiOSStarter.xcodeproj \
  -scheme STALKERiOSStarter -configuration Debug \
  -sdk iphoneos -destination 'generic/platform=iOS' \
  -derivedDataPath build-openxray CODE_SIGNING_ALLOWED=NO \
  OPENXRAY_ARCHIVE_LINK_FLAGS="-Wl,-force_load,$archive" build
echo "Linked iOS app at build-openxray/Build/Products/Debug-iphoneos/STALKERiOSStarter.app"
echo "A successful link does NOT establish playable OpenXRay on iPhone."
