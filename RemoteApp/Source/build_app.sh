#!/bin/zsh
set -euo pipefail

# Builds a fully self-contained "Digilog Remote.app" with PyInstaller: bundles the Python
# interpreter, all dependencies (dearpygui, pyserial, pyyaml, pyobjc), CONFIG.yaml, and the
# assets/ folder (fonts + icon) so the app runs on a Mac with no Python install at all.
# This is a local testing build, not a signed/notarized release.

SCRIPT_DIR="${0:A:h}"
APP_NAME="Digilog Remote"
APP_PATH="$SCRIPT_DIR/$APP_NAME.app"
BUILD_DIR="$SCRIPT_DIR/build"
DIST_DIR="$SCRIPT_DIR/dist"
PYTHON_BIN="${PYTHON_BIN:-/opt/anaconda3/bin/python3}"

echo "Building $APP_NAME.app with PyInstaller..."

rm -rf "$APP_PATH" "$BUILD_DIR" "$DIST_DIR"

"$PYTHON_BIN" -m PyInstaller \
    --noconfirm \
    --clean \
    --onedir \
    --windowed \
    --name "$APP_NAME" \
    --icon "$SCRIPT_DIR/assets/icon.icns" \
    --add-data "$SCRIPT_DIR/config/CONFIG.yaml:config" \
    --add-data "$SCRIPT_DIR/assets:assets" \
    --collect-all dearpygui \
    --hidden-import serial.tools.list_ports_osx \
    --distpath "$DIST_DIR" \
    --workpath "$BUILD_DIR" \
    --specpath "$BUILD_DIR" \
    "$SCRIPT_DIR/src/run.py"

mv "$DIST_DIR/$APP_NAME.app" "$APP_PATH"
rm -rf "$BUILD_DIR" "$DIST_DIR"

# Match the identity previously set by hand for the shell-launcher bundle.
/usr/libexec/PlistBuddy -c "Set :CFBundleDisplayName $APP_NAME" "$APP_PATH/Contents/Info.plist" 2>/dev/null || true
/usr/libexec/PlistBuddy -c "Set :CFBundleIdentifier com.digilog.control" "$APP_PATH/Contents/Info.plist" 2>/dev/null || true

# Apple Silicon refuses to launch unsigned arm64 binaries at all, so ad-hoc sign the bundle
# (this is NOT a real Developer ID signature - Gatekeeper will still warn on other Macs the
# first time; right-click > Open, or `xattr -cr "Digilog Remote.app"`, gets past that).
echo "Ad-hoc code-signing the app bundle..."
codesign --force --deep --sign - "$APP_PATH"

echo "Application built successfully: $APP_PATH"
