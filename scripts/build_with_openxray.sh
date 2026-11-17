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

python3 scripts/prepare_ios_app_icon.py
xcodegen generate
# The native bridge has a strong reference to the entry. Normal archive
# linkage lets the linker pull the engine and dependencies it actually uses.
xcodebuild -project STALKERiOSStarter.xcodeproj \
  -scheme STALKERiOSStarter -configuration Release \
  -sdk iphoneos -destination 'generic/platform=iOS' \
  -derivedDataPath build-openxray CODE_SIGNING_ALLOWED=NO \
  ENABLE_DEBUG_DYLIB=NO STRIP_INSTALLED_PRODUCT=NO \
  PRODUCT_BUNDLE_IDENTIFIER=com.stalkermobile.ios.openxray \
  INFOPLIST_KEY_CFBundleDisplayName="STALKER X-Ray" \
  MARKETING_VERSION=1.0.5 CURRENT_PROJECT_VERSION=6 \
  INFOPLIST_KEY_CFBundleShortVersionString=1.0.5 INFOPLIST_KEY_CFBundleVersion=6 \
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
python3 - "$app" <<'PYTHON'
from pathlib import Path
import plistlib
import sys

app = Path(sys.argv[1])
path = app / "Info.plist"
info = plistlib.loads(path.read_bytes())
assert info["CFBundleIdentifier"] == "com.stalkermobile.ios.openxray"
assert info["CFBundleVersion"] == "6"
assert "rcp_scale * float2(offset)" in (app / "gather.ps").read_text()
icon_files = info["CFBundleIcons"]["CFBundlePrimaryIcon"]["CFBundleIconFiles"]
assert icon_files and all(any(app.glob(f"{name}*.png")) for name in icon_files)
info.update(CFBundleName="STALKER X-Ray", CFBundleIconFiles=icon_files,
            CFBundleIconFile=icon_files[0], UIFileSharingEnabled=True,
            LSSupportsOpeningDocumentsInPlace=True)
path.write_bytes(plistlib.dumps(info))
PYTHON
xcrun strip -x "$app_binary"
if /usr/bin/codesign -d "$app_binary" >/dev/null 2>&1; then
  /usr/bin/codesign --remove-signature "$app_binary"
fi
verify="$(mktemp -d)"
trap 'rm -rf "$verify"' EXIT
ditto "$app" "$verify/${app##*/}"
/usr/bin/codesign --force --sign - --timestamp=none "$verify/${app##*/}"
/usr/bin/codesign --verify --strict "$verify/${app##*/}"
/usr/bin/codesign -dv --verbose=6 "$verify/${app##*/}" > "$verify/signature.txt" 2>&1
info_hash="$(shasum -a 256 "$app/Info.plist" | awk '{print $1}')"
grep -Eq "^[[:space:]]*-1=$info_hash$" "$verify/signature.txt"
ipa_output="$(pwd)/build-openxray/STALKERMobile-OpenXRay-unsigned.ipa"
python3 scripts/package_unsigned_ipa.py "$app" "$ipa_output"
echo "Unsigned device IPA: $ipa_output"
echo "Resource-aware X-Ray entry found in the linked iPhone executable."
