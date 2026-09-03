# Changelog

## Unreleased

## v0.1.0-rc.5 — 2026-09-03

- Adds dedicated script-free manual ZIPs for Windows and Linux/Proton.
- Places `winmm.dll`, `PeaceWalkerUltraWideFix.ini` and a short platform-specific
  `INSTALL.txt` at the root of each manual archive for direct installation.
- Documents safe backup, configuration and removal steps for manual installs.
- Normalizes every shell and Python script in the guided Linux archive to LF
  endings, fixing `/usr/bin/env: 'bash\r': No such file or directory` in packages
  produced by the Windows release runner.
- Audits Linux script line endings and executable modes, manual archive layouts
  and payload identity before publication.
- Does not change the runtime ultrawide, visibility or centered-interface behavior
  introduced in rc.4.

## v0.1.0-rc.4 — 2026-09-03

- Corrects the CPU-side horizontal visibility boundary together with the Hor+
  projection so ultrawide-edge terrain is no longer rejected by the original
  16:9 planes.
- Derives the left/right plane coefficient from the live vertical projection
  scale and output aspect while preserving the original top/bottom planes.
- Locates the visibility-plane builder with a long unique signature in both
  maintained Steam executables and requires it as part of fail-closed
  compatibility detection.
- Installs visibility before projection and leaves `CorrectFOV` unapplied if
  the matching visibility correction cannot be installed.
- Adds deterministic code-island and signature tests plus fixed-camera visual
  evidence at 3440x1440.
- Documents the distinction between physical 3440x1440 validation, simulated
  5120x1440 angular coverage and the remaining physical 5120x1440 acceptance
  check.

- Windows setup now ignores stale Steam-library entries whose drives are no
  longer mounted, then continues searching the remaining libraries.
- Adds a regression test covering an unavailable drive followed by a valid
  custom Steam library.

## v0.1.0-rc.3 — 2026-09-03

- Adds a fail-closed signature compatibility path for address-only game updates.
- Validates the complete 32-byte resolution table before any table write.
- Uses long, unique instruction windows and verifies related call targets before installing hooks.
- Adds deterministic signature tests and a maintainer-only forced-fallback build mode.
- Documents the runtime architecture, update boundary and validation procedure.
- Preserves execute permission while patching live code pages, avoiding a race
  with game threads already executing on the same page.
- Adds a repeatable Defender check for extracted release executables and the
  final package.
- Adds GitHub build-provenance attestations for future release assets.
- Documents antivirus false-positive handling without recommending exclusions
  or disabling protection.

## v0.1.0-rc.2 — 2026-09-02

- Restores compatibility with Steam build 25052315 after the September game update.
- Retains support for the previously tested Steam executable.
- Selects all patch addresses from an exact, fail-closed executable profile.

The centered-HUD behavior and its documented candidate limitations are unchanged.

## v0.1.0-rc.1 — 2026-08-30

First public candidate.

- Removes the original 16:9 pillarbox at ultrawide resolutions.
- Corrects the projection at its verified per-frame writer for stable Hor+ rendering.
- Adds optional centered 16:9 HUD, menus, subtitles and cinematic presentation.
- Adds a reversible optional Unity-launcher bypass.
- Adds Windows and Linux/Proton setup, configuration and uninstall tooling.
- Adds automatic custom-Steam-library detection and reversible Proton launch-option management.
- Adds MSVC and MinGW CI, tests, package auditing and signed-by-checksum release assets.

Known candidate limitation: some pure or long loading illustrations can still switch to full-width and appear stretched.
