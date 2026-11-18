"""Synthetic tests for X-Ray chunk boundaries; no proprietary game files."""
import importlib.util
from pathlib import Path
import struct
import tempfile
import unittest

MODULE_PATH = Path(__file__).resolve().parents[1] / "tools" / "xray_chunk_scan.py"
spec = importlib.util.spec_from_file_location("xray_chunk_scan", MODULE_PATH)
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)


class ChunkScanTests(unittest.TestCase):
    def scan(self, data, **kwargs):
        with tempfile.TemporaryDirectory() as d:
            path = Path(d) / "test.db0"
            path.write_bytes(data)
            return module.scan_chunks(path, **kwargs)

    def test_two_chunks(self):
        raw = struct.pack("<II", 1, 3) + b"abc"
        raw += struct.pack("<II", 0x80000002, 2) + b"xy"
        result = self.scan(raw)
        self.assertTrue(result["valid"])
        self.assertEqual([c["id"] for c in result["chunks"]], [1, 2])
        self.assertTrue(result["chunks"][1]["compressed"])
        self.assertEqual(result["bytes_scanned"], len(raw))

    def test_truncated_header(self):
        result = self.scan(b"abc")
        self.assertEqual(result["reason"], "truncated_header")

    def test_out_of_bounds_length(self):
        result = self.scan(struct.pack("<II", 1, 999))
        self.assertEqual(result["reason"], "chunk_exceeds_file")

    def test_chunk_limit(self):
        raw = struct.pack("<II", 1, 0) * 2
        result = self.scan(raw, max_chunks=1)
        self.assertEqual(result["reason"], "chunk_limit")

    def test_empty(self):
        result = self.scan(b"")
        self.assertTrue(result["valid"])
        self.assertEqual(result["chunks"], [])


if __name__ == "__main__":
    unittest.main()
