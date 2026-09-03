# Peace Walker UltraWide Fix v0.1.0-rc.4

This candidate fixes the intermittent missing geometry reported at very wide
aspect ratios. The Hor+ projection and the game's CPU-side horizontal visibility
boundary now use the same output aspect.

## What changed

- Corrected the left and right visibility planes that retained the original
  16:9 boundary after the rendered projection was widened.
- Preserved the original vertical FOV and top/bottom visibility planes.
- Added the visibility-plane builder to complete signature detection for both
  maintained Steam executables and unknown address-only updates.
- Made `CorrectFOV` fail closed: the wider projection is not installed unless
  its matching visibility correction is available.
- Kept the existing current-build compatibility, optional centered interface,
  reversible launcher bypass and stale Steam-library-drive fix.
- Added deterministic tests and public technical documentation for the new hook.

## Fixed-camera evidence

At 3440x1440, the camera, projection and scene were held fixed while only the
horizontal visibility value changed. The original 16:9 value rejected terrain
at the left edge; the corrected output-aspect value restored it in the next
aligned frame.

Original horizontal visibility:

![Original visibility leaves a black opening at the left edge](https://raw.githubusercontent.com/drbermejor/MGSPeaceWalkerUW-Fix/v0.1.0-rc.4/docs/images/visibility-original-3440x1440.jpg)

Corrected horizontal visibility:

![Corrected visibility restores the terrain](https://raw.githubusercontent.com/drbermejor/MGSPeaceWalkerUW-Fix/v0.1.0-rc.4/docs/images/visibility-corrected-3440x1440.jpg)

The 5120x1440 angular path was also reproduced while retaining a physical
3440x1440 output. A physical 5120x1440 run remains a useful independent
acceptance check, not an unresolved mechanism.

## Choose an asset

- **Windows:** `PeaceWalkerUltraWideFix-v0.1.0-rc.4-windows.zip`
- **Linux / Proton:** `PeaceWalkerUltraWideFix-v0.1.0-rc.4-linux.tar.gz`

Both packages contain the same Windows proxy DLL. The Linux package adds the
Proton setup and configuration scripts.

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
scan of the final Windows package. If a release file is blocked, verify the same
release's checksum and report the exact filename, SHA-256 and detection name.
Do not add a broad antivirus exclusion.
