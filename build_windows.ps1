<#
Name:        build_windows.ps1
Description: verifies the toolchain, configures and builds Spider-Man for Windows x64, runs the
             ROM check and the frame comparison, and packages a ROM-free release zip.
Version:     0.1.0
Created:     28-10-2026
Author:      Spider-Man recompilation project

Usage:
  .\build_windows.ps1                 build and package
  .\build_windows.ps1 -NoPackage      build only
  .\build_windows.ps1 -SkipTests      build without running the comparisons
  .\build_windows.ps1 -Clean          remove build/cmake first
#>
[CmdletBinding()]
param(
    [switch]$NoPackage,
    [switch]$SkipTests,
    [switch]$Clean,
    [string]$MsysRoot = "C:\msys64",
    [string]$RomSet = ""
)

$ErrorActionPreference = "Stop"
$root = $PSScriptRoot
$build = Join-Path $root "build\cmake"
$bin = Join-Path $root "build\bin"
$dist = Join-Path $root "dist"

function Fail([string]$message) {
    Write-Host ""
    Write-Host "ERROR: $message" -ForegroundColor Red
    exit 1
}

function Need([string]$name, [string]$path) {
    if (-not (Test-Path $path)) { Fail "$name not found at $path" }
    Write-Host ("  {0,-14} {1}" -f $name, $path)
}

Write-Host "=== Spider-Man: The Videogame - Windows x64 build ===" -ForegroundColor Cyan
Write-Host ""
Write-Host "Toolchain"

$gcc = Join-Path $MsysRoot "mingw64\bin\gcc.exe"
$gxx = Join-Path $MsysRoot "mingw64\bin\g++.exe"
$sdl = Join-Path $MsysRoot "mingw64\include\SDL2\SDL.h"
Need "MinGW gcc" $gcc
Need "MinGW g++" $gxx
Need "SDL2 headers" $sdl

$python = (Get-Command python -ErrorAction SilentlyContinue).Source
if (-not $python) { Fail "Python 3 is not on PATH" }
$cmake = (Get-Command cmake -ErrorAction SilentlyContinue).Source
if (-not $cmake) { Fail "CMake is not on PATH" }
$ninja = (Get-Command ninja -ErrorAction SilentlyContinue).Source
if (-not $ninja) { Fail "Ninja is not on PATH (or use -G 'MinGW Makefiles')" }
Write-Host ("  {0,-14} {1}" -f "Python", $python)
Write-Host ("  {0,-14} {1}" -f "CMake", $cmake)
Write-Host ("  {0,-14} {1}" -f "Ninja", $ninja)

# The MinGW compiler needs its own bin directory first: other GCC/LLVM installs on PATH can load
# incompatible runtime DLLs (the linker then fails with no diagnostic).
$env:PATH = (Join-Path $MsysRoot "mingw64\bin") + ";" + (Join-Path $MsysRoot "usr\bin") + ";" + $env:PATH

if (-not $RomSet) {
    if (Test-Path (Join-Path $root "rom\spidman.zip")) {
        $RomSet = Join-Path $root "rom\spidman.zip"
    } elseif (Test-Path (Join-Path $root "rom")) {
        $RomSet = Join-Path $root "rom"
    }
}
if ($RomSet -and (Test-Path $RomSet)) {
    Write-Host ""
    Write-Host "ROM set"
    & $python (Join-Path $root "tools\verify_rom.py") $RomSet
    if ($LASTEXITCODE -ne 0) { Fail "the ROM set did not verify" }
} else {
    Write-Host ""
    Write-Host "No ROM set in rom/: the build still works, but the game needs spidman.zip at run time." -ForegroundColor Yellow
}

if ($RomSet -and (Test-Path $RomSet)) {
    Write-Host ""
    Write-Host "Translating the V60 program"
    & $python (Join-Path $root "tools\recompile.py") $RomSet
    if ($LASTEXITCODE -ne 0) { Fail "the V60 translation failed" }
}

if ($Clean -and (Test-Path $build)) {
    Write-Host ""
    Write-Host "Removing $build"
    Remove-Item $build -Recurse -Force
}

Write-Host ""
Write-Host "Configure"
& $cmake -S $root -B $build -G Ninja -DCMAKE_BUILD_TYPE=Release `
    "-DCMAKE_C_COMPILER=$gcc" "-DCMAKE_CXX_COMPILER=$gxx"
if ($LASTEXITCODE -ne 0) { Fail "CMake configuration failed" }

Write-Host ""
Write-Host "Build"
& $cmake --build $build
if ($LASTEXITCODE -ne 0) { Fail "build failed (see the messages above)" }

if (-not (Test-Path (Join-Path $bin "spidman.exe"))) { Fail "spidman.exe was not produced" }
if (-not (Test-Path (Join-Path $bin "SDL2.dll"))) {
    $dll = Join-Path $MsysRoot "mingw64\bin\SDL2.dll"
    if (Test-Path $dll) { Copy-Item $dll $bin -Force }
    else { Fail "SDL2.dll was not copied beside spidman.exe" }
}
Write-Host ""
Get-ChildItem $bin -File | Where-Object { $_.Name -match 'spidman|s32ref|oracle|SDL2' } |
    Select-Object Name, Length | Format-Table -AutoSize

if (-not $SkipTests -and $RomSet -and (Test-Path $RomSet)) {
    Write-Host "Frame comparisons"
    $env:SPIDERMAN_ROM = $RomSet
    & $python (Join-Path $root "tests\run_tests.py") --bin $bin --rom $RomSet
    if ($LASTEXITCODE -ne 0) { Fail "the frame comparisons failed" }
}

if (-not $NoPackage) {
    Write-Host ""
    Write-Host "Package (ROM-free)"
    if (Test-Path $dist) { Remove-Item $dist -Recurse -Force }
    New-Item -ItemType Directory -Path $dist | Out-Null
    $stage = Join-Path $dist "SpiderMan-Windows-x64"
    New-Item -ItemType Directory -Path $stage | Out-Null
    Copy-Item (Join-Path $bin "spidman.exe") $stage
    Copy-Item (Join-Path $bin "SDL2.dll") $stage
    foreach ($dll in "libgcc_s_seh-1.dll", "libstdc++-6.dll", "libwinpthread-1.dll") {
        $source = Join-Path $bin $dll
        if (-not (Test-Path $source)) { $source = Join-Path $MsysRoot "mingw64\bin\$dll" }
        if (Test-Path $source) { Copy-Item $source $stage }
    }
    Copy-Item (Join-Path $root "README.md") $stage
    Copy-Item (Join-Path $root "LICENSE") $stage
    Copy-Item (Join-Path $root "BUILD_STATUS.md") $stage
    New-Item -ItemType Directory -Path (Join-Path $stage "docs") | Out-Null
    Copy-Item (Join-Path $root "docs\CONTROLS.md") (Join-Path $stage "docs")
    Copy-Item (Join-Path $root "docs\ROM_ANALYSIS.md") (Join-Path $stage "docs")
    @"
Spider-Man: The Videogame - Windows x64
=======================================

This package contains no ROM and no game data. Put your own MAME spidman.zip beside
spidman.exe (or set SPIDERMAN_ROM to it) and run spidman.exe.

Controls: see docs/CONTROLS.md.
Status and known limitations: see BUILD_STATUS.md.
"@ | Set-Content -Encoding ASCII (Join-Path $stage "READ-ME-FIRST.txt")
    $zip = Join-Path $dist "SpiderMan-Windows-x64.zip"
    Compress-Archive -Path $stage -DestinationPath $zip -Force
    Remove-Item $stage -Recurse -Force
    Write-Host ("  {0}  ({1:N0} bytes)" -f $zip, (Get-Item $zip).Length)
}

Write-Host ""
Write-Host "OK: build finished." -ForegroundColor Green
Write-Host "Run: $bin\spidman.exe  (with spidman.zip beside it or SPIDERMAN_ROM set)"
