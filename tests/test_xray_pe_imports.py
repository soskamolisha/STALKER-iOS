"""Synthetic PE import parser tests; no commercial binaries included."""
import importlib.util
from pathlib import Path
import struct
import unittest

PATH = Path(__file__).resolve().parents[1] / "tools" / "xray_pe_imports.py"
spec = importlib.util.spec_from_file_location("xray_pe_imports", PATH)
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)


def pe_with_imports(name=b"kernel32.dll", import_size=40):
    data = bytearray(1024)
    data[:2] = b"MZ"
    struct.pack_into("<I", data, 0x3c, 0x80)
    data[0x80:0x84] = b"PE\0\0"
    struct.pack_into("<HHIIIHH", data, 0x84, 0x14c, 1, 0, 0, 0, 224, 0)
    opt = 0x98
    struct.pack_into("<H", data, opt, 0x10b)
    struct.pack_into("<II", data, opt + 96 + 8, 0x1000, import_size)
    section = opt + 224
    struct.pack_into("<IIII", data, section + 8, 0x200, 0x1000, 0x200, 0x200)
    struct.pack_into("<IIIII", data, 0x200, 0, 0, 0, 0x1050, 0)
    data[0x250:0x250 + len(name) + 1] = name + b"\0"
    return bytes(data)


class PEImportTests(unittest.TestCase):
    def test_import_name(self):
        result = module.imports_from_pe(pe_with_imports())
        self.assertEqual(result["imports"], ["kernel32.dll"])
        self.assertNotIn("error", result)

    def test_non_pe(self):
        self.assertEqual(module.imports_from_pe(b"invalid")["error"], "not_mz")

    def test_import_directory_too_small(self):
        result = module.imports_from_pe(pe_with_imports(import_size=4))
        self.assertEqual(result["error"], "invalid_import_directory_size")

    def test_unterminated_name(self):
        data = bytearray(pe_with_imports())
        data[0x250:0x400] = b"A" * (0x400 - 0x250)
        result = module.imports_from_pe(bytes(data))
        self.assertEqual(result["error"], "unterminated_import_name")


if __name__ == "__main__":
    unittest.main()
