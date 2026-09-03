# Peace Walker UltraWide Fix v0.1.0-rc.3

This candidate makes game-update compatibility safer and more resilient while
preserving the ultrawide, projection and centered-interface behavior validated
in the previous release.

## What changed

- Added a fail-closed signature path for game updates that move otherwise
  unchanged code and data.
- Kept exact profiles for both tested Steam executables and independently
  verifies every hook target on known builds.
- Added structural validation for call targets, PE sections, projection data
  and detour boundaries before installing a hook.
- Fixed a live-code page-permission race that could raise an access violation
  while late hooks were installed.
- Added deterministic signature tests and documented the exact compatibility
  boundary after a game update.
- Added SHA-256 checksums, GitHub build-provenance attestations and documented
  antivirus verification guidance.

## Choose an asset

- **Windows:** `PeaceWalkerUltraWideFix-v0.1.0-rc.3-windows.zip`
- **Linux / Proton:** `PeaceWalkerUltraWideFix-v0.1.0-rc.3-linux.tar.gz`

Both packages contain the same Windows proxy DLL. The Linux package adds the
Proton setup and configuration scripts.

## Important limitations

- The optional centered HUD remains experimental.
- Some real-time Codec video content can still appear horizontally compressed.
- Some pure or long loading illustrations can switch to full width near the end
  of loading.
- Full-screen 2D layers can leave side areas uncovered during some transitions
  with `CenterHUD=1`.
- 3440x1440 remains the primary visually verified mode; wider aspect ratios need
  broader testing.

Set `CenterHUD=0` if a 2D screen is affected. Hor+ projection and pillarbox
removal remain available independently.

Keep antivirus protection enabled. If a release file is blocked, verify the
same release's checksum and report the exact filename, SHA-256 and detection
name. Do not add a broad antivirus exclusion.
