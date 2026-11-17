#!/usr/bin/env bash
# Build an unsigned launcher IPA with a separately compiled iPhoneOS engine.
set -euo pipefail
if [[ $# -ne 1 ]]; then
  echo "Usage: bash scripts/build_with_openxray.sh /path/to/libOpenXRayCombinedProbe.a or artifact.zip" >&2
  exit 2
fi
if [[ ! -f "$1" ]]; then
  echo "Missing OpenXRay archive or artifact ZIP: $1" >&2
  exit 1
fi
python3 - "$1" <<'PY'
import pathlib
import struct
import sys

path = pathlib.Path(sys.argv[1])
with path.open("rb") as stream:
    header = stream.read(16)
magic = {bytes.fromhex("cffaedfe"): "<", bytes.fromhex("feedfacf"): ">"}
if len(header) >= 16 and header[:4] in magic:
    filetype = struct.unpack_from(magic[header[:4]] + "I", header, 12)[0]
    if filetype == 2:
        sys.exit("Standalone Mach-O MH_EXECUTE cannot be linked as an OpenXRay "
                 "static engine. Supply a rebuilt arm64 .a exporting "
                 "st_xray_embedded_run_with_resources.")
PY
input="$(cd "$(dirname "$1")" && pwd)/$(basename "$1")"
archive="$input"
command -v xcodebuild >/dev/null || { echo "Requires macOS with Xcode" >&2; exit 1; }
command -v xcodegen >/dev/null || { echo "Install xcodegen first" >&2; exit 1; }
if [[ "$input" == *.[zZ][iI][pP] ]]; then
  python3 scripts/integrate_openxray_artifact.py "$input"
  archive="$(pwd)/build-openxray/engine/libOpenXRayCombinedProbe.a"
fi
test -s "$archive" || { echo "Missing or empty archive: $archive" >&2; exit 1; }
python3 scripts/verify_openxray_archive.py --require-resources "$archive"
# The path is passed through an Xcode build setting as a linker argument.
if [[ "$archive" == *[[:space:]]* ]]; then
  echo "OpenXRay archive path must not contain whitespace: $archive" >&2
  exit 1
fi

xcodegen generate
# The native bridge has a strong reference to the entry. Normal archive
# linkage lets the linker pull the engine and dependencies it actually uses.
xcodebuild -project STALKERiOSStarter.xcodeproj \
  -scheme STALKERiOSStarter -configuration Release \
  -sdk iphoneos -destination 'generic/platform=iOS' \
  -derivedDataPath build-openxray CODE_SIGNING_ALLOWED=NO \
  ENABLE_DEBUG_DYLIB=NO STRIP_INSTALLED_PRODUCT=NO \
  OPENXRAY_ENGINE_DEFINES=ST_XRAY_STATIC_ENGINE_ADAPTER=1 \
  OPENXRAY_EXCLUDED_SOURCES="${OPENXRAY_EXCLUDED_SOURCES:-}" \
  OPENXRAY_ARCHIVE_LINK_FLAGS="$archive ${OPENXRAY_SYSTEM_LINK_FLAGS:-}" build

app="build-openxray/Build/Products/Release-iphoneos/STALKERiOSStarter.app"
app_binary="$app/STALKERiOSStarter"
test -s "$app_binary" || { echo "Missing linked iOS executable: $app_binary" >&2; exit 1; }
xcrun lipo "$app_binary" -verify_arch arm64
symbols="$(xcrun nm -gUj "$app_binary")"
if ! grep -Fxq _st_xray_embedded_run_with_resources <<< "$symbols"; then
  echo "Missing resource-aware OpenXRay entry in the iPhone executable" >&2
  exit 1
fi
ipa_output="$(pwd)/build-openxray/STALKERMobile-OpenXRay-unsigned.ipa"
python3 scripts/package_unsigned_ipa.py "$app" "$ipa_output"
echo "Unsigned device IPA: $ipa_output"
echo "Resource-aware X-Ray entry found in the linked iPhone executable."
