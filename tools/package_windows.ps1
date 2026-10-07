<#
  Builds the release configuration and packs the Windows download:
  dist\OutpostKalokiX-<version>-windows-x64.zip with OutpostKalokiX\ holding
  the program, its two DLLs, README.txt and licenses. No game files: players
  install those from their own package in the launcher.

  Needs okx\generated\default (setup.ps1 with your package) and okx\build.bat's tools.

  .\tools\package_windows.ps1 -Version v1.1.0
#>
param([Parameter(Mandatory = $true)][string]$Version)
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot

cmd /c "`"$root\okx\build.bat`" okx-release"
if ($LASTEXITCODE -ne 0) { throw "Build failed." }

$bin = "$root\okx\out\build\okx-release"
$stage = "$root\dist\OutpostKalokiX"
if (Test-Path $stage) { Get-ChildItem $stage -Recurse -File | ForEach-Object { $_.Delete() } }
New-Item -ItemType Directory -Force "$stage\licenses" | Out-Null
Copy-Item "$bin\outpost_kaloki_x.exe", "$bin\rexgpu-xenos.dll" $stage
# Always the clean rebuild of rexruntime.dll: the official v0.10.0 one is an
# antivirus false positive (see tools\rexruntime-fix\README.md).
$clean = "$root\tools\rexruntime-fix\rexruntime.dll"
if ((Get-FileHash $clean -Algorithm SHA256).Hash -ne 'E87C3555602C41B18579EF8932639F7AFC9324B0CA448F6E7608010D10EDBBC3') {
    throw "tools\rexruntime-fix\rexruntime.dll is not the expected file"
}
Copy-Item $clean $stage
Copy-Item "$root\tools\licenses\ReXGlue-LICENSE.txt" "$stage\licenses\"
Copy-Item "$root\tools\rexglue\win-amd64\licenses\SDL3\LICENSE.txt" "$stage\licenses\SDL3-LICENSE.txt"

$readme = @"
Outpost Kaloki X PC Port $Version
================================

1. Run outpost_kaloki_x.exe. The launcher opens.
2. On the Play page click "Install from package..." and pick your own
   Outpost Kaloki X Xbox Live Arcade package.
3. Press PLAY.

No game files are included. You need your own package, this exact release:

  Title ID      584107DB (Xbox Live Arcade, content type 000D0000)
  Package size  21,557,248 bytes
  Game version  default.xex 0.0.1.1 (2005-11-04), 3,252,224 bytes,
                CRC32 CCB0ACE9

The launcher checks both and tells you if your package is a different version.

Settings: outpost_kaloki_x.toml next to this program.
Saves:    Documents\outpost_kaloki_x
Problems: https://github.com/TekRantGaming/outpost-kaloki-x-recompiled/issues

Updates: the launcher checks GitHub when it opens and offers new versions
(About page: Updates).

Requires Windows 10 or 11 (64-bit) and a DirectX 12 graphics card.
Outpost Kaloki X is a game by NinjaBee. This project is not affiliated with or
endorsed by NinjaBee or Microsoft.
"@
[IO.File]::WriteAllText("$stage\README.txt", $readme.Replace("`r`n", "`n").Replace("`n", "`r`n"))

$zip = "$root\dist\OutpostKalokiX-$Version-windows-x64.zip"
if (Test-Path $zip) { [IO.File]::Delete($zip) }
# Windows' tar writes zip entries with forward slashes, which every unzip tool reads.
& "$env:SystemRoot\System32\tar.exe" -a -c -f $zip -C "$root\dist" OutpostKalokiX
if ($LASTEXITCODE -ne 0) { throw "Could not create $zip" }
Get-Item $zip | Select-Object Name, Length
(Get-FileHash $zip -Algorithm SHA256).Hash
