"""Additional boundary tests for read-only X-Ray chunk scanning."""
import importlib.util
from pathlib import Path
import struct
import tempfile
import unittest

MODULE_PATH = Path(__file__).resolve().parents[1] / "tools" / "xray_chunk_scan.py"
spec = importlib.util.spec_from_file_location("xray_chunk_scan_limits", MODULE_PATH)
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)


class ChunkScanLimitTests(unittest.TestCase):
    def scan(self, data, **kwargs):
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder) / "candidate.bin"
            path.write_bytes(data)
            return module.scan_chunks(path, **kwargs)

    def test_zero_max_chunks_rejected(self):
        with self.assertRaises(ValueError):
            self.scan(b"", max_chunks=0)

    def test_negative_max_chunks_rejected(self):
        with self.assertRaises(ValueError):
            self.scan(b"", max_chunks=-1)

    def test_exact_boundary(self):
        data = struct.pack("<II", 42, 4) + b"abcd"
        result = self.scan(data)
        self.assertTrue(result["valid"])
        self.assertEqual(result["bytes_scanned"], len(data))

    def test_zero_length_chunk(self):
        result = self.scan(struct.pack("<II", 7, 0))
        self.assertTrue(result["valid"])
        self.assertEqual(result["chunks"][0]["payload_size"], 0)

    def test_large_claimed_payload_rejected(self):
        result = self.scan(struct.pack("<II", 9, 0xffffffff))
        self.assertFalse(result["valid"])
        self.assertEqual(result["reason"], "chunk_exceeds_file")


if __name__ == "__main__":
    unittest.main()
