"""Unit tests for read-only archive probe, using synthetic data only."""
import importlib.util
from pathlib import Path
import tempfile
import unittest
import zipfile

PROBE = Path(__file__).resolve().parents[1] / "tools" / "xray_archive_probe.py"
spec = importlib.util.spec_from_file_location("xray_archive_probe", PROBE)
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)


class ArchiveProbeTests(unittest.TestCase):
    def test_non_zip_binary(self):
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder) / "gamedata.db0"
            path.write_bytes(bytes([1, 2, 3, 4]) + b"test-data")
            info = module.inspect(path)
            self.assertFalse(info["zip_container"])
            self.assertEqual(info["size_bytes"], 13)
            self.assertEqual(info["first_u32_le"], 0x04030201)
            self.assertIn("note", info)

    def test_zip_wrapper(self):
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder) / "gamedata.db0.zip"
            with zipfile.ZipFile(path, "w", zipfile.ZIP_DEFLATED) as archive:
                archive.writestr("gamedata.db0", b"fake-data")
            info = module.inspect(path)
            self.assertTrue(info["zip_container"])
            self.assertEqual(info["zip_entries"], 1)
            self.assertEqual(info["zip_first_entries"][0]["name"], "gamedata.db0")
            self.assertEqual(info["zip_total_uncompressed_bytes"], 9)
            self.assertEqual(info["db_member_candidates"][0]["name"], "gamedata.db0")

    def test_no_db_members(self):
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder) / "sample.zip"
            with zipfile.ZipFile(path, "w") as archive:
                archive.writestr("readme.txt", "hello")
            info = module.inspect(path)
            self.assertEqual(info["db_member_candidates"], [])

    def test_empty_file(self):
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder) / "empty.db0"
            path.write_bytes(b"")
            info = module.inspect(path)
            self.assertEqual(info["size_bytes"], 0)
            self.assertNotIn("first_u32_le", info)

    def test_invalid_sample_size(self):
        with self.assertRaises(ValueError):
            module.inspect(Path("unused.db0"), sample_size=0)

    def test_missing_file(self):
        with self.assertRaises(FileNotFoundError):
            module.inspect(Path("/not-a-real-folder-12345/missing.db0"))


if __name__ == "__main__":
    unittest.main()
