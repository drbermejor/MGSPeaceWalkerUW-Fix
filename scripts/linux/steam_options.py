#!/usr/bin/env python3
"""Reversibly add the Proton winmm override to Steam launch options."""

from __future__ import annotations

import json
import os
import re
import sys
from pathlib import Path

APP_ID = "2492660"
PREFIX = 'WINEDLLOVERRIDES="winmm=n,b"'


def block_bounds(text: str) -> tuple[int, int]:
    match = re.search(r'"' + APP_ID + r'"\s*\{', text)
    if not match:
        raise RuntimeError(f"Steam app block {APP_ID} was not found")
    opening = text.find("{", match.start())
    depth = 0
    for index in range(opening, len(text)):
        if text[index] == "{":
            depth += 1
        elif text[index] == "}":
            depth -= 1
            if depth == 0:
                return opening + 1, index
    raise RuntimeError("Incomplete Steam VDF block")


def decode(value: str) -> str:
    return value.replace(r'\"', '"').replace(r"\\", "\\")


def encode(value: str) -> str:
    return value.replace("\\", r"\\").replace('"', r'\"')


def find_config() -> Path:
    override = os.environ.get("STEAM_LOCALCONFIG")
    if override:
        return Path(override)
    roots = [Path.home() / ".local/share/Steam/userdata",
             Path.home() / ".var/app/com.valvesoftware.Steam/.local/share/Steam/userdata"]
    for root in roots:
        for candidate in root.glob("*/config/localconfig.vdf"):
            try:
                if f'"{APP_ID}"' in candidate.read_text(errors="replace"):
                    return candidate
            except OSError:
                pass
    raise RuntimeError("Steam localconfig.vdf for Peace Walker was not found")


def launch_pattern() -> re.Pattern[str]:
    return re.compile(r'(?m)^([ \t]*)"LaunchOptions"[ \t]+"((?:\\.|[^"\\])*)"[ \t]*(?:\r?\n)?')


def update(config: Path, text: str) -> None:
    temporary = config.with_suffix(".vdf.pwuwfix.tmp")
    temporary.write_text(text)
    os.replace(temporary, config)


def install(state_path: Path) -> None:
    config = find_config()
    text = config.read_text()
    start, end = block_bounds(text)
    block = text[start:end]
    pattern = launch_pattern()
    match = pattern.search(block)
    previous = decode(match.group(2)) if match else None
    value = previous if previous and PREFIX in previous else (
        f"{PREFIX} {previous}" if previous else f"{PREFIX} %command%"
    )
    state_path.parent.mkdir(parents=True, exist_ok=True)
    state_path.write_text(json.dumps({"config": str(config), "previous": previous,
                                      "installed": value}, indent=2) + "\n")
    if match:
        replacement = f'{match.group(1)}"LaunchOptions"\t\t"{encode(value)}"\n'
        new_block = pattern.sub(replacement, block, count=1)
    else:
        indent = re.search(r'(\r?\n)([ \t]*)$', block)
        if not indent:
            raise RuntimeError("Could not insert Steam launch options")
        line = f'{indent.group(2)}\t"LaunchOptions"\t\t"{encode(value)}"'
        new_block = block[:indent.start()] + "\n" + line + indent.group(0)
    update(config, text[:start] + new_block + text[end:])


def uninstall(state_path: Path) -> None:
    if not state_path.exists():
        return
    state = json.loads(state_path.read_text())
    config = Path(state["config"])
    text = config.read_text()
    start, end = block_bounds(text)
    block = text[start:end]
    pattern = launch_pattern()
    match = pattern.search(block)
    current = decode(match.group(2)) if match else None
    if current != state["installed"]:
        raise RuntimeError("Steam launch options changed; leaving them untouched")
    if state["previous"] is None:
        new_block = pattern.sub("", block, count=1)
    else:
        line = f'{match.group(1)}"LaunchOptions"\t\t"{encode(state["previous"])}"\n'
        new_block = pattern.sub(line, block, count=1)
    update(config, text[:start] + new_block + text[end:])
    state_path.unlink()


if __name__ == "__main__":
    if len(sys.argv) != 3 or sys.argv[1] not in {"install", "uninstall"}:
        raise SystemExit(f"Usage: {sys.argv[0]} install|uninstall STATE.json")
    try:
        globals()[sys.argv[1]](Path(sys.argv[2]))
    except Exception as error:
        print(f"ERROR: {error}", file=sys.stderr)
        raise SystemExit(1)
