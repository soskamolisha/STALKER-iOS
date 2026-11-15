#!/usr/bin/env python3
"""Read-only diagnostic for original X-Ray game archives.

Inspects a small raw-file prefix and suffix, or ZIP central-directory metadata.
Does not extract, decrypt, or modify game data.
"""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import zipfile

MAX_LISTED_ENTRIES = 10
MAX_ARCHIVE_ENTRIES = 100000


def inspect(path: Path, sample_size: int = 65536) -> dict:
    if sample_size < 4 or sample_size > 1024 * 1024:
        raise ValueError("sample_size must be between 4 bytes and 1 MiB")
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
    if len(head) >= 4:
        result["first_u32_le"] = struct.unpack_from("<I", head)[0]
        result["first_u32_be"] = struct.unpack_from(">I", head)[0]
    if result["zip_container"]:
        with zipfile.ZipFile(path) as archive:
            count = 0
            total_size = 0
            first_entries = []
            candidate_db_members = []
            for entry in archive.infolist():
                count += 1
                if count > MAX_ARCHIVE_ENTRIES:
                    raise ValueError("ZIP has too many entries for diagnostic scan")
                total_size += entry.file_size
                if len(first_entries) < MAX_LISTED_ENTRIES:
                    first_entries.append({
                        "name": entry.filename,
                        "size": entry.file_size,
                        "compressed_size": entry.compress_size,
                        "method": entry.compress_type,
                    })
                if entry.filename.lower().endswith((".db0", ".db1", ".db2", ".db3")):
                    candidate_db_members.append({
                        "name": entry.filename,
                        "size": entry.file_size,
                    })
            result["zip_entries"] = count
            result["zip_total_uncompressed_bytes"] = total_size
            result["zip_first_entries"] = first_entries
            result["db_member_candidates"] = candidate_db_members[:MAX_LISTED_ENTRIES]
            result["db_member_candidates_truncated"] = (
                len(candidate_db_members) > MAX_LISTED_ENTRIES
            )
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
    try:
        result = inspect(args.archive)
    except (OSError, ValueError, zipfile.BadZipFile) as error:
        parser.error(str(error))
    print(json.dumps(result, ensure_ascii=False, indent=2))


if __name__ == "__main__":
    main()
