"""Synthetic tests for ZIP-contained X-Ray DB prefix inspection."""
import importlib.util
from pathlib import Path
import tempfile
import unittest
import zipfile

SCRIPT = Path(__file__).resolve().parents[1] / "tools" / "xray_zip_db_peek.py"
spec = importlib.util.spec_from_file_location("xray_zip_db_peek", SCRIPT)
mod = importlib.util.module_from_spec(spec)
spec.loader.exec_module(mod)


class ZipDbPeekTests(unittest.TestCase):
    def test_peek_db_header_without_extraction(self):
        with tempfile.TemporaryDirectory() as directory:
            folder = Path(directory)
            path = folder / "sample.zip"
            with zipfile.ZipFile(path, "w", zipfile.ZIP_DEFLATED) as archive:
                archive.writestr("assets/gamedata.db0", bytes([1, 2, 3, 4]) + b"rest")
                archive.writestr("readme.txt", "ignored")
            result = mod.peek_db_members(path, prefix_bytes=4)
            self.assertEqual(len(result), 1)
            self.assertEqual(result[0]["prefix_hex"], "01020304")
            self.assertEqual(result[0]["first_u32_le"], 0x04030201)
            self.assertFalse((folder / "assets").exists())

    def test_invalid_limit(self):
        with self.assertRaises(ValueError):
            mod.peek_db_members(Path("unused.zip"), prefix_bytes=10000)

    def test_no_db_members(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "sample.zip"
            with zipfile.ZipFile(path, "w") as archive:
                archive.writestr("readme.txt", "text")
            self.assertEqual(mod.peek_db_members(path), [])


if __name__ == "__main__":
    unittest.main()
