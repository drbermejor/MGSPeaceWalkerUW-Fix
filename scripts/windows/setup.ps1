param(
    [string]$GameDir,
    [ValidateSet("recommended", "full-width-ui", "disabled")]
    [string]$Profile,
    [switch]$SkipConfigure
)
. (Join-Path $PSScriptRoot "common.ps1")
$GameDir = Select-PeaceWalkerGameDir $GameDir
$sourceDll = if ($env:PWUWFIX_BINARY) { $env:PWUWFIX_BINARY } else { Join-Path $script:PackageRoot "bin\winmm.dll" }
if (-not (Test-Path -LiteralPath $sourceDll -PathType Leaf)) {
    throw "Package payload is incomplete: bin\winmm.dll is missing."
}
$sourceLauncher = if ($env:PWUWFIX_LAUNCHER_BINARY) { $env:PWUWFIX_LAUNCHER_BINARY } else { Join-Path $script:PackageRoot "bin\launcher.exe" }
if (-not (Test-Path -LiteralPath $sourceLauncher -PathType Leaf)) {
    throw "Package payload is incomplete: bin\launcher.exe is missing."
}
$sourceIni = Join-Path $script:PackageRoot "config\$script:IniName"
$targetDll = Join-Path $GameDir "winmm.dll"
$targetIni = Join-Path $GameDir $script:IniName
$managed = Join-Path $GameDir ".PeaceWalkerUltraWideFix"
$backup = Join-Path $managed "backup"
New-Item -ItemType Directory -Force -Path $backup | Out-Null

if (Test-Path -LiteralPath $targetDll) {
    $sourceHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $sourceDll).Hash
    $targetHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $targetDll).Hash
    if ($sourceHash -ne $targetHash -and -not (Test-Path -LiteralPath (Join-Path $backup "winmm.dll.preinstall"))) {
        Copy-Item -LiteralPath $targetDll -Destination (Join-Path $backup "winmm.dll.preinstall")
    }
}
Copy-Item -Force -LiteralPath $sourceDll -Destination $targetDll
(Get-FileHash -Algorithm SHA256 -LiteralPath $targetDll).Hash |
    Set-Content -LiteralPath (Join-Path $managed "installed-winmm.sha256") -Encoding ascii
$script:Version | Set-Content -LiteralPath (Join-Path $managed "version") -Encoding ascii
Copy-Item -Force -LiteralPath $sourceLauncher -Destination (Join-Path $managed "launcher-wrapper.exe")
Copy-Item -Force -LiteralPath $sourceIni -Destination (Join-Path $managed "default.ini")

if (-not (Test-Path -LiteralPath $targetIni)) {
    $legacy = Join-Path $GameDir "pw_ultrawide.ini"
    if (Test-Path -LiteralPath $legacy) { Copy-Item -LiteralPath $legacy -Destination $targetIni }
    else { Copy-Item -LiteralPath $sourceIni -Destination $targetIni }
}
Copy-Item -Force -LiteralPath (Join-Path $PSScriptRoot "common.ps1"),
    (Join-Path $PSScriptRoot "configure.ps1"),(Join-Path $PSScriptRoot "uninstall.ps1") -Destination $managed
@"
@echo off
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0.PeaceWalkerUltraWideFix\configure.ps1" -GameDir "%~dp0"
"@ | Set-Content -LiteralPath (Join-Path $GameDir "PeaceWalkerUltraWideFix-Configure.cmd") -Encoding ascii
@"
@echo off
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0.PeaceWalkerUltraWideFix\uninstall.ps1" -GameDir "%~dp0"
"@ | Set-Content -LiteralPath (Join-Path $GameDir "PeaceWalkerUltraWideFix-Uninstall.cmd") -Encoding ascii

Write-Host "Installed $script:ProductName $script:Version in: $GameDir"
if ($Profile) {
    & (Join-Path $PSScriptRoot "configure.ps1") -GameDir $GameDir -Profile $Profile
} elseif (-not $SkipConfigure) {
    & (Join-Path $PSScriptRoot "configure.ps1") -GameDir $GameDir
}
