# Resolution framing audit — 5 September 2026

## Outcome

The reporter's `resolution-test.1` log confirms active row 2 while rc.5's
height-based selection chooses row 3 at 5120x2160. The candidate successfully
writes row 2, but the supplied screenshot fails visual acceptance: a left bar
remains, the scene and interface are enlarged, and content is cropped.

Static inspection of the same Steam build identifies the mechanism that can
produce exactly this kind of regression: live viewport dimensions come from the
resolution table, while composition origin, output dimensions, and graphics
resources are derived separately during initialization/reconfiguration. A
successful later table write does not keep them consistent.

No new runtime fix was implemented or packaged in this review. Installed game
files and monitor configuration were not changed.

## Binary evidence (Steam build 25052315)

Inspected local unpacked executable:
`../.tools/pw-static/mgspw-build-25052315.exe.unpacked.exe`

SHA256: `65c891d951cc020e74dcc36177f0a788770d2aab9f125037d8ffc9459343bd8e`

Addresses below are RVAs, not absolute process addresses.

| Location | Observed data flow |
| --- | --- |
| `0x27e00`, `0x27ea0` | Read parsed `resolution` and `upscale` argument values respectively. |
| `0x76c71..0x76c8c` | Pass those values into the display setup function at `0x1a5a0`. |
| `0x1a5dc..0x1a5ee` | Store the resolution code at context `+0x296c`; store `max(resolution code, upscale code)` at `+0x2970`. This is not a row inferred from desktop height. |
| `0x1a2b5..0x1a360` | Read table `0xd8f1c8` using `+0x2970`; calculate output dimensions at `+0x2940/+0x2944` and centered origin at `+0x2948/+0x294c`. |
| `0x190c7..0x190de` | Initialization calls that framing routine, then copies the resulting width/height to the structure at `+0x29a0/+0x29a4`. |
| `0x1916b..0x19180` | Pass the structure beginning at `+0x29a0` into the graphics factory call that returns the object stored at `+0x2938`. |
| `0x159c8..0x15a0b`, `0x16747..0x16777` | Other graphics resource setup also consumes the cached width/height. |
| `0x1783c..0x178d3` | The game's reconfiguration path recalculates framing, updates the dimensions structure, then recreates/resizes the graphics output. Merely editing four context integers would bypass this lifecycle. |
| `0x5c7a5..0x5c7e0` | Final composition obtains origin from `+0x2948/+0x294c`, but width/height from the current table row, and passes both to the viewport wrapper at `0x17dc0`. |
| `0x5c7e5..0x5c82f` | The clipping rectangle is constructed from that same origin plus current table dimensions. |

The private candidate's `maintain_resolution_once()` changes only the table. It
does not call the game's reconfiguration path, recreate resources, or update the
cached framing fields. Its active index becomes available only after complete
signature validation, followed by three stable samples at 250 ms intervals.
Removing only the sampling delay would not establish an initialization ordering
guarantee.

The local bypass explicitly passes `-resolution 1 -upscale 2`, which explains
active row 2 there. The reporter's actual command line was not supplied, so the
same launcher settings must not be assumed on their machine; their active row
2 is independently established by their log.

## Arithmetic reproduction and screenshot agreement

For mode-sizing inputs 5120x2160 and original active row 2560x1440, the inspected
float32/truncation operations compute:

* cached canvas: **3413x1440**;
* cached origin: **426,0**;
* original composition rectangle: **426,0,2560,1440**.

If that canvas is scaled over a 5120x2160 output, the original visible image is
approximately **3840x2160**, with a left margin of **639 pixels** (rounding makes
the two margins slightly unequal). This agrees with the roughly 16:9 content
inside the original screenshots, not the reporter's description of 4:3.

Changing only the live table row to 5120x2160 leaves the modeled origin and
canvas unchanged. The projected rectangle becomes approximately
**X=639, Y=0, width=7681, height=3240**, overflowing the display. This matches
the direction and scale of the new screenshot's enlargement/cropping while
preserving its left margin.

Supplying the correct row BEFORE these calculations instead produces a modeled
canvas of 5120x2160 and origin 0,0. This is an arithmetic result, not validation
of a new DLL or proof of the reporter's live GPU resource dimensions.

The standalone reproduction is `../.tools/pw-static/pw_framing_model.py`.
Run `python pw_framing_model.py` from that directory. It uses the inspected
float32 arithmetic and asserts the above cases; it does not execute the game.
The read-only PE inspection helper is `pw_inspect.py` in the same directory.

## Why local tests missed the failure

With active row 2, rc.5's height heuristic selects the correct row at 3440x1440
and 5120x1440, but the wrong row at 5120x2160 (row 3) and 2560x1080 (row 1).
This is a mode-selection/startup-order issue, not inherently an unsupported
ultrawide aspect ratio.

The three existing compiled test executables were rerun and pass. The resolution
fixture checks table selection, memory protection and readback, not framing
fields or graphics resource allocation. The previous local 5120x2160-INI run
on a 3440x1440 desktop proved the table switch only, as recorded at the time.
Neither test establishes correct 5K2K presentation.

## Requirements for the next implementation

1. Determine the initial row from verified game mode-selection semantics, not
   desktop height. Argument parsing must match the game's behavior, with guarded
   handling of absent, duplicate and invalid values and compatibility limits.
2. Make the correct dimensions available before the game computes framing and
   creates associated resources. A source-level interception at a validated
   initialization point is an alternative to early table selection; asynchronous
   polling alone does not guarantee the required order.
3. Validate cached canvas/origin, actual graphics output dimensions, and final
   composition viewport. Do not equate a window-client size with those values.
4. Do not perform live row migration unless the complete reconfiguration
   lifecycle is respected. Do not just zero the origin, force all rows, or shorten
   the sleep as a presumed complete fix.
5. Compare original/candidate at 5120x2160 and 2560x1080; retain 3440x1440 and
   5120x1440 regression coverage; test both HUD settings after validating the
   full scene. Revalidate ordinary launch and bypass launch independently.

The mechanism is strongly supported by the binary data flow, reporter log, and
numerical agreement. A corrected implementation still needs real-game visual
acceptance; this review does not certify a definitive fix.
