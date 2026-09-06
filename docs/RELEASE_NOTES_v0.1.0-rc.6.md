# Peace Walker UltraWide Fix v0.1.0-rc.6

Fixes the reported persistent gameplay side bars at **5120x2160** on Windows
with Steam build 25052315. The reporter validated the candidate with both
full-width and centered gameplay HUD.

## What changed

- Selects the resolution-table row from the game's launch mode codes, rather
  than guessing it from desktop height, on the audited Steam build.
- Applies the output dimensions before display initialization. No late row
  migration or rewriting during play, avoiding the crop seen in private test 1.
- Enables the validated policy automatically on build 25052315, including when
  keeping an older INI. Other executable profiles keep their prior policy.
- Adds checked writes, optional framing diagnostics and runtime regression tests.
- Preserves the existing projection, world visibility, HUD and audio proxy.

## Install / update

Close the game and use one of these assets:

- **Windows guided:** `PeaceWalkerUltraWideFix-v0.1.0-rc.6-windows.zip`
- **Windows manual:** `PeaceWalkerUltraWideFix-v0.1.0-rc.6-windows-manual.zip`
- **Linux / Proton guided:** `PeaceWalkerUltraWideFix-v0.1.0-rc.6-linux.tar.gz`
- **Linux / Proton manual:** `PeaceWalkerUltraWideFix-v0.1.0-rc.6-linux-proton-manual.zip`

Read the included installation instructions and back up existing files. The
default remains automatic primary-desktop resolution and centered HUD. No
3440x1440 resolution is hardcoded. Explicit Width and Height are still supported.
If retaining a test INI, remove `EarlyResolution=0` to restore automatic policy;
`CenterHUD=0` can be changed to 1 to center the interface. Diagnostics are optional.

## Validation and limitations

The reporter's real 5120x2160 Windows run confirms full-width tutorial gameplay,
consistent cached framing and centered-HUD presentation in a follow-up capture.
The accompanying cinematic still has centered side bars: this release does not
promise full-width cinematics or fix every 2D presentation issue.

- Centered HUD remains a candidate feature; some Codec content can appear
  compressed, and some loading illustrations or transitions remain imperfect.
- This startup correction is audited only for Steam build 25052315. Older and
  signature-only builds retain legacy resolution selection.
- The new path needs separate Proton runtime acceptance; other physical output
  modes and all game screens have not been exhaustively retested.
- Automatic output follows the primary desktop, not a secondary monitor or
  arbitrary launcher resolution. Restart after changing resolution.

See the repository's validation record for the exact evidence and scope.
Release publication is gated on automated tests, package auditing and Microsoft
Defender scanning, with checksums and build-provenance attestations. Keep antivirus
protection enabled.
