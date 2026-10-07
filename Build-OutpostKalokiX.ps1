<#
  Outpost Kaloki X builder.

  Builds the PC port on this computer from your own copy of the Xbox Live
  Arcade game. Nothing from the game is downloaded or included: the game code
  is translated and compiled here, from your file.

  Steps: check build tools (offer to install them), pick your XBLA package,
  download the ReXGlue SDK, translate the game code, compile, and put the
  finished game in the OutpostKalokiX folder.
#>
param(
    [string]$Package,                     # skip the file picker
    [string]$GameDir,                     # use game files already extracted (the launcher's updates)
    [string]$OutDir = "$PSScriptRoot\OutpostKalokiX",
    [switch]$Yes,                         # answer yes to every question
    [switch]$NoShortcut,                  # never add a desktop shortcut
    [switch]$NoLaunch                     # do not offer to start the game
)
$ErrorActionPreference = 'Stop'
$root = $PSScriptRoot
$titleId = 0x584107DB

function Say($text, $color = 'Gray') { Write-Host $text -ForegroundColor $color }
function Step($n, $text) { Write-Host ""; Write-Host " $n  $text" -ForegroundColor Green }
function Ask($question) {
    if ($Yes) { return $true }
    $a = Read-Host "$question [Y/n]"
    return ($a -eq '' -or $a -match '^[Yy]')
}
function Fail($text) { Write-Host ""; Write-Host " $text" -ForegroundColor Red; Read-Host "Press Enter to close"; exit 1 }

Clear-Host
Say ""
Say "  OUTPOST KALOKI X  -  PC port builder" 'Green'
Say "  Builds the game on this PC from your own XBLA package."
Say "  This takes about 10 to 20 minutes, most of it compiling."

# ---------------------------------------------------------------- 1. tools ---
Step 1 "Checking build tools"
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
function Find-VS { if (Test-Path $vswhere) { & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Llvm.Clang -property installationPath } }
function Have($exe, $fallback) { (Get-Command $exe -ErrorAction SilentlyContinue) -or ($fallback -and (Test-Path $fallback)) }

$missing = @()
if (-not (Find-VS)) { $missing += 'vs' }
if (-not (Have cmake "$env:ProgramFiles\CMake\bin\cmake.exe")) { $missing += 'cmake' }
if (-not (Have ninja "")) { $missing += 'ninja' }

if ($missing.Count) {
    Say "  Missing:" 'Yellow'
    if ($missing -contains 'vs') { Say "   - Visual Studio 2022 Build Tools with C++ and Clang (about 6 GB)" 'Yellow' }
    if ($missing -contains 'cmake') { Say "   - CMake" 'Yellow' }
    if ($missing -contains 'ninja') { Say "   - Ninja" 'Yellow' }
    if (-not (Get-Command winget -ErrorAction SilentlyContinue)) { Fail "winget is not available. Install the tools above yourself, then run this again." }
    if (-not (Ask "  Install them now with winget? Windows will ask for permission")) { Fail "The build tools are needed. Install them, then run this again." }
    if ($missing -contains 'cmake') { winget install --id Kitware.CMake -e --accept-package-agreements --accept-source-agreements --silent }
    if ($missing -contains 'ninja') { winget install --id Ninja-build.Ninja -e --accept-package-agreements --accept-source-agreements --silent }
    if ($missing -contains 'vs') {
        Say "  Installing Visual Studio Build Tools. This can take a while." 'Yellow'
        winget install --id Microsoft.VisualStudio.2022.BuildTools -e --accept-package-agreements --accept-source-agreements --override "--quiet --wait --norestart --nocache --add Microsoft.VisualStudio.Workload.VCTools --includeRecommended --add Microsoft.VisualStudio.Component.VC.Llvm.Clang --add Microsoft.VisualStudio.Component.VC.Llvm.ClangToolset --add Microsoft.VisualStudio.Component.Windows11SDK.26100"
    }
    $env:Path = [Environment]::GetEnvironmentVariable('Path', 'Machine') + ';' + [Environment]::GetEnvironmentVariable('Path', 'User')
    if (-not (Find-VS)) { Fail "Visual Studio Build Tools still not found. Restart your PC and run this again." }
}
if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) { $env:Path += ";$env:ProgramFiles\CMake\bin" }
Say "  Build tools ready." 'Green'

# -------------------------------------------------------------- 2. package ---
Step 2 "Choosing your Outpost Kaloki X package"
if ($GameDir) {
    if (-not (Test-Path (Join-Path $GameDir 'default.xex'))) { Fail "No game files in $GameDir." }
    Say "  Using the installed game files in $GameDir." 'Green'
} else {
    if (-not $Package) {
        Say "  Pick your XBLA package: the file with no extension from your Xbox 360 or emulator content folder."
        Add-Type -AssemblyName System.Windows.Forms
        $dlg = New-Object System.Windows.Forms.OpenFileDialog
        $dlg.Title = 'Select your Outpost Kaloki X XBLA package'
        $dlg.Filter = 'Xbox 360 package (no extension)|*.*'
        if ($dlg.ShowDialog() -ne 'OK') { Fail "No package chosen." }
        $Package = $dlg.FileName
    }
    if (-not (Test-Path $Package)) { Fail "File not found: $Package" }
    $fs = [IO.File]::OpenRead($Package); $hdr = New-Object byte[] 0x364; [void]$fs.Read($hdr, 0, $hdr.Length); $fs.Close()
    $magic = [Text.Encoding]::ASCII.GetString($hdr, 0, 4)
    $id = ([uint32]$hdr[0x360] -shl 24) -bor ([uint32]$hdr[0x361] -shl 16) -bor ([uint32]$hdr[0x362] -shl 8) -bor $hdr[0x363]
    if ($magic -notin 'LIVE', 'PIRS', 'CON ') { Fail "That file is not an Xbox 360 package." }
    if ($id -ne $titleId) { Fail ("That package is title {0:X8}, not Outpost Kaloki X ({1:X8})." -f $id, $titleId) }
    Say "  Found Outpost Kaloki X ($([IO.Path]::GetFileName($Package)))." 'Green'
}

# ------------------------------------------------- 3. extract and translate ---
Step 3 "Unpacking and translating the game code"
$source = if ($GameDir) { @{ GameDir = $GameDir } } else { @{ Package = $Package } }
try { & "$root\setup.ps1" @source } catch { Fail $_.Exception.Message }
if ($LASTEXITCODE -and $LASTEXITCODE -ne 0) { Fail "Setup failed (see above)." }

# -------------------------------------------------------------- 4. compile ---
Step 4 "Compiling (the long part)"
cmd /c "`"$root\okx\build.bat`" okx-release"
if ($LASTEXITCODE -ne 0) { Fail "Compiling failed (see above)." }

# ---------------------------------------------------------------- 5. stage ---
Step 5 "Putting the game together"
$bin = "$root\okx\out\build\okx-release"
New-Item -ItemType Directory -Force "$OutDir\game" | Out-Null
$exe = "$OutDir\outpost_kaloki_x.exe"
# An update starts this builder from the launcher, which then quits; make sure
# the old game has closed before its files are replaced.
Get-Process outpost_kaloki_x -ErrorAction SilentlyContinue | Where-Object { $_.Path -eq $exe } |
    Wait-Process -Timeout 60 -ErrorAction SilentlyContinue
Copy-Item "$bin\*.exe", "$bin\*.dll" $OutDir -Force
$sameGame = $GameDir -and ((Resolve-Path $GameDir).Path -eq (Resolve-Path "$OutDir\game").Path)
if (-not $sameGame) { Copy-Item "$root\okx\assets\*" "$OutDir\game" -Recurse -Force }
Say "  Done: $exe" 'Green'

if (-not $NoShortcut -and (Ask "  Add a desktop shortcut?")) {
    $lnk = Join-Path ([Environment]::GetFolderPath('Desktop')) 'Outpost Kaloki X.lnk'
    $sh = (New-Object -ComObject WScript.Shell).CreateShortcut($lnk)
    $sh.TargetPath = $exe; $sh.WorkingDirectory = $OutDir; $sh.Save()
    Say "  Shortcut added." 'Green'
}
Say ""
Say "  All done. Hold Shift while starting the game to open the launcher at any time." 'Green'
if (-not $NoLaunch -and (Ask "  Start Outpost Kaloki X now?")) { Start-Process $exe -WorkingDirectory $OutDir }
