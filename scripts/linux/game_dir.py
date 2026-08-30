#!/usr/bin/env python3
"""Find Peace Walker's mgspw directory in Steam libraries."""

from __future__ import annotations

import argparse
import os
import re
from pathlib import Path

APP_ID = "2492660"
EXE = "METAL GEAR SOLID PEACE WALKER.exe"


def valid(value: str | Path) -> Path | None:
    path = Path(value).expanduser()
    if path.is_file() and path.name.lower() == EXE.lower():
        path = path.parent
    if (path / "mgspw" / EXE).is_file():
        path = path / "mgspw"
    return path.resolve() if (path / EXE).is_file() else None


def steam_roots() -> list[Path]:
    home = Path.home()
    values = [
        Path(os.environ[name]).expanduser()
        for name in ("STEAM_DIR", "STEAM_ROOT")
        if os.environ.get(name)
    ]
    values += [
        home / ".local/share/Steam",
        home / ".steam/steam",
        home / ".steam/root",
        home / ".var/app/com.valvesoftware.Steam/.local/share/Steam",
        home / ".var/app/com.valvesoftware.Steam/data/Steam",
    ]
    return list(dict.fromkeys(path.resolve() for path in values))


def libraries(root: Path) -> list[Path]:
    result = [root]
    try:
        text = (root / "steamapps/libraryfolders.vdf").read_text(errors="replace")
    except OSError:
        return result
    for raw in re.findall(r'"path"\s+"((?:\\.|[^"\\])*)"', text):
        result.append(Path(raw.replace(r"\\", "\\")).expanduser())
    return result


def discover() -> list[Path]:
    result: list[Path] = []
    for root in steam_roots():
        for library in libraries(root):
            manifest = library / f"steamapps/appmanifest_{APP_ID}.acf"
            install = "MGS_PW"
            try:
                match = re.search(
                    r'"installdir"\s+"([^"]+)"', manifest.read_text(errors="replace")
                )
                if match:
                    install = match.group(1)
            except OSError:
                pass
            candidate = valid(library / "steamapps/common" / install / "mgspw")
            if candidate and candidate not in result:
                result.append(candidate)
    return result


def resolve() -> Path:
    override = os.environ.get("PW_GAME_DIR")
    if override:
        candidate = valid(override)
        if candidate:
            return candidate
        raise SystemExit(f"PW_GAME_DIR does not contain {EXE}: {override}")
    candidate = valid(Path.cwd())
    if candidate:
        return candidate
    found = discover()
    if len(found) == 1:
        return found[0]
    if os.isatty(0):
        print(f"Enter the mgspw folder containing {EXE}:")
        candidate = valid(input().strip())
        if candidate:
            return candidate
    raise SystemExit(
        "Could not locate Peace Walker. Set PW_GAME_DIR to the mgspw folder "
        "or run this package from that folder."
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("command", choices=["resolve"])
    parser.parse_args()
    print(resolve())
