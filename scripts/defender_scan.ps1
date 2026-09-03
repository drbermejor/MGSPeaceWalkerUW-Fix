param(
    [Parameter(Mandatory = $true)]
    [string]$Dll,

    [Parameter(Mandatory = $true)]
    [string]$Launcher,

    [string[]]$Package,

    [switch]$UpdateSignatures
)

$ErrorActionPreference = 'Stop'

$scanner = Join-Path $env:ProgramFiles 'Windows Defender\MpCmdRun.exe'
if (-not (Test-Path -LiteralPath $scanner)) {
    throw "Microsoft Defender command-line scanner was not found: $scanner"
}

if ($UpdateSignatures) {
    Write-Host 'Updating Microsoft Defender security intelligence...'
    Update-MpSignature
}

$targets = @(
    [pscustomobject]@{ Name = 'proxy DLL'; Path = $Dll },
    [pscustomobject]@{ Name = 'launcher'; Path = $Launcher }
)
foreach ($packagePath in $Package) {
    $targets += [pscustomobject]@{
        Name = "Windows package ($([IO.Path]::GetFileName($packagePath)))"
        Path = $packagePath
    }
}

$failed = $false
foreach ($target in $targets) {
    $resolved = (Resolve-Path -LiteralPath $target.Path).Path
    $hash = (Get-FileHash -LiteralPath $resolved -Algorithm SHA256).Hash
    Write-Host "Scanning $($target.Name): $resolved"
    Write-Host "SHA-256: $hash"
    & $scanner -Scan -ScanType 3 -File $resolved -DisableRemediation
    if ($LASTEXITCODE -ne 0) {
        $failed = $true
        Write-Error "Defender did not return a clean result for $($target.Name) (exit $LASTEXITCODE)." -ErrorAction Continue
    }
}

$status = Get-MpComputerStatus
Write-Host "Engine: $($status.AMEngineVersion)"
Write-Host "Security intelligence: $($status.AntivirusSignatureVersion)"
Write-Host "Security intelligence updated: $($status.AntivirusSignatureLastUpdated)"
Write-Host "Real-time protection: $($status.RealTimeProtectionEnabled)"

if ($failed) {
    throw 'One or more release targets did not pass Microsoft Defender.'
}

Write-Host 'All requested targets passed Microsoft Defender.'
