#!/usr/bin/env python3
"""Adapt an original fsgame.ltx for a sandboxed imported-game directory.

This prepares configuration only; it does not mount assets or launch OpenXRay.
"""
import argparse
from pathlib import Path
import re

ROOT_LINE = re.compile(r"^(\s*\$app_data_root\$\s*=\s*[^|\r\n]*\|\s*[^|\r\n]*\|).*$", re.I)

def adapt(source: str) -> str:
    lines = source.splitlines(keepends=True)
    matches = [i for i, line in enumerate(lines) if ROOT_LINE.match(line)]
    if len(matches) != 1:
        raise ValueError("Expected exactly one $app_data_root$ definition")
    index = matches[0]
    original = lines[index]
    ending = "\r\n" if original.endswith("\r\n") else "\n" if original.endswith("\n") else ""
    # Keep the original flags, but relocate writable saves/logs under the
    # game's imported root. Do not carry Windows user paths into the app.
    lines[index] = "$app_data_root$ = true| false| $fs_root$| appdata\\" + ending
    return "".join(lines)

def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("input", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    source = args.input.read_bytes()
    try:
        text = source.decode("utf-8-sig")
    except UnicodeDecodeError:
        text = source.decode("cp1251")
    result = adapt(text)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes(result.encode("utf-8"))

if __name__ == "__main__":
    main()
