param([string]$BuildDir = 'build-resolution-test-2')
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$installation = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $installation) { throw 'MSVC C++ tools not found.' }
& (Join-Path $installation 'Common7\Tools\Launch-VsDevShell.ps1') -Arch amd64 -HostArch amd64 -SkipAutomaticLocation
Push-Location $root
try {
    cmake -S . -B $BuildDir -G Ninja -DCMAKE_BUILD_TYPE=Release '-DPWUWFIX_RELEASE=v0.1.0-rc.5-resolution-test.2'
    if ($LASTEXITCODE) { throw 'Configure failed.' }
    cmake --build $BuildDir
    if ($LASTEXITCODE) { throw 'Build failed.' }
    ctest --test-dir $BuildDir --output-on-failure
    if ($LASTEXITCODE) { throw 'Tests failed.' }
} finally { Pop-Location }
