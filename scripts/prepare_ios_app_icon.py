#!/usr/bin/env python3
"""Turn the existing STALKER artwork into an iOS primary icon set."""
import argparse
import json
from pathlib import Path
import shutil


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("catalog", nargs="?", type=Path, default=Path("Sources/GeneratedAppIcons.xcassets"))
    args = parser.parse_args()
    source = Path(__file__).resolve().parents[1] / "Resources" / "AppIcon.png"
    target = args.catalog / "AppIcon.appiconset"
    target.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(source, target / "AppIcon.png")
    contents = {
        "images": [{"filename": "AppIcon.png", "idiom": "universal", "platform": "ios", "size": "1024x1024"}],
        "info": {"author": "xcode", "version": 1},
    }
    (target / "Contents.json").write_text(json.dumps(contents, indent=2) + "\n")


if __name__ == "__main__":
    main()
