#!/usr/bin/env python3
"""Classify a user-supplied iOS port Mach-O without executing it or copying assets.

Usage: python3 scripts/check_port_binary_linkability.py /path/to/StalkerMobile
Supports thin Mach-O and static ar archives. Fat binaries need slice inspection. This is a
format preflight, not proof of engine compatibility or runtime support.
"""
import argparse
import pathlib
import struct
import sys

ARM64 = 0x0100000C
MH_OBJECT = 1
MH_EXECUTE = 2
MH_DYLIB = 6
MH_BUNDLE = 8

def classify(data):
    if data.startswith(b"!<arch>\\n"):
        return "static archive (member architectures and symbols unverified)", True
    if len(data) < 32:
        return "truncated or unknown binary", False
    magic = data[:4]
    if magic in (b"\\xcf\\xfa\\xed\\xfe", b"\\xfe\\xed\\xfa\\xcf",
                 b"\\xce\\xfa\\xed\\xfe", b"\\xfe\\xed\\xfa\\xce"):
        endian = "<" if magic in (b"\\xcf\\xfa\\xed\\xfe", b"\\xce\\xfa\\xed\\xfe") else ">"
        cpu, subtype, kind = struct.unpack_from(endian + "III", data, 4)
        names = {MH_OBJECT: "MH_OBJECT", MH_EXECUTE: "MH_EXECUTE",
                 MH_DYLIB: "MH_DYLIB", MH_BUNDLE: "MH_BUNDLE"}
        return f"Mach-O {names.get(kind, kind)} cpu=0x{cpu:08x}", kind == MH_OBJECT and cpu == ARM64
    return "unsupported binary format (fat binaries require per-slice inspection)", False

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("binary", type=pathlib.Path)
    args = parser.parse_args()
    with args.binary.open("rb") as source:
        header = source.read(4096)
    kind, linkable = classify(header)
    print(kind)
    if not linkable:
        print("NOT directly linkable as an arm64 static engine object.", file=sys.stderr)
        print("A standalone MH_EXECUTE cannot be relinked into the launcher.", file=sys.stderr)
        return 1
    print("Format may be linkable; symbols, dependencies and iOS ABI still need validation.")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
