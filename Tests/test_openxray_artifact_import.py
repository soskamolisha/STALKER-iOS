#!/usr/bin/env python3
"""Exercise artifact ZIP import with a fake Apple toolchain on Linux."""
import os
import stat
from pathlib import Path
import subprocess
import sys
import tempfile
import zipfile

root = Path(__file__).resolve().parents[1]
importer = root / "scripts/integrate_openxray_artifact.py"

with tempfile.TemporaryDirectory(prefix="openxray-import-test-") as tmp:
    work = Path(tmp)
    tool = work / "xcrun"
    tool.write_text("""#!/usr/bin/env python3
import os
import sys
if sys.argv[1:3] == ['lipo', '-verify_arch']:
    sys.exit(0)
if sys.argv[1:3] == ['nm', '-gU']:
    print(os.environ['MOCK_NM'])
    sys.exit(0)
sys.exit(9)
""")
    tool.chmod(0o755)
    artifact = work / "engine.zip"
    output = work / "output"
    env = os.environ.copy()
    env["PATH"] = str(work) + os.pathsep + env.get("PATH", "")

    def run(symbols, entries):
        env["MOCK_NM"] = symbols
        with zipfile.ZipFile(artifact, "w") as zip_file:
            for name, data in entries:
                zip_file.writestr(name, data)
        return subprocess.run(
            [sys.executable, str(importer), str(artifact), "--output", str(output)],
            env=env, capture_output=True, text=True
        )

    legacy = "0000000000000100 T _st_xray_embedded_run"
    resource = "0000000000000200 T _st_xray_embedded_run_with_resources"
    entries = [("libOpenXRayCombinedProbe.a", b"mock-engine")]
    rejected = run(legacy, entries)
    assert rejected.returncode != 0, rejected.stdout + rejected.stderr
    assert "Missing st_xray_embedded_run_with_resources" in rejected.stderr
    assert not (output / "libOpenXRayCombinedProbe.a").exists()

    accepted = run(legacy + "\n" + resource, entries)
    assert accepted.returncode == 0, accepted.stdout + accepted.stderr
    assert (output / "libOpenXRayCombinedProbe.a").read_bytes() == b"mock-engine"

    # A ZIP member marked as a Unix symlink must not be imported as a library.
    link = zipfile.ZipInfo("libOpenXRayCombinedProbe.a")
    link.create_system = 3
    link.external_attr = (stat.S_IFLNK | 0o777) << 16
    rejected = run(legacy + "\n" + resource, [(link, b"/tmp/other.a")])
    assert rejected.returncode != 0, rejected.stdout + rejected.stderr
    assert "symbolic link" in rejected.stderr

    rejected = run(legacy + "\n" + resource, entries + [("extra.a", b"other")])
    assert rejected.returncode != 0
    assert "expected exactly one static .a archive" in rejected.stderr

print("OpenXRay artifact ZIP import integration tests passed")
