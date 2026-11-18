"""Read-only scanner for a *candidate* X-Ray chunk stream.

This is not a .db0 file-table parser. A rejected candidate does not imply
that the original archive is corrupt.
"""
import argparse
import json
from pathlib import Path
import struct

HEADER = struct.Struct("<II")
COMPRESSED_FLAG = 0x80000000


def scan_chunks(path: Path, max_chunks: int = 10000) -> dict:
    if max_chunks < 1:
        raise ValueError("max_chunks must be positive")
    size = path.stat().st_size
    chunks = []
    offset = 0
    with path.open("rb") as stream:
        while offset < size and len(chunks) < max_chunks:
            remaining = size - offset
            if remaining < HEADER.size:
                return {"valid": False, "reason": "truncated_header",
                        "offset": offset, "chunks": chunks}
            stream.seek(offset)
            raw = stream.read(HEADER.size)
            if len(raw) != HEADER.size:
                return {"valid": False, "reason": "short_read",
                        "offset": offset, "chunks": chunks}
            chunk_id, length = HEADER.unpack(raw)
            end = offset + HEADER.size + length
            if end > size:
                return {"valid": False, "reason": "chunk_exceeds_file",
                        "offset": offset, "chunks": chunks}
            chunks.append({
                "offset": offset,
                "id": chunk_id & ~COMPRESSED_FLAG,
                "compressed": bool(chunk_id & COMPRESSED_FLAG),
                "payload_size": length,
            })
            offset = end
    if offset < size:
        return {"valid": False, "reason": "chunk_limit",
                "offset": offset, "chunks": chunks}
    return {"valid": True, "chunks": chunks, "bytes_scanned": offset}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("archive", type=Path)
    parser.add_argument("--max-chunks", type=int, default=10000)
    parser.add_argument("--show", type=int, default=20,
                        help="maximum chunk records in output")
    args = parser.parse_args()
    if args.show < 0:
        parser.error("--show must be nonnegative")
    try:
        result = scan_chunks(args.archive, args.max_chunks)
    except (OSError, ValueError) as error:
        parser.error(str(error))
    total = len(result["chunks"])
    result["chunks"] = result["chunks"][:args.show]
    result["chunks_total"] = total
    result["chunks_omitted"] = total - len(result["chunks"])
    result["warning"] = "Candidate chunk stream only; not a verified X-Ray .db0 parser"
    print(json.dumps(result, indent=2))


if __name__ == "__main__":
    main()
