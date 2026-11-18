#!/usr/bin/env python3
"""Recover Apple system link dependencies from the X-Ray iPhoneOS target."""
import argparse
from pathlib import Path
import re


def system_link_flags(arguments, sdk):
    # CMake emits both compiler-driver arguments and forwarded linker options.
    tokens = []
    for arg in arguments:
        if arg.startswith("-Wl,"):
            tokens.extend(arg.split(",")[1:])
        elif arg != "-Xlinker":
            tokens.append(arg)

    framework_options = {"-framework", "-weak_framework", "-needed_framework"}
    flags = {}
    tokens = iter(tokens)
    for token in tokens:
        if token in framework_options:
            name = next(tokens, "")
            if not re.fullmatch(r"[A-Za-z0-9_]+", name):
                raise ValueError(f"Invalid framework dependency: {name!r}")
            option = (f"-Wl,{token},{name}",) if token == "-needed_framework" else (token, name)
            flags[option] = None
        elif re.fullmatch(r"-l[A-Za-z0-9_.+-]+", token):
            flags[(token,)] = None
        elif token.startswith("/"):
            path = Path(token)
            if not path.is_relative_to(sdk):
                continue
            framework = next((part for part in path.parts if part.endswith(".framework")), None)
            if framework:
                flags[("-framework", Path(framework).stem)] = None
            elif path.parent == sdk / "usr/lib" and path.suffix in {".tbd", ".dylib"}:
                if path.stem.startswith("lib"):
                    flags[("-l" + path.stem.removeprefix("lib"),)] = None
    return [flag for dependency in flags for flag in dependency]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("arguments", type=Path, help="resolved compiler arguments, one per line")
    parser.add_argument("sdk", type=Path, help="absolute iPhoneOS SDK path")
    args = parser.parse_args()
    try:
        print(" ".join(system_link_flags(args.arguments.read_text().splitlines(), args.sdk)))
    except ValueError as error:
        parser.error(str(error))


if __name__ == "__main__":
    main()
