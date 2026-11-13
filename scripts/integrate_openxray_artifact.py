#!/usr/bin/env python3
"""Safely unpack a GitHub Actions OpenXRay archive artifact for iOS linkage.

Input is a downloaded GitHub Actions artifact ZIP, not an IPA. No game assets
are distributed. Does not claim the engine is playable on iOS.
"""
import argparse
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import zipfile


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("artifact_zip", type=Path)
    parser.add_argument("--output", type=Path, default=Path("build-openxray/engine"))
    args = parser.parse_args()
    source = args.artifact_zip.resolve()
    if not source.is_file():
        parser.error(f"artifact not found: {source}")
    try:
        artifact = zipfile.ZipFile(source)
    except (OSError, zipfile.BadZipFile) as error:
        parser.error(f"invalid artifact ZIP: {error}")
    with artifact, tempfile.TemporaryDirectory(prefix="openxray-artifact-") as temp:
        root = Path(temp)
        archive = artifact
        if len(archive.infolist()) > 10000:
            parser.error("artifact contains too many entries")
        candidates = []
        for member in archive.infolist():
            # Bound uncompressed size to avoid archive extraction bombs.
            if member.file_size > 4 * 1024 * 1024 * 1024:
                parser.error("archive member exceeds 4 GiB safety limit")
            name = Path(member.filename)
            if member.is_dir():
                continue
            if name.is_absolute() or ".." in name.parts or chr(92) in member.filename:
                parser.error("unsafe path inside artifact ZIP")
            if name.suffix == ".a":
                candidates.append(member)
        if len(candidates) != 1:
            parser.error(f"expected exactly one static .a archive; found {len(candidates)}")
        target = root / "engine.a"
        # Stream with an actual byte limit: ZIP metadata alone is not trustworthy.
        max_bytes = 4 * 1024 * 1024 * 1024
        with archive.open(candidates[0]) as stream, target.open("wb") as out:
            copied = 0
            while chunk := stream.read(1024 * 1024):
                copied += len(chunk)
                if copied > max_bytes:
                    parser.error("extracted archive exceeds 4 GiB safety limit")
                out.write(chunk)
        verifier = Path(__file__).with_name("verify_openxray_archive.py")
        result = subprocess.run([sys.executable, str(verifier), str(target)])
        if result.returncode:
            return result.returncode
        output = args.output.resolve()
        output.mkdir(parents=True, exist_ok=True)
        destination = output / "libOpenXRayCombinedProbe.a"
        shutil.copyfile(target, destination)
        print(f"Verified archive ready: {destination}")
        print(f"Next: bash scripts/build_with_openxray.sh '{destination}'")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
