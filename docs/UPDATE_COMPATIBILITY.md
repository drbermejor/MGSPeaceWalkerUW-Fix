# Update compatibility

## What the automatic path covers

The signature resolver is designed for a common minor-update pattern: the game keeps the same instructions and data relationships but moves them to different RVAs. The September 2026 update behaved this way, with different areas moving by different amounts rather than one global offset.

For an executable whose PE identity is not listed, the patch requires all of the following before accepting the candidate:

- one exact resolution-table match in `.rdata`;
- one unique match for each of the six long code locators in decrypted `.text`;
- both frame call sites targeting the resolved viewport wrapper;
- the known projection store-to-epilogue and hook-to-return distances;
- a display context and derived projection matrix contained in `.data`.

If any condition fails, the affected hooks are not installed and the log states that compatibility was rejected.

## What it cannot guarantee

Signatures do not make the patch independent of the game implementation. Automatic compatibility is not accepted when an update:

- changes the projection or viewport logic;
- changes how the camera's CPU-side visibility planes are constructed;
- recompiles the relevant functions into a different instruction structure;
- creates zero or multiple matches for a locator;
- separates the projection matrix from the validated data block;
- changes frame ordering enough to invalidate the HUD classifier.

Those cases require new analysis and an in-game acceptance pass. Keeping the old exact profiles is deliberate: a known executable takes the strongest path and must still agree with all independently resolved signatures.

## Maintainer procedure after a Steam update

1. Install the update normally and record the Steam build ID, SHA-256, PE timestamp and image size.
2. Run the existing public binary once and keep `PeaceWalkerUltraWideFix.log`.
3. Build with `-DPWUWFIX_FORCE_SIGNATURE_FALLBACK=ON` to exercise the candidate path even if the executable later receives an exact profile.
4. Run `pw_signature_test` against a locally obtained unpacked image. Each locator must report exactly one match, and both call relationships must pass.
5. Start the game and confirm the log reports `Signature compatibility accepted` before checking visuals.
6. Validate at minimum: title/menu, gameplay FOV, lateral world visibility while rotating the camera, gameplay HUD, pause/map, Codec, mission briefing, a real-time cinematic, a prerecorded video and a long loading screen.
7. Add an exact profile only after the signature result and the visual acceptance pass agree.
8. Build Windows and MinGW payloads, run package smoke tests, inspect archive contents and verify SHA-256 files before publishing.

Example maintainer build:

```powershell
cmake -S . -B build-signature-check -G Ninja `
  -DCMAKE_BUILD_TYPE=Release `
  -DPWUWFIX_FORCE_SIGNATURE_FALLBACK=ON
cmake --build build-signature-check
ctest --test-dir build-signature-check --output-on-failure
```

The retail executable and any unpacked copy remain local. They must never be committed, attached to an issue or included in a release.

## User reports

After an update, a useful report contains:

- `PeaceWalkerUltraWideFix.log`;
- Steam build ID;
- operating system and Proton version where applicable;
- output resolution;
- enabled settings;
- a screenshot for a visual defect.

The log distinguishes an exact profile, a successfully accepted signature candidate and a rejected candidate. That distinction should be preserved when describing the result.
