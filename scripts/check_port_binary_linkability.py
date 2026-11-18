#!/usr/bin/env python3
"""Inspect a supplied iOS port binary without executing or distributing it.

A successful format check is not proof of ABI, exported entry points, or gameplay.
"""
import argparse
from pathlib import Path
import struct
import sys

ARM64 = 0x0100000C
MACHO_TYPES = {1: "MH_OBJECT", 2: "MH_EXECUTE", 6: "MH_DYLIB", 8: "MH_BUNDLE"}
MAGICS = {
    bytes.fromhex("cffaedfe"): "<",
    bytes.fromhex("feedfacf"): ">",
    bytes.fromhex("cefaedfe"): "<",
    bytes.fromhex("feedface"): ">",
}

def classify(data):
    if data.startswith(bytes.fromhex("213c617263683e0a")):
        return "static ar archive (architecture and symbols unverified)", True
    if len(data) < 16:
        return "truncated or unknown binary", False
    endian = MAGICS.get(data[:4])
    if endian is None:
        return "unsupported format (fat Mach-O slices not inspected)", False
    cpu, _subtype, kind = struct.unpack_from(endian + "III", data, 4)
    name = MACHO_TYPES.get(kind, str(kind))
    return f"Mach-O {name} cpu=0x{cpu:08x}", cpu == ARM64 and kind == 1

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("binary", type=Path)
    args = parser.parse_args()
    with args.binary.open("rb") as stream:
        data = stream.read(4096)
    kind, candidate = classify(data)
    print(kind)
    if not candidate:
        print("Not directly linkable as an arm64 static engine object.", file=sys.stderr)
        return 1
    print("Candidate only: inspect arm64 slices, exported ABI and dependencies.")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
