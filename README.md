# Peace Walker UltraWide Fix

[![Build and test](https://github.com/drbermejor/PeaceWalkerUltraWideFix/actions/workflows/build-and-test.yml/badge.svg)](https://github.com/drbermejor/PeaceWalkerUltraWideFix/actions/workflows/build-and-test.yml)
[![Latest release](https://img.shields.io/github/v/release/drbermejor/PeaceWalkerUltraWideFix?label=latest)](https://github.com/drbermejor/PeaceWalkerUltraWideFix/releases/latest)

Ultrawide support for **METAL GEAR SOLID PEACE WALKER - Master Collection Version** on Windows and Linux/Proton.

The fix removes the original side bars and corrects the 3D projection to produce a Hor+ ultrawide view instead of stretching the game. A centered 16:9 HUD/menu canvas and a reversible Unity-launcher bypass are optional.

> This is the first public release candidate. The ultrawide projection and the main centered-HUD path are working, but the limitations below are real and intentionally documented.

![Ultrawide gameplay with centered interface](docs/images/gameplay-subtitles.jpg)

## What it fixes

- Native ultrawide output without horizontal deformation.
- Hor+ field of view that preserves the game's current vertical FOV.
- Optional centered 16:9 HUD, menus, subtitles, briefings and mission selection.
- Automatic or explicit output resolution.
- Optional, reversible Unity-launcher bypass.
- Windows and Linux/Proton setup, configuration and uninstall flows.

## Windows — quick setup

1. Close the game.
2. Download the latest `windows.zip` from [Releases](https://github.com/drbermejor/PeaceWalkerUltraWideFix/releases/latest).
3. Extract it and run `PeaceWalkerUltraWideFix-Setup.cmd`.
4. Confirm the `mgspw` folder containing `METAL GEAR SOLID PEACE WALKER.exe`.
5. Keep the recommended settings, or disable **Centered 16:9 HUD** if you prefer the interface to span the full display.
6. Start the game normally through Steam.

The installer preserves a pre-existing `winmm.dll` and restores it on uninstall. It never silently removes a DLL or launcher that changed after installation.

## Linux / Proton — quick setup

1. Close the game and Steam.
2. Download and extract the latest `linux.tar.gz`.
3. Run `./PeaceWalkerUltraWideFix-Linux-Setup.sh`.
4. Run `./PeaceWalkerUltraWideFix-Linux-Configure.sh` if you want explicit resolution, full-width UI or the launcher bypass.
5. Restart Steam and launch the game normally.

The setup detects custom Steam libraries. If detection fails:

```bash
PW_GAME_DIR="/path/to/MGS_PW/mgspw" ./PeaceWalkerUltraWideFix-Linux-Setup.sh
```

Proton must use the native proxy:

```text
WINEDLLOVERRIDES="winmm=n,b" %command%
```

The Linux installer adds this reversibly when Steam is closed. If Steam is running, it prints the exact manual action instead of editing live state.

## Configuration

Run the installed `PeaceWalkerUltraWideFix-Configure.cmd` on Windows, or the Linux configure script. The same settings are available in `PeaceWalkerUltraWideFix.ini`:

```ini
[Fix]
Enabled=1
Width=0
Height=0
RemoveLetterboxing=1
CorrectFOV=1
CenterHUD=1

[Launcher]
BypassUnityLauncher=0
```

`Width=0` and `Height=0` use the physical primary display. Set both explicitly if automatic detection is unsuitable under Proton.

`CenterHUD=1` keeps most 2D presentation in a centered 16:9 canvas. `CenterHUD=0` leaves the interface at full output width while retaining the corrected ultrawide 3D view.

The launcher bypass reproduces the official Peace Walker launch protocol and starts the game as Steam's child process. It is off by default and can be toggled at any time; the original Unity launcher is backed up and restored conditionally.

The public default for the bypass is English. Other supported game tokens (`sp`, `fr`, `it`, `gr`, `jp`, `pt`) can be selected with `Language=` under `[Launcher]`.

## Known limitations

- Some pure or long loading illustrations can switch to full-width and appear stretched near the end of the load. Short loading screens, briefings and mission selection follow the centered presentation path.
- Full-screen 2D layers share the centered HUD canvas when `CenterHUD=1`, so side areas can remain uncovered during some transitions.
- The game still renders its 3D world internally at its original high-resolution target (1920×1088) before composing it to the selected output. This fix changes aspect and composition, not internal asset quality.
- Mouse-camera latency is present in the original port even with this patch disabled. It is not introduced by the ultrawide or HUD correction.
- The patch fails safely on an unknown executable. It currently targets the tested Steam Master Collection build.
- 3440×1440 is the primary visually verified ultrawide mode. The calculations are resolution-independent, but 32:9 and additional modes need broader public testing.

![Centered gameplay HUD](docs/images/gameplay-centered-hud.jpg)

![Centered mission menu](docs/images/mission-menu.jpg)

## Troubleshooting

- Check `PeaceWalkerUltraWideFix.log` beside the game executable.
- Confirm the DLL and INI are beside `METAL GEAR SOLID PEACE WALKER.exe`, not one directory above it.
- On Proton, confirm the `WINEDLLOVERRIDES` launch option.
- Disable `CenterHUD` to distinguish a HUD classification issue from the base ultrawide correction.
- Rerun setup after Steam verifies or updates game files.

When reporting a problem, include the log, resolution, OS/Proton version, whether `CenterHUD` and the launcher bypass are enabled, and a screenshot if the issue is visual.

## Build

Windows (MSVC + Ninja):

```powershell
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

Linux cross-build (MinGW-w64):

```bash
cmake -S . -B build -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-w64-x86_64.cmake \
  -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

Release archives are generated from CI-tested binaries with `scripts/package_release.py` and include SHA-256 checksums.

## License

[MIT](LICENSE)
