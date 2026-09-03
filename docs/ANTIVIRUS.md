# Antivirus and release trust

Peace Walker UltraWide Fix is an open-source runtime patch. Its Windows DLL is
loaded by the game as a `winmm.dll` proxy, forwards the original multimedia
exports to the system DLL, and changes a small number of validated addresses
inside the current game process.

That combination can resemble generic malware techniques to a heuristic
scanner: the module allocates executable code islands, temporarily changes
page protection, writes detour jumps and starts initialization threads. The
implementation does not download code, create persistence, open another
process or use `WriteProcessMemory`. These claims can be checked in the source
and in the binary import table.

## What a detection means

An antivirus alert must be taken seriously, but a generic machine-learning
name alone does not establish that this project contains a trojan. It also
must not be dismissed without checking the exact file and hash.

During the 3 September 2026 compatibility work, Microsoft Defender classified
the extracted DLL from public release `v0.1.0-rc.2` as
`Trojan:Win32/Wacatac.H!ml` with security intelligence `1.459.28.0`. The new
signature-aware MSVC candidate and its complete Windows package scanned clean
with the same engine and definitions. An archive-only scan initially missed
the old DLL, so the release procedure now requires scanning the extracted
executables as well as the archive.

This is consistent with a false positive tied to the older binary pattern, but
only the antivirus vendor can make a final determination. A clean result for
one build or one definition version is not a permanent guarantee.

## Safe user response

1. Keep antivirus protection enabled and update its definitions.
2. Download only from this repository's GitHub Releases page.
3. Compare the package SHA-256 with `SHA256SUMS.txt` from the same release.
4. Do not add a broad folder exclusion. Report the release tag, exact filename,
   SHA-256 and detection name in an issue.
5. If the latest release is still blocked, wait for a reviewed replacement or
   build the tagged source yourself. Do not use a mirror or a re-uploaded DLL.

Microsoft accepts suspected false positives at the
[Security Intelligence file-submission portal](https://www.microsoft.com/wdsi/filesubmission).
Select that the file is not believed to contain malware and submit it as a
software developer. Microsoft's
[developer guidance](https://learn.microsoft.com/en-us/defender-xdr/developer-faq)
does not offer a generic allowlist; each disputed file needs a final vendor
determination.

## Provenance and signing

Release packages include SHA-256 checksums. Starting with the next published
release, GitHub Actions also generates a build-provenance attestation for every
release asset. It can be checked with:

```text
gh attestation verify <downloaded-file> -R drbermejor/MGSPeaceWalkerUW-Fix
```

An attestation proves which public repository, commit and workflow produced an
asset. It is not an Authenticode signature and does not suppress antivirus or
SmartScreen warnings. GitHub documents the verification model in
[Using artifact attestations](https://docs.github.com/en/actions/how-tos/secure-your-work/use-artifact-attestations/use-artifact-attestations).

The Windows executables are currently unsigned. Microsoft treats SmartScreen
download reputation separately from Defender Antivirus detections. A stable,
trusted Authenticode identity is the strongest future improvement for publisher
reputation, but even a valid certificate does not guarantee that a new file
will immediately avoid SmartScreen or antivirus review. Microsoft's
[SmartScreen guidance](https://learn.microsoft.com/en-us/windows/apps/package-and-deploy/smartscreen-reputation)
explains the separate file-hash and publisher-reputation signals.

## Maintainer release check

From a source checkout on a Windows machine with Microsoft Defender enabled:

```powershell
./scripts/defender_scan.ps1 `
  -Dll ./build/bin/winmm.dll `
  -Launcher ./build/bin/launcher.exe `
  -Package ./dist/PeaceWalkerUltraWideFix-<version>-windows.zip `
  -UpdateSignatures
```

The script uses a custom scan with remediation disabled so the result can be
recorded. It scans the two extracted executables independently before scanning
the package. A detection blocks publication until the exact file has been
reviewed and, when appropriate, submitted to the vendor.
