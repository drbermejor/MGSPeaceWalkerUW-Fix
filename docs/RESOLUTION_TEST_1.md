# Private Windows resolution test 1

Version: `v0.1.0-rc.5-resolution-test.1`. Not a public release or a confirmed
fix for the reported 5120x2160 side bars.

**Reporter result, 5 September 2026: visual acceptance FAILED.** The active-row
write succeeded, but the left bar remained and the scene/UI became enlarged and
cropped. Do not promote this candidate. See [the framing audit](RESOLUTION_FRAMING_AUDIT.md)
for the verified code paths and the limits of the previous tests. Existing ZIP
artifacts are unchanged so their hashes remain valid.

The reporter's second screenshot retains side bars with `CenterHUD=0`, ruling
out our HUD hook as their source in that run. Successful hook-install messages
do not prove correct final GPU composition. The screenshot shows a roughly
16:9 region; it cannot identify which source resolution-table row was consumed.

## Candidate behavior

- Table writes are checked by reading back the values; failures no longer
  produce the unconditional rc.5 success message.
- `ResolutionDiagnostics=1` logs all four rows, height-inferred row, selected
  row, sampled active index, write attempts/failures, initial desktop resolution,
  INI overrides and window client dimensions. Snapshots occur about every five
  seconds for two minutes. These are asynchronous samples, not per-frame GPU
  viewport, scissor or swap-chain measurements.
- `FollowActiveResolution=1` reselects a row after three consecutive 250 ms
  samples agree on a valid active index. The context address is published only
  after complete code-target validation. Unreadable or changing context pointers
  are rejected through checked reads, without direct pointer dereferences.
- Early patching by height is retained. Previously written rows are left in
  place until game exit. A late correction might not repair cached startup
  offsets; report offset/cropped images as well as persistent side bars.
- Both new settings default to off in code. The private test INI enables them,
  selects automatic desktop resolution and disables centered HUD. The public
  default INI and HUD code have not been changed.
- No launcher binary or game files are included. No launcher behavior is changed.

## Validation on 5 September 2026

MSVC Release build succeeded. All three CTest suites passed: existing arithmetic
and code-island checks, signature checks, and the new Windows resolution runtime
test. The latter exercises the actual maintenance and Windows memory APIs using
a fake context and read-only table: a 2160p/active-1440p mismatch, stable sampling,
restored rows, readback, protection restoration, invalid/unreadable pointers,
1080p follow and observe-only behavior, disabled table writes, and the identical
5120x2160 / 2560x1080 aspect-band arithmetic.

The exact candidate DLL was started through the existing local Steam launcher
bypass on Steam build 25052315. With automatic output on a physical 3440x1440
desktop, it verified row 2 at 3440x1440 and observed active row 2. The game reached
mission selection and jungle gameplay with the scene spanning the display and
`CenterHUD=0`. The log reported one write attempt and no failures.

A second real-game run deliberately set the INI target to 5120x2160 while the
desktop remained 3440x1440. It first verified row 3, then observed stable active
row 2, switched to that row and verified 5120x2160 there. The process remained
responsive. This validates the selection mechanism; it is NOT a physical 5K2K
visual test and does not establish that the reporter has the same mismatch.

The local pre-test DLL, INI and log were restored with hash verification. Local
captures and logs are retained outside the distribution under
`.tools/pw-resolution-test-local-20260905` in the workspace.

The package script verifies ZIP contents by extraction and scans the extracted
DLL and ZIP with Microsoft Defender before reporting success. Individual payload
hashes are in the archive; the ZIP hash is supplied beside the archive.

## Remaining acceptance

The reporter should run at physical 5120x2160 with the included INI, save the
log and screenshot, then repeat with only `FollowActiveResolution=0` changed.
Compare active vs selected row, readback and images. If the active row already
matches and its contents remain verified while the bars persist, investigate
cached composition offsets, GPU viewports/scissors and actual presentation
dimensions. Do not infer these from the window client size or a success log.
