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
          "docs/ANTIVIRUS.md", "docs/HORIZONTAL_VISIBILITY.md",
          "docs/UPDATE_COMPATIBILITY.md",
          "docs/VALIDATION_2026-09-03_VISIBILITY.md", "docs/images"]
WINDOWS = ["PeaceWalkerUltraWideFix-Setup.cmd", "PeaceWalkerUltraWideFix-Configure.cmd",
           "PeaceWalkerUltraWideFix-Uninstall.cmd", "scripts/windows"]
LINUX = ["PeaceWalkerUltraWideFix-Linux-Setup.sh",
         "PeaceWalkerUltraWideFix-Linux-Configure.sh",
         "PeaceWalkerUltraWideFix-Linux-Uninstall.sh", "scripts/linux"]
MANUAL_COMMON = ["VERSION", "LICENSE", "SECURITY.md"]


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


def stage_manual(destination: Path, instructions: Path, dll: Path) -> None:
    for relative in MANUAL_COMMON:
        copy_item(ROOT / relative, destination / relative)
    shutil.copy2(dll, destination / "winmm.dll")
    shutil.copy2(ROOT / "config/PeaceWalkerUltraWideFix.ini",
                 destination / "PeaceWalkerUltraWideFix.ini")
    shutil.copy2(instructions, destination / "INSTALL.txt")


def normalize_linux_scripts(destination: Path) -> None:
    """Make archives created on Windows directly executable by Linux shells."""
    for pattern in ("*.sh", "*.py"):
        for path in destination.rglob(pattern):
            path.write_bytes(path.read_bytes().replace(b"\r\n", b"\n"))
            path.chmod(0o755)


def audit_guided(names: list[str]) -> None:
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


def audit_manual(names: list[str]) -> None:
    expected = {"INSTALL.txt", "LICENSE", "PeaceWalkerUltraWideFix.ini",
                "SECURITY.md", "VERSION", "winmm.dll"}
    actual = set(names)
    if actual != expected:
        missing = sorted(expected - actual)
        unexpected = sorted(actual - expected)
        raise SystemExit(
            f"Invalid manual archive; missing={missing}, unexpected={unexpected}")


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
    windows_manual_name = f"PeaceWalkerUltraWideFix-{version}-windows-manual.zip"
    linux_manual_name = f"PeaceWalkerUltraWideFix-{version}-linux-proton-manual.zip"
    outputs: list[Path] = []
    with tempfile.TemporaryDirectory() as temporary:
        base = Path(temporary)
        windows = base / "windows"
        linux = base / "linux"
        windows_manual = base / "windows-manual"
        linux_manual = base / "linux-manual"
        stage(windows, WINDOWS, args.dll, args.launcher)
        stage(linux, LINUX, args.dll, args.launcher)
        stage_manual(windows_manual, ROOT / "manual/windows/INSTALL.txt", args.dll)
        stage_manual(linux_manual, ROOT / "manual/linux/INSTALL.txt", args.dll)
        normalize_linux_scripts(linux)

        zip_path = args.output / windows_name
        with zipfile.ZipFile(zip_path, "w", zipfile.ZIP_DEFLATED, compresslevel=9) as archive:
            for path in sorted(windows.rglob("*")):
                if path.is_file():
                    archive.write(path, path.relative_to(windows).as_posix())
            audit_guided(archive.namelist())
        outputs.append(zip_path)

        tar_path = args.output / linux_name
        with tarfile.open(tar_path, "w:gz", format=tarfile.PAX_FORMAT) as archive:
            for path in sorted(linux.rglob("*")):
                info = archive.gettarinfo(
                    str(path), arcname=path.relative_to(linux).as_posix())
                if path.is_file() and path.suffix in {".sh", ".py"}:
                    # Windows has no Unix execute bit, so encode it explicitly.
                    info.mode = 0o755
                if path.is_file():
                    with path.open("rb") as stream:
                        archive.addfile(info, stream)
                else:
                    archive.addfile(info)
        with tarfile.open(tar_path, "r:gz") as archive:
            audit_guided([member.name for member in archive.getmembers() if member.isfile()])
        outputs.append(tar_path)

        for source, name in ((windows_manual, windows_manual_name),
                             (linux_manual, linux_manual_name)):
            path = args.output / name
            with zipfile.ZipFile(path, "w", zipfile.ZIP_DEFLATED,
                                 compresslevel=9) as archive:
                for item in sorted(source.rglob("*")):
                    if item.is_file():
                        archive.write(item, item.relative_to(source).as_posix())
                audit_manual(archive.namelist())
            outputs.append(path)

    checksums = args.output / "SHA256SUMS.txt"
    checksums.write_text("".join(f"{checksum(path)}  {path.name}\n" for path in outputs))
    for path in outputs + [checksums]:
        print(path)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
