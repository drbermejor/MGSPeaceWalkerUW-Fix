# Architecture

This document describes the current implementation in terms that can be checked against the source and the running game. It intentionally separates verified behavior from candidate compatibility logic.

## Runtime components

The package loads as a `winmm.dll` proxy beside the Peace Walker executable. Every exported multimedia function is forwarded to the system DLL. A worker thread performs the ultrawide initialization so that loader-sensitive work does not run directly inside `DllMain`.

The fix has three independent settings:

| Setting | Game behavior | Implementation |
| --- | --- | --- |
| `RemoveLetterboxing` | Makes the output surface use the requested ultrawide size | Replaces one validated row in the four-entry resolution table and reasserts it if the game restores the row |
| `CorrectFOV` | Preserves vertical FOV, expands horizontal view and keeps visibility consistent with it | Redirects the projection-builder epilogue to write `m0 = m5 / aspect`, then widens only the camera's left/right visibility planes from the same live FOV and output aspect |
| `CenterHUD` | Fits the shared 2D presentation into a centered 16:9 band | Redirects the viewport wrapper and classifies the 2D block from two verified frame call sites |

The projection is corrected inside the function that produces it. An external maintenance loop loses a race against the game's once-per-frame writer and can flicker, so it remains only as a logged last-resort fallback if the source hook cannot be installed.

The game builds six CPU-side visibility planes immediately after updating the camera. Its left/right seeds keep the original PSP aspect even after the rendered projection is widened. The visibility island replaces only those two horizontal seeds. It derives the current vertical scale from live camera fields rather than assuming a fixed FOV; the non-horizontal stores remain byte-for-byte equivalent to the original code. The visibility hook is installed before the projection hook. If it cannot be installed, `CorrectFOV` is not applied, avoiding a wider rendered view paired with narrower visibility.

## Startup order

1. Read the PE identity and select an exact profile when one is known.
2. Find the original four-row resolution table uniquely inside `.rdata` and verify all 32 bytes before allowing a write.
3. Start table maintenance early. The initial framing can otherwise be calculated from the original 16:9 row while SteamStub is still decrypting `.text`.
4. Wait for the in-memory `.text` section to be decrypted.
5. Resolve six complete code signatures and validate their relationships.
6. For a known profile, require every resolved RVA to match the profile.
7. Install only the enabled hooks.

SteamStub encrypts the retail code on disk, so code signatures are searched in the mapped process image after decryption. The resolution table is not encrypted and can be verified immediately.

## Signature compatibility path

The code does not search for the short byte sequences overwritten by the hooks. Those fragments occur in many unrelated functions. It instead uses complete instruction windows for:

- the projection builder's matrix-store tail and epilogue;
- the block that seeds the camera's six CPU-side visibility planes;
- the viewport wrapper;
- the frame-start viewport call;
- the 2D-block marker viewport call;
- the instruction that publishes the display-context pointer.

The search accepts exactly one match per target. It then checks that both frame call sites call the same viewport wrapper, that the projection store remains `0x17` bytes before its epilogue, that each hook return follows the complete displaced instructions, and that derived data addresses remain within `.data`.

The projection matrix has no direct static code reference. The compatibility path derives it from the display-context block using the invariant distance observed in both maintained executables, then bounds-checks the address and retains the existing live matrix-shape check before any fallback write. If a future build reorganizes that data block, the candidate is rejected rather than guessed.

## HUD classifier

The game uses one viewport wrapper for the world, intermediate render targets and 2D presentation. The hook therefore cannot classify the interface from viewport size alone.

The code island counts calls around two verified frame markers:

- a frame with a rendered world has many calls before the 2D marker and begins banding at the third 1920-wide call after it, leaving the world composite untouched;
- a menu or cutscene has very few calls before the marker and begins banding at the first 1920-wide call;
- 256-wide shadow passes reset the post-marker count so scene-dependent shadow counts do not shift the boundary.

This is a measured classifier, not a universal semantic tag. The limitations in the README remain applicable to loading illustrations and uncommon 2D transitions.

## Safety invariants

- No patch address is accepted from the first signature match.
- Unknown or ambiguous signatures do not install a hook.
- Known builds must match both their PE profile and the independently resolved targets.
- The resolution table must match all four original `(width, height)` rows before its first write.
- All signature scans stay inside `.text` or `.rdata`; derived writable objects must stay inside `.data`.
- Both detours replace complete instructions and return to the first untouched instruction.
- The visibility detour preserves flags and the temporary SIMD register, and it is installed before the wider projection can become visible.
- Code pages remain executable while detours are installed. Temporarily
  removing execute permission from a live `.text` page can race another game
  thread and raise an access violation in an unrelated nearby instruction.
- Unit tests inspect code-island operands, branch destinations, stack restoration and signature ambiguity behavior.

## Validation record

I validated the current implementation on 3 September 2026 in three layers:

1. MSVC/Ninja build and all CTest checks passed.
2. The optional maintainer audit found each of the seven data/code locators exactly once in both locally held unpacked Steam builds and recovered the expected old and new RVA sets.
3. The current Steam build started through its exact profile and through the
   maintainer-only forced signature fallback. Both paths installed projection
   and HUD hooks at the expected targets.
4. The first late-hook candidate exposed a live-code page-permission race. The
   write helper was corrected to preserve execute permission, the automated
   checks were repeated, and a final 3440x1440 in-game pass confirmed startup
   and gameplay through the signature-aware build.
5. A maintainer-only 32:9 visibility experiment kept the physical output at
   3440x1440, forced 5120x1440 angular coverage and toggled only the horizontal
   plane seeds during a live jungle traversal. The original seeds reproduced
   intermittent lateral holes and the corrected seeds stopped the reproduction.
   A fixed-camera 3440x1440 comparison then changed only the visibility value:
   the original value rejected terrain at the left edge and the corrected value
   restored it in the next aligned frame.

Retail executables, unpacked images, dumps and private test artifacts are not part of this repository or its release archives.
