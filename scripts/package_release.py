#!/usr/bin/env python3
"""Create audited Windows and Linux release archives."""

from __future__ import annotations

import argparse
import hashlib
import shutil
import tarfile
import tempfile
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
COMMON = ["VERSION", "README.md", "CHANGELOG.md", "LICENSE", "SECURITY.md",
          "config/PeaceWalkerUltraWideFix.ini", "docs/ARCHITECTURE.md",
          "docs/ANTIVIRUS.md", "docs/UPDATE_COMPATIBILITY.md", "docs/images"]
WINDOWS = ["PeaceWalkerUltraWideFix-Setup.cmd", "PeaceWalkerUltraWideFix-Configure.cmd",
           "PeaceWalkerUltraWideFix-Uninstall.cmd", "scripts/windows"]
LINUX = ["PeaceWalkerUltraWideFix-Linux-Setup.sh",
         "PeaceWalkerUltraWideFix-Linux-Configure.sh",
         "PeaceWalkerUltraWideFix-Linux-Uninstall.sh", "scripts/linux"]


def copy_item(source: Path, destination: Path) -> None:
    if source.is_dir():
        shutil.copytree(source, destination,
                        ignore=shutil.ignore_patterns("__pycache__", "*.pyc"))
    else:
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, destination)


def stage(destination: Path, files: list[str], dll: Path, launcher: Path) -> None:
    for relative in COMMON + files:
        source = ROOT / relative
        if not source.exists():
            raise SystemExit(f"Missing package input: {relative}")
        copy_item(source, destination / relative)
    (destination / "bin").mkdir()
    shutil.copy2(dll, destination / "bin/winmm.dll")
    shutil.copy2(launcher, destination / "bin/launcher.exe")


def audit(names: list[str]) -> None:
    required = {"VERSION", "README.md", "LICENSE", "bin/winmm.dll",
                "bin/launcher.exe", "config/PeaceWalkerUltraWideFix.ini"}
    missing = sorted(required - set(names))
    if missing:
        raise SystemExit(f"Archive missing: {', '.join(missing)}")
    forbidden = (".pdb", ".obj", ".ilk", ".pyc", ".log", "__pycache__",
                 "handoff", "research", "probe",
                 "scratchpad", ".git")
    bad = [name for name in names if any(token in name.lower() for token in forbidden)]
    if bad:
        raise SystemExit(f"Private/debug files in archive: {bad}")


def checksum(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--dll", type=Path, required=True)
    parser.add_argument("--launcher", type=Path, required=True)
    parser.add_argument("--output", type=Path, default=ROOT / "dist")
    args = parser.parse_args()
    version = (ROOT / "VERSION").read_text().strip()
    args.output.mkdir(parents=True, exist_ok=True)
    windows_name = f"PeaceWalkerUltraWideFix-{version}-windows.zip"
    linux_name = f"PeaceWalkerUltraWideFix-{version}-linux.tar.gz"
    outputs: list[Path] = []
    with tempfile.TemporaryDirectory() as temporary:
        base = Path(temporary)
        windows = base / "windows"
        linux = base / "linux"
        stage(windows, WINDOWS, args.dll, args.launcher)
        stage(linux, LINUX, args.dll, args.launcher)
        for path in linux.rglob("*.sh"):
            path.chmod(0o755)
        for path in linux.rglob("*.py"):
            path.chmod(0o755)

        zip_path = args.output / windows_name
        with zipfile.ZipFile(zip_path, "w", zipfile.ZIP_DEFLATED, compresslevel=9) as archive:
            for path in sorted(windows.rglob("*")):
                if path.is_file():
                    archive.write(path, path.relative_to(windows).as_posix())
            audit(archive.namelist())
        outputs.append(zip_path)

        tar_path = args.output / linux_name
        with tarfile.open(tar_path, "w:gz", format=tarfile.PAX_FORMAT) as archive:
            for path in sorted(linux.rglob("*")):
                archive.add(path, arcname=path.relative_to(linux).as_posix(), recursive=False)
        with tarfile.open(tar_path, "r:gz") as archive:
            audit([member.name for member in archive.getmembers() if member.isfile()])
        outputs.append(tar_path)

    checksums = args.output / "SHA256SUMS.txt"
    checksums.write_text("".join(f"{checksum(path)}  {path.name}\n" for path in outputs))
    for path in outputs + [checksums]:
        print(path)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
