#!/usr/bin/env python3
"""Inspect a user-owned X-Ray .db archive without copying game assets into Git."""
import argparse
import os
import struct
import zipfile
import sys

def inspect(path):
    archive_zip = zipfile.ZipFile(path) if path.lower().endswith(".zip") else None
    if archive_zip:
        members = [i for i in archive_zip.infolist() if os.path.basename(i.filename).lower().startswith("gamedata.db") and not i.filename.startswith("__MACOSX/")]
        if len(members) != 1:
            print("ERROR: expected exactly one gamedata.db archive in ZIP")
            return 1
        size = members[0].file_size
        stream_context = archive_zip.open(members[0])
    else:
        size = os.path.getsize(path)
        stream_context = open(path, "rb")
    print(f"archive={os.path.basename(path)} bytes={size}")
    with stream_context as stream:
        offset = 0
        for index in range(4096):
            if offset == size:
                return 0
            if size - offset < 8:
                print(f"ERROR: truncated chunk header at {offset}")
                return 1
            stream.seek(offset)
            chunk_id, encoded_size = struct.unpack("<II", stream.read(8))
            compressed = bool(chunk_id & 0x80000000)
            payload_size = encoded_size
            end = offset + 8 + payload_size
            if end > size:
                print(f"ERROR: chunk {index} extends beyond EOF")
                return 1
            print(f"chunk={index} id={chunk_id & 0x7fffffff} compressed={compressed} offset={offset} payload_offset={offset+8} payload_size={payload_size}")
            if (chunk_id & 0x7fffffff) == 1:
                stream.seek(offset + 8)
                print(f"directory_prefix={stream.read(min(16,payload_size)).hex()}")
            offset = end
    print("ERROR: too many chunks")
    return 1

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("archive", help="Path to locally owned gamedata.db0 or gamedata.db0.zip")
    args = parser.parse_args()
    sys.exit(inspect(args.archive))
