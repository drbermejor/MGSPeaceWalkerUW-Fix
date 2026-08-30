param([Parameter(Mandatory=$true)][string]$Dll,
      [Parameter(Mandatory=$true)][string]$Launcher,
      [Parameter(Mandatory=$true)][string]$LauncherProbe)
$ErrorActionPreference = "Stop"
$root = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$fixture = Join-Path ([IO.Path]::GetTempPath()) ("pwuwfix-" + [Guid]::NewGuid())
$game = Join-Path $fixture "MGS_PW\mgspw"
$launcherDir = Join-Path $fixture "MGS_PW\launcher"
try {
    New-Item -ItemType Directory -Force -Path $game,$launcherDir | Out-Null
    Copy-Item -LiteralPath $LauncherProbe -Destination (Join-Path $game "METAL GEAR SOLID PEACE WALKER.exe")
    Set-Content -LiteralPath (Join-Path $game "winmm.dll") -Value "previous-dll"
    Set-Content -LiteralPath (Join-Path $launcherDir "launcher.exe") -Value "original-launcher"
    $env:PWUWFIX_BINARY = (Resolve-Path $Dll).Path
    $env:PWUWFIX_LAUNCHER_BINARY = (Resolve-Path $Launcher).Path
    & (Join-Path $root "scripts\windows\setup.ps1") -GameDir $game -Profile recommended
    if (-not (Test-Path (Join-Path $game "PeaceWalkerUltraWideFix.ini"))) { throw "INI missing" }
    if ((Get-Content -Raw (Join-Path $game "PeaceWalkerUltraWideFix.ini")) -notmatch 'CenterHUD=1') { throw "recommended profile missing" }
    . (Join-Path $game ".PeaceWalkerUltraWideFix\common.ps1")
    Set-LauncherBypass $game $true
    if ((Get-FileHash $Launcher).Hash -ne (Get-FileHash (Join-Path $launcherDir "launcher.exe")).Hash) { throw "bypass not installed" }
    Start-Process -FilePath (Join-Path $launcherDir "launcher.exe") -Wait
    $arguments = Get-Content -Raw -LiteralPath (Join-Path $game "launcher-args.txt")
    foreach ($required in @('-lan en','-resolution 1','-upscale 2','-movie 1','-launcherpath launcher.exe','SteamAppId=2492660')) {
        if ($arguments -notmatch [regex]::Escape($required)) { throw "launcher protocol missing: $required" }
    }
    & (Join-Path $root "scripts\windows\uninstall.ps1") -GameDir $game
    if ((Get-Content -Raw (Join-Path $game "winmm.dll")).Trim() -ne "previous-dll") { throw "previous DLL not restored" }
    if ((Get-Content -Raw (Join-Path $launcherDir "launcher.exe")).Trim() -ne "original-launcher") { throw "launcher not restored" }
    if (-not (Test-Path (Join-Path $game "PeaceWalkerUltraWideFix.ini"))) { throw "config was not preserved" }
    Write-Host "Windows package smoke test passed."
} finally {
    Remove-Item Env:PWUWFIX_BINARY -ErrorAction SilentlyContinue
    Remove-Item Env:PWUWFIX_LAUNCHER_BINARY -ErrorAction SilentlyContinue
    if (Test-Path $fixture) { Remove-Item -Recurse -Force -LiteralPath $fixture }
}
