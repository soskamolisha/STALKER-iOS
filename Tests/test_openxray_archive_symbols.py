#!/usr/bin/env python3
"""Portable checks for Mach-O nm symbol parsing (no Apple SDK required)."""
import importlib.util
from pathlib import Path

module_path = Path(__file__).resolve().parents[1] / "scripts/verify_openxray_archive.py"
spec = importlib.util.spec_from_file_location("archive_verifier", module_path)
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)

symbol = "_st_xray_embedded_run_with_resources"
assert module.has_defined_export(f"0000000100000000 T {symbol}", symbol)
assert module.has_defined_export(f"0000000000000000 W {symbol}", symbol)
assert not module.has_defined_export(f"                 U {symbol}", symbol)
assert not module.has_defined_export(f"                 w {symbol}", symbol)
assert not module.has_defined_export(f"archive(member.o):\\n", symbol)
assert not module.has_defined_export(f"0000000100000000 T {symbol}_extra", symbol)

# Validate the full CLI contract without requiring macOS or Xcode.
from unittest.mock import patch
from tempfile import TemporaryDirectory
with TemporaryDirectory() as temporary:
    archive = Path(temporary) / "engine.a"
    archive.write_bytes(b"mock arm64 archive")
    import sys
    from contextlib import redirect_stderr
    from io import StringIO
    with patch.object(module, "run", return_value="00000000 T _st_xray_embedded_run\\n"):
        with patch.object(sys, "argv", ["verify_openxray_archive.py", str(archive)]):
            with redirect_stderr(StringIO()) as errors:
                assert module.main() == 1
            assert "st_xray_embedded_run_with_resources" in errors.getvalue()
    both = "00000000 T _st_xray_embedded_run\\n00000000 T _st_xray_embedded_run_with_resources\\n"
    with patch.object(module, "run", return_value=both):
        with patch.object(sys, "argv", ["verify_openxray_archive.py", str(archive)]):
            assert module.main() == 0

print("OpenXRay archive symbol parser regression tests passed")
