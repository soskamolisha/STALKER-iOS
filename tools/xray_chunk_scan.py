"""Conservative, read-only X-Ray chunk stream scanner.

X-Ray chunk streams use little-endian (id, size) headers. The high bit of
the id marks compressed chunks. This module only identifies boundaries;
it does not decompress, decrypt, or interpret file tables.
"""
from pathlib import Path
import struct

HEADER = struct.Struct("<II")
COMPRESSED_FLAG = 0x80000000


def scan_chunks(path: Path, max_chunks: int = 10000) -> dict:
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
