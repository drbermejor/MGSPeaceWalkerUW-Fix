# Changelog

## Unreleased

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
