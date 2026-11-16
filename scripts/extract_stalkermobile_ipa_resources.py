#!/usr/bin/env python3
"""Extract user-owned StalkerMobile IPA resources for launcher import.

This intentionally does not extract or repackage the MH_EXECUTE binary.
It does not turn the standalone executable into a linkable X-Ray engine.
"""
import argparse
from pathlib import Path, PurePosixPath
import shutil
import sys
import zipfile

CONFIGS = {"fsgame.ltx", "ios_args.txt"}
PREFIX = PurePosixPath("Payload/StalkerMobile.app")
MAX_CONFIG = 1024 * 1024


def extract(ipa: Path, destination: Path) -> None:
    with zipfile.ZipFile(ipa) as archive:
        selected = []
        seen = set()
        for entry in archive.infolist():
            path = PurePosixPath(entry.filename)
            if path.is_absolute() or ".." in path.parts or "\\" in entry.filename:
                raise ValueError(f"Unsafe archive path: {entry.filename}")
            if path.parts[:2] != PREFIX.parts:
                continue
            relative = path.parts[2:]
            if not relative or entry.is_dir():
                continue
            # ZIP external attributes encode POSIX file type in the high bits.
            mode = (entry.external_attr >> 16) & 0o170000
            if mode not in (0, 0o100000):
                raise ValueError(f"Refusing non-regular archive entry: {entry.filename}")
            if len(relative) == 1 and relative[0] in CONFIGS:
                target = relative[0]
                if entry.file_size > MAX_CONFIG:
                    raise ValueError(f"Config too large: {entry.filename}")
            elif relative[0] == "Data" and len(relative) > 1:
                target = "/".join(relative[1:])
            else:
                continue
            if target in seen:
                raise ValueError(f"Duplicate destination: {target}")
            seen.add(target)
            selected.append((entry, target))
        if not selected:
            raise ValueError("No Data resources or root configs in StalkerMobile.app")
        if destination.exists() and any(destination.iterdir()):
            raise ValueError("Destination must be empty to avoid overwriting existing game files")
        destination.mkdir(parents=True, exist_ok=True)
        root = destination.resolve()
        for entry, target in selected:
            output = destination / target
            if not output.resolve().is_relative_to(root):
                raise ValueError(f"Unsafe output path: {target}")
            output.parent.mkdir(parents=True, exist_ok=True)
            with archive.open(entry) as source, output.open("wb") as sink:
                shutil.copyfileobj(source, sink, length=1024 * 1024)
        print(f"Extracted {len(selected)} resource files to {destination}")
        print("Import this folder using the launcher's existing folder importer.")
        print("No executable engine was extracted or linked.")


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("ipa", type=Path)
    parser.add_argument("destination", type=Path)
    args = parser.parse_args()
    try:
        extract(args.ipa, args.destination)
    except (OSError, ValueError, zipfile.BadZipFile) as error:
        print(f"Extraction failed: {error}", file=sys.stderr)
        sys.exit(1)
