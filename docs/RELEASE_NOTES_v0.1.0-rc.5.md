# Peace Walker UltraWide Fix v0.1.0-rc.5

This candidate adds direct manual installation packages and repairs the guided
Linux archive. The runtime ultrawide, horizontal-visibility and centered-interface
behavior is unchanged from rc.4.

## What changed

- Added a script-free `windows-manual.zip` with `winmm.dll`, the default INI and
  direct installation and removal instructions.
- Added a script-free `linux-proton-manual.zip` with the same core payload and
  the required Proton launch option documented in `INSTALL.txt`.
- Normalized scripts in the guided Linux archive to Unix LF endings. This fixes
  `/usr/bin/env: 'bash\r': No such file or directory` on Bazzite and other Linux
  systems when the release archive was produced on Windows.
- Added release tests for archive layout, payload identity, Unix line endings and
  executable permissions.

## Choose an asset

- **Windows guided setup:** `PeaceWalkerUltraWideFix-v0.1.0-rc.5-windows.zip`
- **Windows manual:** `PeaceWalkerUltraWideFix-v0.1.0-rc.5-windows-manual.zip`
- **Linux / Proton guided setup:** `PeaceWalkerUltraWideFix-v0.1.0-rc.5-linux.tar.gz`
- **Linux / Proton manual:** `PeaceWalkerUltraWideFix-v0.1.0-rc.5-linux-proton-manual.zip`

Use a guided package for automatic discovery, managed backups, configuration,
uninstall tools or the optional Unity-launcher bypass. Use a manual ZIP for a
direct installation with no scripts. Read its `INSTALL.txt` before copying files.

## Important limitations

- The optional centered HUD remains experimental.
- Some real-time Codec video content can still appear horizontally compressed.
- Some pure or long loading illustrations can switch to full width near the end
  of loading.
- Full-screen 2D layers can leave side areas uncovered during some transitions
  with `CenterHUD=1`.
- 3440x1440 remains the primary physically verified ultrawide mode; physical
  5120x1440 and additional modes need broader public testing.

Set `CenterHUD=0` if a 2D screen is affected. Hor+ projection, matching world
visibility and pillarbox removal remain available independently.

Keep antivirus protection enabled. Publishing is gated on a Microsoft Defender
scan of the final Windows packages. If a release file is blocked, verify the same
release's checksum and report the exact filename, SHA-256 and detection name.
Do not add a broad antivirus exclusion.
