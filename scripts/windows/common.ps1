$ErrorActionPreference = "Stop"
$script:ProductName = "Peace Walker UltraWide Fix"
$script:ExeName = "METAL GEAR SOLID PEACE WALKER.exe"
$script:IniName = "PeaceWalkerUltraWideFix.ini"
$script:PackageRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$versionFile = Join-Path $script:PackageRoot "VERSION"
if (-not (Test-Path -LiteralPath $versionFile)) { $versionFile = Join-Path $PSScriptRoot "version" }
$script:Version = (Get-Content -Raw -LiteralPath $versionFile).Trim()

function Test-PeaceWalkerGameDir([string]$Path) {
    return $Path -and (Test-Path -LiteralPath (Join-Path $Path $script:ExeName) -PathType Leaf)
}

function Get-SteamRoots {
    $roots = [Collections.Generic.List[string]]::new()
    try {
        $steam = [string](Get-ItemProperty -LiteralPath "HKCU:\Software\Valve\Steam" `
            -Name SteamPath -ErrorAction Stop).SteamPath
        if ($steam) { $roots.Add($steam) }
    } catch {}
    if (${env:ProgramFiles(x86)}) { $roots.Add((Join-Path ${env:ProgramFiles(x86)} "Steam")) }
    if ($env:ProgramFiles) { $roots.Add((Join-Path $env:ProgramFiles "Steam")) }

    $all = [Collections.Generic.List[string]]::new()
    foreach ($root in $roots) {
        if (-not $root) { continue }
        $all.Add($root)
        $vdf = Join-Path $root "steamapps\libraryfolders.vdf"
        if (-not (Test-Path -LiteralPath $vdf)) { continue }
        foreach ($line in Get-Content -LiteralPath $vdf) {
            if ($line -match '^\s*"path"\s+"(.+)"') {
                $all.Add(($matches[1] -replace '\\\\','\'))
            }
        }
    }
    return $all | Select-Object -Unique
}

function Find-PeaceWalkerGameDir {
    foreach ($root in Get-SteamRoots) {
        $candidate = Join-Path $root "steamapps\common\MGS_PW\mgspw"
        if (Test-PeaceWalkerGameDir $candidate) { return $candidate }
    }
    return $null
}

function Select-PeaceWalkerGameDir([string]$Initial) {
    if (Test-PeaceWalkerGameDir $Initial) { return (Resolve-Path $Initial).Path }
    $found = Find-PeaceWalkerGameDir
    if ($found) { return (Resolve-Path $found).Path }
    Add-Type -AssemblyName System.Windows.Forms
    $dialog = [Windows.Forms.FolderBrowserDialog]@{
        Description = "Select the mgspw folder containing $script:ExeName"
        ShowNewFolderButton = $false
    }
    if ($dialog.ShowDialog() -ne [Windows.Forms.DialogResult]::OK -or
        -not (Test-PeaceWalkerGameDir $dialog.SelectedPath)) {
        throw "Select the mgspw folder containing $script:ExeName."
    }
    return (Resolve-Path $dialog.SelectedPath).Path
}

function Get-IniValue([string]$Path, [string]$Key, [string]$Default) {
    $section = ""
    foreach ($line in Get-Content -LiteralPath $Path -ErrorAction SilentlyContinue) {
        $trimmed = $line.Trim()
        if ($trimmed -match '^\[(.+)\]$') { $section = $matches[1]; continue }
        if ($trimmed -match '^([^;#][^=]*)=(.*)$') {
            $name = $matches[1].Trim()
            if ($name -eq $Key -and $section -in @("Fix", "Ultrawide", "Launcher")) {
                return $matches[2].Trim()
            }
        }
    }
    return $Default
}

function Write-CleanIni([string]$Path, [bool]$Enabled, [int]$Width, [int]$Height,
                        [bool]$Letterbox, [bool]$Fov, [bool]$Hud,
                        [bool]$Bypass = $false) {
    $text = @"
; Peace Walker UltraWide Fix $script:Version
; Read once when the game starts.

[Fix]
Enabled=$([int]$Enabled)
Width=$Width
Height=$Height
RemoveLetterboxing=$([int]$Letterbox)
CorrectFOV=$([int]$Fov)
CenterHUD=$([int]$Hud)

[Launcher]
BypassUnityLauncher=$([int]$Bypass)
Region=$(Get-IniValue $Path "Region" "eu")
Language=$(Get-IniValue $Path "Language" "sp")
SelfRegion=$(Get-IniValue $Path "SelfRegion" "EU")
ControllerType=$(Get-IniValue $Path "ControllerType" "XBOX")
"@
    $temporary = "$Path.tmp"
    [IO.File]::WriteAllText($temporary, $text.Replace("`r`n", "`n"),
        [Text.UTF8Encoding]::new($false))
    Move-Item -Force -LiteralPath $temporary -Destination $Path
}

function Get-LauncherPaths([string]$GameDir) {
    $install = Split-Path -Parent $GameDir
    $launcherDir = Join-Path $install "launcher"
    $managed = Join-Path $GameDir ".PeaceWalkerUltraWideFix"
    return @{
        Directory = $launcherDir
        Active = (Join-Path $launcherDir "launcher.exe")
        Payload = (Join-Path $managed "launcher-wrapper.exe")
        Backup = (Join-Path $managed "backup\launcher.exe.preinstall")
        Hash = (Join-Path $managed "installed-launcher.sha256")
    }
}

function Set-LauncherBypass([string]$GameDir, [bool]$Enabled) {
    $paths = Get-LauncherPaths $GameDir
    if ($Enabled) {
        if (-not (Test-Path -LiteralPath $paths.Payload -PathType Leaf)) {
            throw "The direct-launcher payload is missing. Run setup again."
        }
        New-Item -ItemType Directory -Force -Path $paths.Directory,
            (Split-Path -Parent $paths.Backup) | Out-Null
        $payloadHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $paths.Payload).Hash
        if (Test-Path -LiteralPath $paths.Active) {
            $activeHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $paths.Active).Hash
            if ($activeHash -ne $payloadHash) {
                if (Test-Path -LiteralPath $paths.Backup) {
                    $stamp = Get-Date -Format "yyyyMMdd-HHmmss"
                    Copy-Item -LiteralPath $paths.Active -Destination "$($paths.Backup).$stamp"
                } else {
                    Copy-Item -LiteralPath $paths.Active -Destination $paths.Backup
                }
            }
        }
        Copy-Item -Force -LiteralPath $paths.Payload -Destination $paths.Active
        $payloadHash | Set-Content -LiteralPath $paths.Hash -Encoding ascii
        return
    }

    if (-not (Test-Path -LiteralPath $paths.Active)) { return }
    if (-not (Test-Path -LiteralPath $paths.Hash)) { return }
    $activeHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $paths.Active).Hash
    $installedHash = (Get-Content -Raw -LiteralPath $paths.Hash).Trim()
    if ($activeHash -ne $installedHash) {
        Write-Warning "launcher.exe changed after installation; it was left untouched."
        return
    }
    if (Test-Path -LiteralPath $paths.Backup) {
        Copy-Item -Force -LiteralPath $paths.Backup -Destination $paths.Active
    } else {
        Remove-Item -LiteralPath $paths.Active
    }
    Remove-Item -Force -LiteralPath $paths.Hash -ErrorAction SilentlyContinue
}
