#!/usr/bin/env python3
"""Package an unsigned device prototype; this does not sign or port a game."""
import argparse
import os
from pathlib import Path
import plistlib
import zipfile

def package(app, output):
    app, output = Path(app).resolve(), Path(output).resolve()
    if app.suffix != ".app" or not app.is_dir():
        raise ValueError("Expected an existing .app directory")
    with (app / "Info.plist").open("rb") as f:
        info = plistlib.load(f)
    if info.get("CFBundleSupportedPlatforms") != ["iPhoneOS"]:
        raise ValueError("Only a physical-device build can be packaged")
    executable = info.get("CFBundleExecutable", "")
    if not executable or Path(executable).name != executable:
        raise ValueError("Invalid bundle executable name")
    if not (app / executable).is_file():
        raise ValueError("Missing bundle executable")
    if (app / "embedded.mobileprovision").exists() or (app / "_CodeSignature").exists():
        raise ValueError("This packager expects an unsigned app")
    if app == output or app in output.parents:
        raise ValueError("Output must be outside the app")
    files = sorted(app.rglob("*"))
    if any(p.is_symlink() for p in files):
        raise ValueError("Symlinks are unsupported in this prototype bundle")
    output.parent.mkdir(parents=True, exist_ok=True)
    temporary = output.with_suffix(output.suffix + ".tmp")
    try:
        with zipfile.ZipFile(temporary, "w", zipfile.ZIP_DEFLATED) as z:
            for p in files:
                if p.is_file():
                    z.write(p, str(Path("Payload") / app.name / p.relative_to(app)))
        with zipfile.ZipFile(temporary) as z:
            if z.testzip() is not None:
                raise ValueError("Archive integrity check failed")
        os.replace(temporary, output)
    finally:
        temporary.unlink(missing_ok=True)

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("app")
    parser.add_argument("output")
    args = parser.parse_args()
    package(args.app, args.output)
    print("Unsigned prototype IPA created; signing and provisioning still required.")
