# Horizontal visibility validation — 3 September 2026

This is the reproducible acceptance record for the horizontal-visibility correction included in `v0.1.0-rc.4`.

## Source state

- Release label: `v0.1.0-rc.4`
- Exact Steam profile: build `25052315`
- Locally validated DLL SHA-256: `C799361DEC8C80EC04484452564FE59F61123AC9A7D3000344DFE8EAB62BD68C`
- Installed launcher bypass: enabled for the acceptance run

## Automated checks

- MSVC Release build: passed with `/W4` enabled by the project.
- CTest: 2/2 passed (`pw_math`, `pw_signatures`).
- Explicit signature audit: every one of the seven data/code locators matched exactly once in both locally held unpacked Steam executables.
- Visibility signature RVA / hook RVA:
  - build before 25052315: `0x8ed54` / `0x8ed5a`
  - build 25052315: `0x8eea4` / `0x8eeaa`

## Runtime checks

The diagnostic phase forced 5120x1440 angular coverage while leaving physical output at 3440x1440. F7 changed only the two horizontal visibility-plane seeds. During repeated traversal of the same jungle route, the original seeds produced intermittent missing-terrain polygons at lateral edges; the corrected seeds stopped that reproduction.

The decisive fixed-camera test used the production candidate at the real 3440x1440 output. With the projection, scene and camera unchanged, the live visibility constant alone was changed from the original `0.5625` to the corrected `0.4186046`. A black triangular opening at the left edge was replaced by the proper terrain in the next capture. Pixel comparison of the aligned frames measured 5.03% of the leftmost 10% changing by more than 30 summed RGB levels, with a mean summed difference of 7.75 in that band. The corrected value was read back successfully and the game remained responsive. The evidence images and exact scope are recorded in [Horizontal visibility correction](HORIZONTAL_VISIBILITY.md).

An earlier fixed-camera view had no object crossing the two visibility boundaries and therefore produced no lateral change. It was treated as a null control rather than efficacy evidence.

The diagnostic hotkey and aspect override were then removed. The exact production-candidate DLL listed above was installed and started through the launcher. Its log confirmed:

```text
Output 3440x1440 (aspect 2.38889).
Known profile independently confirmed by complete signatures.
Horizontal visibility corrected at 0x8eeaa (height/width=0.418605).
Projection corrected at its source (0x11a604).
Interface in 16:9 band: internal viewport X=245, width=1429 of 1920.
```

The game remained responsive and the in-game jungle view was visually accepted without the missing-geometry artifact.

## Malware scan

Microsoft Defender scanned the extracted DLL and launcher separately with remediation disabled. Both returned no threats.

```text
DLL SHA-256:      C799361DEC8C80EC04484452564FE59F61123AC9A7D3000344DFE8EAB62BD68C
Launcher SHA-256: 8D3E668E14BF916A6A3C9650E5974C7522B4C4E5AB96DD66136901F20EC474B9
Engine:           1.1.26080.3
Definitions:      1.459.28.0
Real-time:        enabled
Result:           no threats
```

A clean result is dated evidence, not a permanent guarantee for future Defender definitions or rebuilt binaries.

## Remaining acceptance boundary

- Native physical 5120x1440 should be checked independently; the local 32:9 result is an angular simulation.
- More stages with large objects near both lateral edges should be sampled.
- Existing centered-interface and loading-screen limitations are unchanged.
