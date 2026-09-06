# Private Windows resolution test 2

Version: `v0.1.0-rc.5-resolution-test.2`. Private candidate for Steam build 25052315.
Reporter acceptance at 5120x2160 was subsequently confirmed on 2026-09-06;
see [public validation record](VALIDATION_2026-09-06_RESOLUTION.md).
This supersedes the failed late-migration experiment in test 1.

## What changed

With `EarlyResolution=1`, the initial row is selected from canonical launch
arguments using the audited `max(resolution, upscale)` rule. It is NOT inferred
from desktop height and does not wait for a live active-index sample.

The validated resolution table receives one synchronous, checked write in the
initialization worker, before code decryption/signature waiting and before the
monitor thread starts. The write is refused if the exact-profile display-context
pointer is already nonzero or unreadable. Context publication is sampled again
after the write; if it appeared meanwhile, the log explicitly warns that startup
ordering has not been established. These samples are not a global thread lock.

After that initial write, this mode only monitors the table. It neither migrates
to another row nor reasserts restored values at runtime. A mismatch is reported
instead of attempting the unsafe late correction that caused test 1's cropping.
`FollowActiveResolution` is retired and ignored.

The existing complete code signatures still independently validate the code
hooks after decryption. Early mode is confined to exact Steam build 25052315;
unknown/other profiles are refused. The original table signature must still be
unique and at the expected known RVA before the early write.

Absent resolution/upscale arguments use zero as the game does. Duplicate,
missing-value, noncanonical or out-of-range recognized arguments are refused,
as are more than 32 arguments. This intentionally accepts a conservative subset
of the game's argument parser rather than guessing ambiguous mode choices.
Only mode codes are logged, not the full command line.

## Diagnostics and settings

The supplied INI uses desktop auto-detection, `EarlyResolution=1`,
`ResolutionDiagnostics=1`, and `CenterHUD=0`. The HUD/projection/frustum islands
are unchanged, and centered HUD remains available by setting `CenterHUD=1`
and restarting. No launcher is included or modified by this package.

Every five seconds for two minutes, diagnostics show the selected/active row,
table readback, window size, and the following audited CPU context fields:

* canvas width/height (`+0x2940/+0x2944`);
* composition origin (`+0x2948/+0x294c`);
* cached output-descriptor width/height (`+0x29a0/+0x29a4`).

`matches-target=1` requires the expected active row, both dimension pairs equal
to the requested output, and origin 0,0. These are asynchronous CPU reads, NOT
measurements of the live GPU resource or final viewport. A screenshot is still
required. No COM graphics calls or extra rendering hooks are made by diagnostics.

`EarlyResolution=0` retains rc.5's height-selected policy for controlled A/B
comparison. It does NOT enable test 1's active-row migration. Public INI defaults
were not changed in this private build; early mode defaulted off unless explicitly
requested. Public rc.6 enables it by default only on the audited exact build.

## Validation

MSVC Release compilation succeeded. All three CTest suites passed. The Windows
runtime fixture exercises the actual protected memory write and readback,
immediate row-2 selection before context publication at 5120x2160, unmodified
other rows, rejected late/unreadable contexts, disabled writes, no live migration
or reassertion, and targets 2560x1080, 3440x1440 and 5120x1440.

New parser checks cover every supported resolution/upscale combination, defaults,
quoting/order and rejected ambiguous inputs. Framing checks explicitly reject
the test-1 regression (canvas 3413x1440, origin 426,0) despite a correct table,
and separately reject a stale output descriptor. The fixture cannot validate
game resource allocation, hook scheduling, or actual GPU composition.

A real local Steam-bypass startup at 3440x1440 with CenterHUD=0 selected row 2
from `resolution=1, upscale=2`. The context was unpublished on both sides of the
write. Once initialized, active row 2, canvas/descriptor 3440x1440 and origin 0,0
remained consistent, with one write and zero failures. Startup screenshots cover
the publisher logo/transition, not gameplay. The original installed DLL, INI and
log were restored and their hashes checked after the test.

A second local run deliberately used `-resolution 1 -upscale 3` on the same
3440x1440 desktop. This creates a real mismatch between the old height heuristic
(row 2) and the game's selected mode (row 3). The candidate wrote row 3 before
context publication, then observed active row 3 with canvas/descriptor 3440x1440,
origin 0,0, one write, and zero failures. A captured staff menu spans the display
without the persistent side bars. This supports early mode selection in a real
game run, but is not a fixed-camera gameplay comparison or native 5120x2160 test.
The original installed files were again restored with hash verification.

The user initially reported no sound during an automated startup. Two subsequent
interactive launches were checked by the user: the original DLL had sound, and
then this candidate also had sound. The candidate comparison retained the user's
CenterHUD=1 and launcher settings, adding only EarlyResolution and diagnostics.
The early write occurred before context publication and the HUD hook installed.
The initial silence was not reproduced; its cause has not been established.
The audio proxy implementation and export list are unchanged. This listening
confirmation applies to the local system, not the reporter's system.

## Reporter acceptance

Keep Windows at 5120x2160 and the same launcher settings as before. Replace BOTH
DLL and INI, restart, enter the tutorial, and save the log and a full screenshot.
The expected CPU state is canvas/descriptor 5120x2160, origin 0,0 and selected row
matching active row. Then, if the world fills the display without cropping,
repeat with centered HUD enabled. Save each log before restarting.

Errors, incorrect cached framing or visual cropping mean the candidate is not
accepted. Do not attempt to repair a running session by changing table values.
Close the game and restore the pre-test files if the image is worse.

The packaging script verifies the actual ZIP by extraction and hash comparison,
then scans the extracted DLL and ZIP with Microsoft Defender. Hashes and scan
results establish artifact integrity/security checks, not visual correctness.
