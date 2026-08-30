#!/usr/bin/env bash
set -euo pipefail
SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
PACKAGE_DIR="$(cd -- "$SCRIPT_DIR/../.." && pwd)"
GAME_DIR="$(python3 "$SCRIPT_DIR/game_dir.py" resolve)"
MANAGED="$GAME_DIR/.PeaceWalkerUltraWideFix"
BACKUP="$MANAGED/backup"
mkdir -p -- "$BACKUP"
[[ -f "$PACKAGE_DIR/bin/winmm.dll" ]] || { echo 'Missing bin/winmm.dll' >&2; exit 1; }
[[ -f "$PACKAGE_DIR/bin/launcher.exe" ]] || { echo 'Missing bin/launcher.exe' >&2; exit 1; }
if [[ -f "$GAME_DIR/winmm.dll" ]] && ! cmp -s "$PACKAGE_DIR/bin/winmm.dll" "$GAME_DIR/winmm.dll"; then
  [[ -f "$BACKUP/winmm.dll.preinstall" ]] || cp -a -- "$GAME_DIR/winmm.dll" "$BACKUP/winmm.dll.preinstall"
fi
install -m0644 "$PACKAGE_DIR/bin/winmm.dll" "$GAME_DIR/winmm.dll"
install -m0755 "$PACKAGE_DIR/bin/launcher.exe" "$MANAGED/launcher-wrapper.exe"
install -m0644 "$PACKAGE_DIR/VERSION" "$MANAGED/version"
sha256sum "$GAME_DIR/winmm.dll" | awk '{print $1}' >"$MANAGED/installed-winmm.sha256"
if [[ ! -f "$GAME_DIR/PeaceWalkerUltraWideFix.ini" ]]; then
  install -m0644 "$PACKAGE_DIR/config/PeaceWalkerUltraWideFix.ini" "$GAME_DIR/PeaceWalkerUltraWideFix.ini"
fi
if ! pgrep -x steam >/dev/null 2>&1; then
  python3 "$SCRIPT_DIR/steam_options.py" install "$MANAGED/steam-options.json"
else
  echo 'Steam is running: add WINEDLLOVERRIDES="winmm=n,b" %command% to Launch Options, or exit Steam and rerun setup.' >&2
fi
echo "Installed in: $GAME_DIR"
echo 'Run PeaceWalkerUltraWideFix-Linux-Configure.sh to change resolution, HUD or launcher bypass.'
