#!/usr/bin/env python3
"""End-to-end archive verifier checks with a fake xcrun (Linux-compatible)."""
import os
from pathlib import Path
import struct
import subprocess
import sys
import tempfile

root = Path(__file__).resolve().parents[1]
verifier = root / "scripts/verify_openxray_archive.py"
builder = root / "scripts/build_with_openxray.sh"
link_flags = root / "scripts/openxray_system_link_flags.py"
legacy = "_st_xray_embedded_run"
resources = "_st_xray_embedded_run_with_resources"

with tempfile.TemporaryDirectory(prefix="openxray-verifier-") as tmp:
    folder = Path(tmp)
    archive = folder / "libOpenXRayProbe.a"
    archive.write_bytes(b"mock-archive")
    xcrun = folder / "xcrun"
    xcrun.write_text("""#!/usr/bin/env python3
import os
import sys
if sys.argv[1:3] == ['lipo', '-verify_arch']:
    sys.exit(0)
if sys.argv[1:3] == ['nm', '-gU']:
    print(os.environ['MOCK_NM'])
    sys.exit(0)
sys.exit(9)
""")
    xcrun.chmod(0o755)
    env = os.environ.copy()
    env["PATH"] = str(folder) + os.pathsep + env.get("PATH", "")

    def verify(nm, strict=False):
        e = env.copy()
        e["MOCK_NM"] = nm
        args = [sys.executable, str(verifier)]
        if strict:
            args.append("--require-resources")
        args.append(str(archive))
        return subprocess.run(args, env=e, text=True, capture_output=True)

    only_legacy = f"0000000000000100 T {legacy}"
    both = only_legacy + f"\n0000000000000200 T {resources}"
    assert verify(only_legacy).returncode != 0
    rejected = verify(only_legacy, strict=True)
    assert rejected.returncode != 0
    assert "Missing st_xray_embedded_run_with_resources" in rejected.stderr
    assert verify(both).returncode == 0
    assert verify(both, strict=True).returncode == 0
    # An unresolved legacy symbol is harmless when the resource-aware entry is defined.
    assert verify(f"                 U {legacy}\n0000000000000200 T {resources}", strict=True).returncode == 0
    assert verify(f"{only_legacy}\n                 U {resources}", strict=True).returncode != 0
    # Resource-only engines are supported; legacy entry is optional.
    assert verify(f"0000000000000200 T {resources}", strict=True).returncode == 0

    # The executable taken from a working IPA must be rejected before Xcode.
    executable = folder / "StalkerMobile"
    for endian in ("<", ">"):
        executable.write_bytes(struct.pack(endian + "IIII", 0xFEEDFACF, 0x0100000C, 0, 2))
        build = subprocess.run(["bash", str(builder), str(executable)],
                               cwd=root, capture_output=True, text=True)
        assert build.returncode != 0 and "MH_EXECUTE" in build.stderr, build.stderr

    sdk = folder / "iPhoneOS.sdk"
    arguments = folder / "link-arguments.txt"
    arguments.write_text("\n".join([
        "-framework", "UIKit", "-Wl,-framework,UIKit",
        "-Xlinker", "-weak_framework", "-Xlinker", "GameController",
        str(sdk / "System/Library/Frameworks/OpenAL.framework/OpenAL.tbd"),
        str(sdk / "usr/lib/libz.tbd"), "-lz",
        str(folder / "libExternal.a"), "-isysroot", str(sdk),
    ]))
    actual = subprocess.check_output(
        [sys.executable, str(link_flags), str(arguments), str(sdk)], text=True,
    )
    assert actual.split() == ["-framework", "UIKit", "-weak_framework", "GameController",
                              "-framework", "OpenAL", "-lz"], actual
print("OpenXRay archive CLI integration checks passed")
