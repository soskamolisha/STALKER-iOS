import importlib.util
from pathlib import Path
import unittest

spec = importlib.util.spec_from_file_location("adapt_fsgame_ios", Path(__file__).resolve().parents[1] / "scripts" / "adapt_fsgame_ios.py")
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)

class AdaptFsgameTests(unittest.TestCase):
    def test_preserves_game_aliases_and_replaces_windows_path(self):
        source = "$app_data_root$ = true| false| C:\\Users\\Someone\\Documents\\STALKER\r\n$game_data$ = false| true| $fs_root$| gamedata\\\r\n"
        output = module.adapt(source)
        self.assertIn("$app_data_root$ = true| false| $fs_root$| appdata\\\r\n", output)
        self.assertIn("$game_data$ = false| true| $fs_root$| gamedata\\", output)
        self.assertNotIn("C:\\Users", output)

    def test_rejects_missing_or_duplicate_roots(self):
        with self.assertRaises(ValueError):
            module.adapt("$game_data$ = true| true| root\n")
        with self.assertRaises(ValueError):
            module.adapt("$app_data_root$ = true| false| a\n$app_data_root$ = true| false| b\n")

if __name__ == "__main__":
    unittest.main()
