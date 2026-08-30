#!/usr/bin/env bash
set -euo pipefail
SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
GAME_DIR="$(python3 "$SCRIPT_DIR/game_dir.py" resolve)"
INI="$GAME_DIR/PeaceWalkerUltraWideFix.ini"
[[ -f "$INI" ]] || { echo 'Run setup first.' >&2; exit 1; }
width="${PW_WIDTH:-0}"; height="${PW_HEIGHT:-0}"; hud="${PW_CENTER_HUD:-yes}"; bypass="${PW_BYPASS_LAUNCHER:-no}"
if [[ "${PW_HEADLESS:-0}" != 1 ]] && command -v zenity >/dev/null 2>&1 && [[ -n "${DISPLAY:-}${WAYLAND_DISPLAY:-}" ]]; then
  result="$(zenity --forms --title='Peace Walker UltraWide Fix' \
    --text='0 x 0 uses the primary display. Centered HUD is recommended but long loading illustrations may stretch.' \
    --separator='|' --add-entry='Width (0 = automatic)' --add-entry='Height (0 = automatic)' \
    --add-combo='Centered 16:9 HUD' --combo-values='Enabled|Disabled' \
    --add-combo='Skip Unity launcher' --combo-values='Disabled|Enabled' --width=720 --height=430)" || exit 0
  IFS='|' read -r width height hud_choice bypass_choice <<<"$result"
  width="${width:-0}"; height="${height:-0}"
  [[ "$hud_choice" == Enabled ]] && hud=yes || hud=no
  [[ "$bypass_choice" == Enabled ]] && bypass=yes || bypass=no
fi
python3 "$SCRIPT_DIR/configure.py" "$INI" --width "$width" --height "$height" --hud "$hud" --bypass "$bypass"
LAUNCHER_DIR="$(dirname -- "$GAME_DIR")/launcher"
ACTIVE="$LAUNCHER_DIR/launcher.exe"; MANAGED="$GAME_DIR/.PeaceWalkerUltraWideFix"; WRAPPER="$MANAGED/launcher-wrapper.exe"; ORIGINAL="$MANAGED/backup/launcher.exe.preinstall"
if [[ "$bypass" == yes ]]; then
  mkdir -p -- "$LAUNCHER_DIR"
  if [[ -f "$ACTIVE" ]] && ! cmp -s "$ACTIVE" "$WRAPPER"; then [[ -f "$ORIGINAL" ]] || cp -a -- "$ACTIVE" "$ORIGINAL"; fi
  install -m0755 "$WRAPPER" "$ACTIVE"
  sha256sum "$ACTIVE" | awk '{print $1}' >"$MANAGED/installed-launcher.sha256"
elif [[ -f "$MANAGED/installed-launcher.sha256" && -f "$ACTIVE" ]]; then
  current="$(sha256sum "$ACTIVE" | awk '{print $1}')"; installed="$(tr -d '[:space:]' <"$MANAGED/installed-launcher.sha256")"
  if [[ "$current" == "$installed" ]]; then
    if [[ -f "$ORIGINAL" ]]; then cp -a -- "$ORIGINAL" "$ACTIVE"; else rm -f -- "$ACTIVE"; fi
    rm -f -- "$MANAGED/installed-launcher.sha256"
  else echo 'launcher.exe changed externally; leaving it untouched.' >&2; fi
fi
echo "Saved: $INI"
