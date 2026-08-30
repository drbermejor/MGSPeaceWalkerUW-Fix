param([string]$GameDir, [switch]$RemoveConfig)
. (Join-Path $PSScriptRoot "common.ps1")
$GameDir = Select-PeaceWalkerGameDir $GameDir
$managed = Join-Path $GameDir ".PeaceWalkerUltraWideFix"
Set-LauncherBypass $GameDir $false
$dll = Join-Path $GameDir "winmm.dll"
$hashMarker = Join-Path $managed "installed-winmm.sha256"
if (Test-Path -LiteralPath $dll) {
    $safe = $false
    if (Test-Path -LiteralPath $hashMarker) {
        $safe = (Get-FileHash -Algorithm SHA256 -LiteralPath $dll).Hash -eq
            (Get-Content -Raw -LiteralPath $hashMarker).Trim()
    }
    if (-not $safe) { throw "winmm.dll changed after installation; it was left untouched." }
    Remove-Item -LiteralPath $dll
}
$previous = Join-Path $managed "backup\winmm.dll.preinstall"
if (Test-Path -LiteralPath $previous) { Move-Item -LiteralPath $previous -Destination $dll }
Remove-Item -Force -LiteralPath (Join-Path $GameDir "PeaceWalkerUltraWideFix-Configure.cmd"),
    (Join-Path $GameDir "PeaceWalkerUltraWideFix-Uninstall.cmd") -ErrorAction SilentlyContinue
if ($RemoveConfig) { Remove-Item -Force -LiteralPath (Join-Path $GameDir $script:IniName) -ErrorAction SilentlyContinue }
if (Test-Path -LiteralPath $managed) { Remove-Item -Recurse -Force -LiteralPath $managed }
Write-Host "$script:ProductName removed. Configuration kept: $(-not $RemoveConfig)"
