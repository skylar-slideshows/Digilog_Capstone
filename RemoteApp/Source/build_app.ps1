# Builds a fully self-contained "Digilog Remote.exe" with PyInstaller: bundles the Python
# interpreter, all dependencies (dearpygui, pyserial, pyyaml), CONFIG.yaml, and the assets/
# folder (fonts + icon) into a single .exe so the app runs on Windows with no Python install
# at all. No installer/wizard - just the one file, run it directly.

$ErrorActionPreference = "Stop"

$ScriptDir = $PSScriptRoot
$AppName = "Digilog Remote"
$BuildDir = Join-Path $ScriptDir "build"
$DistDir = Join-Path $ScriptDir "dist"
$PythonBin = if ($env:PYTHON_BIN) { $env:PYTHON_BIN } else { "python" }

Write-Host "Building fully self-contained $AppName.exe with PyInstaller..."

Remove-Item -Recurse -Force $BuildDir -ErrorAction SilentlyContinue
Remove-Item -Recurse -Force $DistDir -ErrorAction SilentlyContinue
Remove-Item -Force (Join-Path $ScriptDir "$AppName.exe") -ErrorAction SilentlyContinue

# dearpygui's compiled extension links against the VC++ redistributable's MSVCP140.dll.
# PyInstaller's dependency scan would happily grab whatever msvcp140.dll it finds on the
# PATH/System32 and assume it's an always-present OS DLL - but on an ARM64 Windows host
# (e.g. Parallels on Apple Silicon) System32's copy is ARM64-native, not x64, so bundling
# it produces "%1 is not a valid Win32 application" on real x64 machines even though the
# rest of the exe (Python itself, dearpygui, etc.) is built x64. Use the known-x64 copy
# vendored in vendor/msvcp140.dll instead (pulled from numpy's Windows wheel, which ships
# a genuine x64 build for its OpenBLAS backend) so this works regardless of the build
# host's native CPU architecture. Must stay named exactly "msvcp140.dll" - --add-binary
# preserves the source filename, and that's the exact name _dearpygui.pyd looks up at
# runtime.
$Msvcp140 = Join-Path $ScriptDir "vendor\msvcp140.dll"
if (-not (Test-Path $Msvcp140)) {
    throw "Missing $Msvcp140 - restore vendor/msvcp140.dll (a genuine x64 MSVCP140.dll) and retry."
}

& $PythonBin -m PyInstaller `
    --noconfirm `
    --clean `
    --onefile `
    --windowed `
    --name "$AppName" `
    --icon "$ScriptDir\assets\icon.ico" `
    --add-data "$ScriptDir\config\CONFIG.yaml;config" `
    --add-data "$ScriptDir\assets;assets" `
    --add-binary "$Msvcp140;." `
    --collect-all dearpygui `
    --hidden-import serial.tools.list_ports_windows `
    --distpath "$DistDir" `
    --workpath "$BuildDir" `
    --specpath "$BuildDir" `
    "$ScriptDir\src\run.py"

if ($LASTEXITCODE -ne 0) {
    throw "PyInstaller build failed"
}

Move-Item -Force (Join-Path $DistDir "$AppName.exe") (Join-Path $ScriptDir "$AppName.exe")
Remove-Item -Recurse -Force $BuildDir -ErrorAction SilentlyContinue
Remove-Item -Recurse -Force $DistDir -ErrorAction SilentlyContinue

Write-Host "Built self-contained app: $ScriptDir\$AppName.exe"
