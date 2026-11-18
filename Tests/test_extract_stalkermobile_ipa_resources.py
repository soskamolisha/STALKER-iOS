#!/usr/bin/env python3
"""Regression tests for extraction of user-owned StalkerMobile IPA resources."""
import importlib.util
from pathlib import Path
import tempfile
import unittest
import zipfile

SCRIPT = Path(__file__).resolve().parents[1] / "scripts" / "extract_stalkermobile_ipa_resources.py"
spec = importlib.util.spec_from_file_location("stalker_ipa_extract", SCRIPT)
extractor = importlib.util.module_from_spec(spec)
spec.loader.exec_module(extractor)


class ExtractIPAResourcesTests(unittest.TestCase):
    def test_imports_data_and_configs_but_not_executable(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            ipa = root / "port.ipa"
            with zipfile.ZipFile(ipa, "w") as archive:
                archive.writestr("Payload/StalkerMobile.app/StalkerMobile", b"executable")
                archive.writestr("Payload/StalkerMobile.app/Info.plist", b"plist")
                archive.writestr("Payload/StalkerMobile.app/fsgame.ltx", b"config")
                archive.writestr("Payload/StalkerMobile.app/ios_args.txt", b"-soc")
                archive.writestr("Payload/StalkerMobile.app/Data/gamedata.db0", b"archive")
                archive.writestr("Payload/StalkerMobile.app/Data/gamedata/config/test.ltx", b"loose")
            dest = root / "import"
            extractor.extract(ipa, dest)
            self.assertEqual((dest / "gamedata.db0").read_bytes(), b"archive")
            self.assertEqual((dest / "gamedata/config/test.ltx").read_bytes(), b"loose")
            self.assertEqual((dest / "fsgame.ltx").read_bytes(), b"config")
            self.assertEqual((dest / "ios_args.txt").read_bytes(), b"-soc")
            self.assertFalse((dest / "StalkerMobile").exists())
            self.assertFalse((dest / "Info.plist").exists())

    def test_rejects_path_traversal(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            ipa = root / "bad.ipa"
            with zipfile.ZipFile(ipa, "w") as archive:
                archive.writestr("Payload/StalkerMobile.app/Data/../../escape", b"bad")
            with self.assertRaises(ValueError):
                extractor.extract(ipa, root / "import")
            self.assertFalse((root / "escape").exists())

    def test_does_not_overwrite_existing_import(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            ipa = root / "port.ipa"
            with zipfile.ZipFile(ipa, "w") as archive:
                archive.writestr("Payload/StalkerMobile.app/Data/gamedata.db0", b"new")
            dest = root / "import"
            dest.mkdir()
            (dest / "gamedata.db0").write_bytes(b"old")
            with self.assertRaises(ValueError):
                extractor.extract(ipa, dest)
            self.assertEqual((dest / "gamedata.db0").read_bytes(), b"old")


if __name__ == "__main__":
    unittest.main()
