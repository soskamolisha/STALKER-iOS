#!/usr/bin/env python3
"""Inspect the first bytes of a .db member inside a ZIP, without extracting files.

Only a small prefix is decompressed into memory. This is not a .db decoder.
"""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import zipfile

MAX_PREFIX = 4096
DB_SUFFIXES = (".db0", ".db1", ".db2", ".db3")


def peek_db_members(path: Path, prefix_bytes: int = 64, max_members: int = 10):
    if not 4 <= prefix_bytes <= MAX_PREFIX:
        raise ValueError("prefix_bytes must be between 4 and 4096")
    if not 1 <= max_members <= 100:
        raise ValueError("max_members must be between 1 and 100")
    output = []
    with zipfile.ZipFile(path) as archive:
        for member in archive.infolist():
            if len(output) >= max_members:
                break
            if member.is_dir() or not member.filename.lower().endswith(DB_SUFFIXES):
                continue
            item = {"name": member.filename, "declared_size": member.file_size,
                    "compression_method": member.compress_type}
            if member.flag_bits & 1:
                item["error"] = "encrypted_zip_member"
            else:
                try:
                    with archive.open(member, "r") as stream:
                        data = stream.read(prefix_bytes)
                    item["prefix_hex"] = data.hex()
                    item["prefix_bytes_read"] = len(data)
                    item["prefix_sha256"] = hashlib.sha256(data).hexdigest()
                    if len(data) >= 4:
                        item["first_u32_le"] = struct.unpack_from("<I", data)[0]
                except (OSError, RuntimeError, ValueError, EOFError,
                        NotImplementedError, zipfile.BadZipFile) as error:
                    item["error"] = type(error).__name__
            output.append(item)
    return output


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("zip_path", type=Path)
    parser.add_argument("--prefix-bytes", type=int, default=64)
    args = parser.parse_args()
    try:
        data = peek_db_members(args.zip_path, args.prefix_bytes)
    except (OSError, ValueError, zipfile.BadZipFile) as error:
        parser.error(str(error))
    print(json.dumps({"members": data, "warning": "Prefix inspection only; no X-Ray format verification"},
                     indent=2, ensure_ascii=False))


if __name__ == "__main__":
    main()
