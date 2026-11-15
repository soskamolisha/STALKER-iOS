"""Synthetic tests for X-Ray PE inventory."""
import importlib.util
from pathlib import Path
import struct
import tempfile
import unittest
import zipfile

SCRIPT = Path(__file__).resolve().parents[1] / "tools" / "xray_pe_inventory.py"
spec = importlib.util.spec_from_file_location("xray_pe_inventory", SCRIPT)
mod = importlib.util.module_from_spec(spec)
spec.loader.exec_module(mod)


def fake_pe(machine=0x14c, dll=True):
    data = bytearray(256)
    data[:2] = b"MZ"
    struct.pack_into("<I", data, 0x3c, 128)
    data[128:132] = b"PE\0\0"
    struct.pack_into("<HHIIIHH", data, 132, machine, 3, 0, 0, 0, 224,
                     0x2000 if dll else 0)
    return bytes(data)


class PEInventoryTests(unittest.TestCase):
    def test_x86_dll(self):
        self.assertEqual(mod.parse_pe_header(fake_pe())["architecture"], "x86")
        self.assertTrue(mod.parse_pe_header(fake_pe())["dll"])

    def test_arm64_executable(self):
        info = mod.parse_pe_header(fake_pe(0xaa64, False))
        self.assertEqual(info["architecture"], "arm64")
        self.assertFalse(info["dll"])

    def test_bad_mz(self):
        self.assertFalse(mod.parse_pe_header(b"not PE")["pe"])

    def test_out_of_range_offset(self):
        data = bytearray(fake_pe())
        struct.pack_into("<I", data, 0x3c, 5000)
        self.assertEqual(mod.parse_pe_header(data)["reason"],
                         "pe_header_outside_prefix")

    def test_zip_inventory_no_extraction(self):
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder) / "bin.zip"
            with zipfile.ZipFile(path, "w") as archive:
                archive.writestr("bin/xrCore.dll", fake_pe())
                archive.writestr("__MACOSX/._xrCore.dll", b"junk")
                archive.writestr("bin/readme.txt", "ignored")
            info = mod.inventory(path)
            self.assertEqual(info["count"], 1)
            self.assertEqual(info["files"][0]["name"], "bin/xrCore.dll")
            self.assertFalse((Path(folder) / "bin").exists())


if __name__ == "__main__":
    unittest.main()
