# Contributing

Contributions and reproducible bug reports are welcome. I review changes against the same requirements used for my own work.

## Required evidence

A memory patch should include:

- the behavior being changed and the smallest verified patch point;
- the original bytes or data invariant checked before writing;
- the reason each wildcard or derived address is safe;
- deterministic tests where the logic can run outside the game;
- an in-game A/B result for visual or timing behavior;
- an explicit list of screens, resolutions and builds that were not tested.

Passing compilation is necessary but does not establish that a visual patch is correct. Screenshots should be captured at native client resolution when aspect ratio is part of the claim.

## Tool-assisted work

I may use debuggers, disassemblers, scripts and AI-assisted tools during research or implementation. No generated suggestion is accepted as evidence by itself. I require an explainable invariant, source review, automated checks where possible and direct runtime validation before treating a change as supported.

Contributors may use the tools they prefer, but remain responsible for understanding and verifying the submitted code. Please submit the distilled technical result rather than chat transcripts, prompts or raw exploratory notes.

## Repository hygiene

- Do not commit game executables, unpacked images, memory dumps or proprietary assets.
- Do not commit credentials, absolute personal paths, private review logs or generated build directories.
- Keep public documentation in English and distinguish verified behavior from hypotheses.
- Use first-person singular for maintainer statements. Prefer impersonal technical prose where authorship is irrelevant.
- Keep changes focused and preserve the fail-closed behavior of every patch site.

## Release security check

Before publishing a Windows package, scan the built `winmm.dll`,
`launcher.exe` and final archive independently. Scanning only the archive is not
sufficient: a container scan can return clean while an extracted runtime DLL is
classified. The repeatable local command is documented in
[Antivirus and release trust](docs/ANTIVIRUS.md).
