#!/usr/bin/env python3
"""Read-only diagnostic for original X-Ray gamedata archives.

Does not extract, decrypt, or modify game data.
"""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import zipfile


def inspect(path: Path, sample_size: int = 65536) -> dict:
    if not path.is_file():
        raise FileNotFoundError(path)
    size = path.stat().st_size
    with path.open("rb") as f:
        head = f.read(min(size, sample_size))
        f.seek(max(0, size - 64))
        tail = f.read()
    result = {
        "filename": path.name,
        "size_bytes": size,
        "first_64_bytes_hex": head[:64].hex(),
        "last_64_bytes_hex": tail.hex(),
        "sample_sha256": hashlib.sha256(head).hexdigest(),
        "sample_size": len(head),
        "zip_container": zipfile.is_zipfile(path),
    }
    if size >= 4:
        result["first_u32_le"] = struct.unpack_from("<I", head)[0]
        result["first_u32_be"] = struct.unpack_from(">I", head)[0]
    if result["zip_container"]:
        with zipfile.ZipFile(path) as z:
            infos = z.infolist()
            result["zip_entries"] = len(infos)
            result["zip_total_uncompressed_bytes"] = sum(i.file_size for i in infos)
            result["zip_first_entries"] = [
                {"name": i.filename, "size": i.file_size,
                 "compressed_size": i.compress_size, "method": i.compress_type}
                for i in infos[:10]
            ]
    else:
        result["note"] = (
            "Not a ZIP container. A .db0 file may use a proprietary X-Ray "
            "archive layout; this probe does not assert a format or decode it."
        )
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("archive", type=Path)
    args = parser.parse_args()
    print(json.dumps(inspect(args.archive), ensure_ascii=False, indent=2))


if __name__ == "__main__":
    main()
