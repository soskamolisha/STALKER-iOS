"""Read-only PE architecture inventory of Windows binaries in a ZIP."""
import argparse
import json
from pathlib import Path
import struct
import zipfile

MAX_PREFIX = 4096
MACHINES = {0x14c: "x86", 0x8664: "x86_64", 0xaa64: "arm64"}


def parse_pe_header(data):
    if len(data) < 64 or data[:2] != b"MZ":
        return {"pe": False, "reason": "no_mz_header"}
    offset = struct.unpack_from("<I", data, 0x3c)[0]
    if offset > MAX_PREFIX - 24 or offset + 24 > len(data):
        return {"pe": False, "reason": "pe_header_outside_prefix"}
    if data[offset:offset + 4] != b"PE\0\0":
        return {"pe": False, "reason": "no_pe_signature"}
    machine, sections, _, _, _, optional_size, flags = struct.unpack_from(
        "<HHIIIHH", data, offset + 4
    )
    return {"pe": True, "machine_hex": f"0x{machine:04x}",
            "architecture": MACHINES.get(machine, "unknown"),
            "sections": sections, "optional_header_size": optional_size,
            "dll": bool(flags & 0x2000)}


def inventory(zip_path, max_entries=10000):
    if not 1 <= max_entries <= 100000:
        raise ValueError("max_entries out of range")
    entries = []
    with zipfile.ZipFile(zip_path) as archive:
        members = archive.infolist()
        if len(members) > max_entries:
            raise ValueError("ZIP entry limit exceeded")
        for member in members:
            name = member.filename
            if (member.is_dir() or name.startswith("__MACOSX/")
                    or not name.lower().endswith((".exe", ".dll"))):
                continue
            item = {"name": name, "size": member.file_size}
            if member.flag_bits & 1:
                item["error"] = "encrypted_member"
            else:
                try:
                    with archive.open(member) as stream:
                        item.update(parse_pe_header(stream.read(MAX_PREFIX)))
                except (OSError, RuntimeError, EOFError, ValueError,
                        zipfile.BadZipFile, NotImplementedError) as error:
                    item["error"] = type(error).__name__
            entries.append(item)
    return {"files": entries, "count": len(entries),
            "warning": "PE inspection only; no executable code is run"}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("bin_zip", type=Path)
    args = parser.parse_args()
    try:
        print(json.dumps(inventory(args.bin_zip), indent=2))
    except (OSError, ValueError, zipfile.BadZipFile) as error:
        parser.error(str(error))


if __name__ == "__main__":
    main()
