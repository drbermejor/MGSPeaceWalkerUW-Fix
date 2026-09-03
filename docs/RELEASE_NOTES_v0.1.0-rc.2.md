# Peace Walker UltraWide Fix v0.1.0-rc.2

This compatibility release restores the fix after the September game update and is published as the current **Latest** release.

## What changed

- Added support for Steam build `25052315`.
- Kept compatibility with the previously supported Steam executable.
- Added exact executable profiles so every patch remains fail-closed on unknown builds.

## Choose an asset

- **Windows:** `PeaceWalkerUltraWideFix-v0.1.0-rc.2-windows.zip`
- **Linux / Proton:** `PeaceWalkerUltraWideFix-v0.1.0-rc.2-linux.tar.gz`

Both packages contain the same updated Windows proxy DLL. The Linux package provides the Proton setup scripts and DLL override configuration.

An exact source snapshot used to build this compatibility release is also attached.

## Candidate limitations

- The optional centered HUD remains experimental.
- Some real-time Codec video content can still appear horizontally compressed.
- Some pure or long loading illustrations can switch to full width near the end of loading.
- Full-screen 2D layers can leave the side areas uncovered during some transitions when `CenterHUD=1`.
- 3440×1440 is the primary visually verified mode; wider aspect ratios need broader testing.

Set `CenterHUD=0` if a 2D screen is affected. The Hor+ ultrawide projection and pillarbox removal remain active.
