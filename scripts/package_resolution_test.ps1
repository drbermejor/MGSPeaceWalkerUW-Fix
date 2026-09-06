param([string]$BuildDir = 'build-resolution-test-2',
      [string]$OutputDir = 'dist-resolution-test-2')
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$build = Join-Path $root $BuildDir
$output = Join-Path $root $OutputDir
$name = 'PeaceWalkerUltraWideFix-v0.1.0-rc.5-resolution-test.2-windows'
$stage = Join-Path $output $name
if (Test-Path -LiteralPath $output) { throw 'Output directory already exists; use a new OutputDir.' }
$dll = Join-Path $build 'bin\winmm.dll'
if (-not (Test-Path -LiteralPath $dll)) { throw 'Build the candidate first.' }
if (-not [Text.Encoding]::ASCII.GetString([IO.File]::ReadAllBytes($dll)).Contains('v0.1.0-rc.5-resolution-test.2')) { throw 'Wrong candidate DLL version.' }
New-Item -ItemType Directory -Path $stage | Out-Null
Copy-Item -LiteralPath $dll -Destination $stage
foreach ($file in @('INSTALL.txt','PeaceWalkerUltraWideFix.ini')) {
    Copy-Item -LiteralPath (Join-Path $root "config\resolution-test-2\$file") -Destination $stage
}
Copy-Item -LiteralPath (Join-Path $root 'docs\RESOLUTION_TEST_2.md') -Destination (Join-Path $stage 'VALIDATION.md')
$lines = foreach ($file in Get-ChildItem -LiteralPath $stage -File | Sort-Object Name) {
    '{0}  {1}' -f (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash.ToLowerInvariant(), $file.Name
}
[IO.File]::WriteAllText((Join-Path $stage 'SHA256SUMS.txt'), ($lines -join "`n") + "`n", [Text.UTF8Encoding]::new($false))
$zip = Join-Path $output "$name.zip"
Compress-Archive -LiteralPath (Get-ChildItem -LiteralPath $stage -File).FullName -DestinationPath $zip
# Re-extract to verify the actual transferable artifact, not only staging.
$verify = Join-Path $output 'verified-extraction'
Expand-Archive -LiteralPath $zip -DestinationPath $verify
$expected = @('INSTALL.txt','PeaceWalkerUltraWideFix.ini','SHA256SUMS.txt','VALIDATION.md','winmm.dll')
if (@(Compare-Object $expected (Get-ChildItem -LiteralPath $verify -File).Name).Count) { throw 'Unexpected ZIP contents.' }
foreach ($file in $expected) {
    if ((Get-FileHash -LiteralPath (Join-Path $stage $file)).Hash -ne
        (Get-FileHash -LiteralPath (Join-Path $verify $file)).Hash) { throw "ZIP payload differs: $file" }
}
$scanner = Join-Path $env:ProgramFiles 'Windows Defender\MpCmdRun.exe'
foreach ($target in @((Join-Path $verify 'winmm.dll'), $zip)) {
    & $scanner -Scan -ScanType 3 -File $target -DisableRemediation
    if ($LASTEXITCODE -ne 0) { throw "Defender scan did not pass: $target" }
}
$zipHash = (Get-FileHash -LiteralPath $zip -Algorithm SHA256).Hash.ToLowerInvariant()
[IO.File]::WriteAllText((Join-Path $output 'SHA256SUMS.txt'), "$zipHash  $name.zip`n", [Text.UTF8Encoding]::new($false))
Get-Item -LiteralPath $zip | Select-Object FullName,Length
Write-Output "SHA256 $zipHash"
