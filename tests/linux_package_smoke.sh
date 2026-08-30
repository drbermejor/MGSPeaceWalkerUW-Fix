#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
FIXTURE="$(mktemp -d)"
trap 'rm -rf -- "$FIXTURE"' EXIT
export HOME="$FIXTURE/home" PW_GAME_DIR="$FIXTURE/Steam/steamapps/common/MGS_PW/mgspw"
export STEAM_LOCALCONFIG="$HOME/.local/share/Steam/userdata/1/config/localconfig.vdf"
mkdir -p -- "$PW_GAME_DIR" "$(dirname -- "$STEAM_LOCALCONFIG")" "$(dirname -- "$PW_GAME_DIR")/launcher"
printf fixture >"$PW_GAME_DIR/METAL GEAR SOLID PEACE WALKER.exe"
printf previous >"$PW_GAME_DIR/winmm.dll"
printf original >"$(dirname -- "$PW_GAME_DIR")/launcher/launcher.exe"
cat >"$STEAM_LOCALCONFIG" <<'EOF'
"UserLocalConfigStore"
{
  "Software"
  {
    "Valve"
    {
      "Steam"
      {
        "apps"
        {
          "2492660"
          {
          }
        }
      }
    }
  }
}
EOF
"$ROOT/scripts/linux/install.sh"
grep -q 'WINEDLLOVERRIDES' "$STEAM_LOCALCONFIG"
PW_HEADLESS=1 PW_WIDTH=3440 PW_HEIGHT=1440 PW_CENTER_HUD=yes PW_BYPASS_LAUNCHER=yes "$ROOT/scripts/linux/configure.sh"
cmp "$ROOT/bin/launcher.exe" "$(dirname -- "$PW_GAME_DIR")/launcher/launcher.exe"
"$ROOT/scripts/linux/uninstall.sh"
grep -q 'WINEDLLOVERRIDES' "$STEAM_LOCALCONFIG" && { echo 'Steam option not restored' >&2; exit 1; }
grep -q original "$(dirname -- "$PW_GAME_DIR")/launcher/launcher.exe"
grep -q previous "$PW_GAME_DIR/winmm.dll"
echo 'Linux package smoke test passed.'
