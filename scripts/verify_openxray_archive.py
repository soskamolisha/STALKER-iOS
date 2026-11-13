#!/usr/bin/env python3
"""Validate an optional arm64 OpenXRay archive before attempting iOS linkage.

Run on macOS with Xcode tools installed. This script does not build the
engine, sign an IPA, or claim that upstream X-Ray supports UIKit/Metal.
"""
import argparse
from pathlib import Path
import subprocess
import sys


def run(*args):
    return subprocess.run(args, text=True, capture_output=True, check=True).stdout


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("archive", type=Path)
    args = parser.parse_args()
    archive = args.archive.resolve()
    if not archive.is_file() or archive.stat().st_size == 0:
        parser.error(f"archive missing or empty: {archive}")
    try:
        run("xcrun", "lipo", "-verify_arch", "arm64", str(archive))
        symbols = run("xcrun", "nm", "-gU", str(archive))
    except (OSError, subprocess.CalledProcessError) as error:
        print(f"Archive validation failed: {error}", file=sys.stderr)
        return 1
    if "_st_xray_embedded_run" not in symbols.split():
        print("Missing st_xray_embedded_run export", file=sys.stderr)
        return 1
    # The entry symbol is weak in the launcher bridge; a static archive
    # therefore requires force-loading or the linker may omit the engine.
    print("Validated arm64 OpenXRay archive and embedded entry symbol.")
    # xcodebuild receives the flag as one build-setting argument.
    print(f"Xcode OTHER_LDFLAGS: -Wl,-force_load,{archive}")
    print("WARNING: upstream platform dependencies and game execution remain unverified.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
