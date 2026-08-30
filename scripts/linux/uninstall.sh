#!/usr/bin/env bash
set -euo pipefail
SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
GAME_DIR="$(python3 "$SCRIPT_DIR/game_dir.py" resolve)"
MANAGED="$GAME_DIR/.PeaceWalkerUltraWideFix"; BACKUP="$MANAGED/backup"
if [[ -f "$MANAGED/steam-options.json" ]]; then
  pgrep -x steam >/dev/null 2>&1 && { echo 'Exit Steam before uninstalling.' >&2; exit 1; }
  python3 "$SCRIPT_DIR/steam_options.py" uninstall "$MANAGED/steam-options.json"
fi
if [[ -f "$MANAGED/installed-launcher.sha256" ]]; then PW_HEADLESS=1 PW_BYPASS_LAUNCHER=no "$SCRIPT_DIR/configure.sh"; fi
if [[ -f "$GAME_DIR/winmm.dll" && -f "$MANAGED/installed-winmm.sha256" ]]; then
  current="$(sha256sum "$GAME_DIR/winmm.dll" | awk '{print $1}')"; installed="$(tr -d '[:space:]' <"$MANAGED/installed-winmm.sha256")"
  [[ "$current" == "$installed" ]] || { echo 'winmm.dll changed externally; leaving it untouched.' >&2; exit 1; }
  rm -f -- "$GAME_DIR/winmm.dll"
fi
if [[ -f "$BACKUP/winmm.dll.preinstall" ]]; then mv -- "$BACKUP/winmm.dll.preinstall" "$GAME_DIR/winmm.dll"; fi
rm -rf -- "$MANAGED"
if [[ "${PW_REMOVE_CONFIG:-0}" == 1 ]]; then rm -f -- "$GAME_DIR/PeaceWalkerUltraWideFix.ini"; fi
echo "Uninstalled from: $GAME_DIR"
