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
print("OpenXRay archive symbol parser regression tests passed")
