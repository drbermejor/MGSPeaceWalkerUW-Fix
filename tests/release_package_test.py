#!/usr/bin/env python3
"""Check release archive layouts and cross-platform installation details."""

from __future__ import annotations

import argparse
import tarfile
import zipfile
from pathlib import Path


MANUAL_FILES = {
    "INSTALL.txt",
    "LICENSE",
    "PeaceWalkerUltraWideFix.ini",
    "SECURITY.md",
    "VERSION",
    "winmm.dll",
}


def require(text: str, fragments: tuple[str, ...], label: str) -> None:
    missing = [fragment for fragment in fragments if fragment not in text]
    if missing:
        raise SystemExit(f"{label} is missing required instructions: {missing}")


def check_manual(path: Path, dll: Path, platform: str) -> None:
    with zipfile.ZipFile(path) as archive:
        names = set(archive.namelist())
        if names != MANUAL_FILES:
            raise SystemExit(f"Unexpected {platform} manual layout: {sorted(names)}")
        if archive.read("winmm.dll") != dll.read_bytes():
            raise SystemExit(f"{platform} manual DLL does not match the build")
        instructions = archive.read("INSTALL.txt").decode("utf-8")
        require(instructions,
                ("METAL GEAR SOLID PEACE WALKER.exe", "winmm.dll",
                 "PeaceWalkerUltraWideFix.ini", "back it up", "UNINSTALL"),
                f"{platform} INSTALL.txt")
        if platform == "Linux / Proton":
            require(instructions,
                    ('WINEDLLOVERRIDES="winmm=n,b" %command%',
                     "without scripts, chmod or sudo"),
                    f"{platform} INSTALL.txt")


def check_linux_guided(path: Path) -> None:
    with tarfile.open(path, "r:gz") as archive:
        scripts = [member for member in archive.getmembers()
                   if member.isfile() and
                   (member.name.endswith(".sh") or member.name.endswith(".py"))]
        if not scripts:
            raise SystemExit("Linux guided archive contains no scripts")
        for member in scripts:
            stream = archive.extractfile(member)
            if stream is None:
                raise SystemExit(f"Cannot read {member.name}")
            if b"\r\n" in stream.read():
                raise SystemExit(f"CRLF found in Linux script: {member.name}")
            if member.mode & 0o111 == 0:
                raise SystemExit(f"Linux script is not executable: {member.name}")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--dist", type=Path, required=True)
    parser.add_argument("--dll", type=Path, required=True)
    parser.add_argument("--version-file", type=Path, default=Path("VERSION"))
    args = parser.parse_args()
    version = args.version_file.read_text().strip()
    check_manual(
        args.dist / f"PeaceWalkerUltraWideFix-{version}-windows-manual.zip",
        args.dll, "Windows")
    check_manual(
        args.dist / f"PeaceWalkerUltraWideFix-{version}-linux-proton-manual.zip",
        args.dll, "Linux / Proton")
    check_linux_guided(
        args.dist / f"PeaceWalkerUltraWideFix-{version}-linux.tar.gz")
    print("Release package tests passed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
