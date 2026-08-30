#!/usr/bin/env python3
"""Small cross-desktop INI configurator for the Proton package."""

from __future__ import annotations

import argparse
import configparser
import os
import shutil
import subprocess
from pathlib import Path


def boolean(value: str) -> str:
    return "1" if value.lower() in {"1", "yes", "true", "on"} else "0"


def write(path: Path, width: int, height: int, hud: str, bypass: str) -> None:
    ini = configparser.ConfigParser()
    ini.optionxform = str
    ini.read(path)
    if "Fix" not in ini:
        ini["Fix"] = {}
    ini["Fix"].update({"Enabled": "1", "Width": str(width), "Height": str(height),
                       "RemoveLetterboxing": "1", "CorrectFOV": "1",
                       "CenterHUD": boolean(hud)})
    if "Launcher" not in ini:
        ini["Launcher"] = {}
    ini["Launcher"].setdefault("Region", "eu")
    ini["Launcher"].setdefault("Language", "sp")
    ini["Launcher"].setdefault("SelfRegion", "EU")
    ini["Launcher"].setdefault("ControllerType", "XBOX")
    ini["Launcher"]["BypassUnityLauncher"] = boolean(bypass)
    temporary = path.with_suffix(".ini.tmp")
    with temporary.open("w", encoding="utf-8", newline="\n") as stream:
        ini.write(stream, space_around_delimiters=False)
    temporary.replace(path)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("ini", type=Path)
    parser.add_argument("--width", type=int, default=0)
    parser.add_argument("--height", type=int, default=0)
    parser.add_argument("--hud", default="yes")
    parser.add_argument("--bypass", default="no")
    args = parser.parse_args()
    if args.width < 0 or args.height < 0 or bool(args.width) != bool(args.height):
        parser.error("width and height must both be zero or positive values")
    write(args.ini, args.width, args.height, args.hud, args.bypass)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
