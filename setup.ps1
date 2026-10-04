<#
.SYNOPSIS
  One-time setup: downloads the ReXGlue SDK, extracts your Outpost Kaloki X
  package, and generates the recompiled C++ sources.

.EXAMPLE
  .\setup.ps1 -Package "D:\xbla\Outpost Kaloki X"
  then: okx\build.bat
#>
param(
    # Path to your own dump of the Outpost Kaloki X XBLA package (STFS "LIVE" file).
    [Parameter(Mandatory = $true)][string]$Package
)
$ErrorActionPreference = 'Stop'
$root = $PSScriptRoot
$sdkVersion = '0.10.0'
$sdkDir = Join-Path $root 'tools\rexglue'
$rexglue = Join-Path $sdkDir 'win-amd64\bin\rexglue.exe'

# 1. ReXGlue SDK (prebuilt release)
if (-not (Test-Path $rexglue)) {
    $zip = Join-Path $env:TEMP "rexglue-sdk-$sdkVersion-win-amd64.zip"
    $url = "https://github.com/rexglue/rexglue-sdk/releases/download/v$sdkVersion/rexglue-sdk-$sdkVersion-win-amd64.zip"
    Write-Host "Downloading ReXGlue SDK $sdkVersion..."
    Invoke-WebRequest -Uri $url -OutFile $zip
    Expand-Archive -Path $zip -DestinationPath $sdkDir -Force
}

# 2. Extract the game package into okx/assets
$assets = Join-Path $root 'okx\assets'
Write-Host "Extracting $Package -> $assets"
& (Join-Path $root 'tools\extract_stfs.ps1') -Package $Package -OutDir $assets
if (-not (Test-Path (Join-Path $assets 'default.xex'))) { throw "default.xex not found after extraction" }

# 3. Generate recompiled sources
Write-Host "Running rexglue codegen..."
Push-Location (Join-Path $root 'okx')
try {
    # rexglue logs to stderr; in Windows PowerShell 5.1 that would trip 'Stop', so judge by exit code.
    $ErrorActionPreference = 'Continue'
    & $rexglue codegen outpost_kaloki_x_manifest.toml 2>&1 | ForEach-Object { "$_" }
    if ($LASTEXITCODE -ne 0) { throw "codegen failed ($LASTEXITCODE)" }
} finally { Pop-Location; $ErrorActionPreference = 'Stop' }

Write-Host "`nDone. Build with: okx\build.bat   Run with: okx\run.bat"
