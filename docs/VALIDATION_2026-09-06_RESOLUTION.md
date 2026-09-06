# Resolution startup validation — 2026-09-06

## Reporter acceptance

A Windows reporter using Steam build 25052315 tested private candidate
`v0.1.0-rc.5-resolution-test.2` at 5120x2160. The previous public build kept
gameplay inside side bars with either HUD setting. Private test 1 changed the
active table row too late and produced an enlarged, displaced crop.

The accepted test-2 log reports:

- Primary desktop and requested output: 5120x2160; Width=0, Height=0.
- Launch codes: resolution=1, upscale=2; selected and active row 2.
- One verified early table write, zero failures; the display context was
  unpublished both before and after that write.
- Cached canvas and output descriptor: 5120x2160, origin 0,0, matches-target=1.
- Physical window client: 5120x2160; consistent samples through 40 seconds.
- Complete code signatures independently confirm the known executable profile.

The corresponding tutorial screenshot shows the world filling the ultrawide
frame, without persistent side bars or the test-1 crop, with CenterHUD=0.
The reporter's follow-up supplied by the maintainer shows the same full-width
gameplay with the interface centered, and the maintainer accepted publication.
No second diagnostic log was supplied with that centered-HUD screenshot.

The accompanying cinematic screenshot has a centered approximately 16:9 image
with side bars. It is not evidence of a full-width cinematic and is not treated
as a recurrence of gameplay pillarboxing. Codec, every cinematic, every menu
and long loading transitions have not been exhaustively revalidated.

## Scope and reproducibility

The visual acceptance is for the private test-2 implementation on the reporter's
real 5120x2160 Windows system. Public rc.6 promotes that implementation with a
new release label and an automatic early-policy default on the audited profile;
the early write, parser, projection, visibility and HUD paths are unchanged.
Public builds are rebuilt and tested, not byte-identical to the private DLL.

Local 3440x1440 startup checks also covered a deliberately mismatched launch row
(upscale=3 instead of the height-selected row 2). Canvas, descriptor and origin
were consistent after the early row-3 write. Local interactive audio checks
confirmed sound with both the original DLL and candidate. The audio proxy was
not modified.

Windows runtime tests cover all canonical launch-mode combinations, checked
protected writes, refused late writes, no live migration/reassertion, stale
canvas and descriptor rejection, and multiple output dimensions. Tests also
check rc.6's audited-build default and retention of legacy defaults elsewhere.
These tests are not substitutes for GPU or visual acceptance at other modes.

The new startup policy has not received equivalent Proton runtime acceptance.
Physical 2560x1080 and 5120x1440 still require separate visual testing. Automatic
output remains the primary desktop, with explicit Width/Height overrides; it
does not select a secondary monitor or follow arbitrary launcher dimensions.
