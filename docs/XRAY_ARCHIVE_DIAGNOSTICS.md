# X-Ray game archive diagnostics (read-only)

These scripts are **diagnostics**, not a working X-Ray archive extractor or iOS game engine.

## Run on your own computer or in a local Python environment

Python 3.11+; no third-party packages.

```sh
python tools/xray_archive_probe.py /path/to/gamedata.db0.zip
python tools/xray_archive_probe.py /path/to/gamedata.db0
python -m unittest discover -s tests -p 'test_xray_*.py' -v
```

The probe reads only a small prefix and suffix of a raw file. For ZIP
wrappers it reads ZIP metadata to report the first ten members and their
uncompressed sizes; it does **not** extract their contents. A large ZIP may
still take time to enumerate its central directory.

`tools/xray_chunk_scan.py` contains a scanner for a generic X-Ray-style
chunk stream (little-endian 32-bit chunk ID and payload length, high bit
indicating compression). It is **not yet established** that a particular
`gamedata.db0` archive is itself a chunk stream. Do not use a successful
chunk scan as evidence that an archive's file table is understood.

## Interpreting results

- `zip_container: true`: the supplied file is a ZIP wrapper. Identify its
  member names first; the actual `.db0` member may have another format.
- `zip_container: false`: the file is not recognized as a ZIP. This alone
  does not mean it is corrupt.
- `first_64_bytes_hex`: useful for comparing header bytes to a documented
  format; it is not a decoder.
- `sample_sha256`: hash of the **first 64 KiB only**, not the full file.
- `chunk_exceeds_file`: the proposed chunk layout does not fit the file;
  this can mean the format assumption is wrong, not necessarily corruption.

## Next technical gates

1. Obtain a lawful, known-good small `.db0` test sample and identify its
   exact game version and engine variant.
2. Confirm the archive header and directory/index layout against compatible
   X-Ray source code.
3. Implement a bounded file-table parser with offset, length, and filename
   validation; add malformed-input tests.
4. Only then implement the matching decompression algorithm and compare
   extracted bytes with known-good data.
5. Port the actual compatible X-Ray runtime to iOS; a successful archive
   parser alone cannot launch gameplay.

Do not upload original commercial game assets to a public GitHub repository.
