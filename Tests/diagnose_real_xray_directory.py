#!/usr/bin/env python3
"""Local diagnostic for a user-owned X-Ray .db archive; never uploads game data."""
import argparse
import ctypes
import ctypes.util
import struct

def decrypt(payload, rounds, table_seed, stream_seed):
    def random(seed):
        seed = (0x08088405 * seed + 1) & 0xffffffff
        return seed, (seed * 256) >> 32
    alphabet = list(range(256))
    seed = table_seed
    for _ in range(rounds):
        seed, j = random(seed)
        seed, k = random(seed)
        while j == k:
            seed, k = random(seed)
        alphabet[j], alphabet[k] = alphabet[k], alphabet[j]
    reverse = [0] * 256
    for i, value in enumerate(alphabet):
        reverse[value] = i
    seed = stream_seed
    result = bytearray(len(payload))
    for i, value in enumerate(payload):
        seed, key = random(seed)
        result[i] = reverse[value ^ key]
    return result

def run(path):
    with open(path, "rb") as stream:
        header = stream.read(8)
        if len(header) != 8:
            raise ValueError("missing first chunk header")
        chunk_id, length = struct.unpack("<II", header)
        if chunk_id != 0:
            raise ValueError("expected first data chunk")
        stream.seek(length, 1)
        directory_header = stream.read(8)
        if len(directory_header) != 8:
            raise ValueError("missing directory header")
        directory_id, directory_length = struct.unpack("<II", directory_header)
        if directory_id != 0x80000001 or directory_length > 64 * 1024 * 1024:
            raise ValueError("unexpected directory chunk")
        payload = stream.read(directory_length)
        if len(payload) != directory_length:
            raise ValueError("truncated directory")
    print("directory compressed bytes:", directory_length)
    libname = ctypes.util.find_library("lzo2")
    if not libname:
        print("liblzo2 unavailable: showing decrypted prefixes only")
        return
    lib = ctypes.CDLL(libname)
    decode = lib.lzo1x_decompress_safe
    decode.argtypes = [ctypes.c_void_p, ctypes.c_ulong, ctypes.c_void_p,
                       ctypes.POINTER(ctypes.c_ulong), ctypes.c_void_p]
    decode.restype = ctypes.c_int
    for name, variant in (("none", None),
                          ("worldwide", (1024, 6011979, 24031979)),
                          ("russian", (2048, 20091958, 20031955))):
        data = payload if variant is None else decrypt(payload, *variant)
        print(name, "prefix:", bytes(data[:16]).hex())
        for skip in (0, 4):
            source = ctypes.create_string_buffer(bytes(data[skip:]))
            output = ctypes.create_string_buffer(8 * 1024 * 1024)
            count = ctypes.c_ulong(len(output))
            code = decode(source, len(data) - skip, output, ctypes.byref(count), None)
            print(" ", "skip:", skip, "LZO status:", code, "decoded:", count.value)
            if code == 0:
                print("  decoded prefix:", output.raw[:min(count.value, 16)].hex())

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("archive", help="Locally owned, unpacked gamedata.db0")
    run(parser.parse_args().archive)
