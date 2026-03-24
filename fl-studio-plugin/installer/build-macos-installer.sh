#!/bin/bash
# =============================================================================
# StarDust — macOS .pkg Installer Builder
# Creates a signed .pkg that installs:
#   - StarDust.vst3 → /Library/Audio/Plug-Ins/VST3/
#   - StarDust.app  → /Applications/
# =============================================================================

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="$PROJECT_DIR/build"
ARTEFACTS="$BUILD_DIR/StarDust_artefacts/Release"
PKG_ROOT="$BUILD_DIR/pkg-root"
PKG_OUTPUT="$BUILD_DIR/StarDust-Installer.pkg"
VERSION="1.0.0"

# Optional: set DEVELOPER_ID for signing (e.g. "Developer ID Installer: Your Name (TEAMID)")
SIGN_IDENTITY="${DEVELOPER_ID_INSTALLER:-}"

echo "=== StarDust macOS Installer Builder ==="
echo ""

# ---- Step 1: Build if needed ------------------------------------------------
if [ ! -d "$ARTEFACTS/VST3/StarDust.vst3" ]; then
    echo "[1/5] Building StarDust..."
    cmake -B "$BUILD_DIR" -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release "$PROJECT_DIR"
    cmake --build "$BUILD_DIR" --config Release -j"$(sysctl -n hw.ncpu)"
else
    echo "[1/5] Build artefacts found, skipping build."
fi

# ---- Step 2: Verify artefacts exist -----------------------------------------
echo "[2/5] Verifying build artefacts..."

VST3_BUNDLE="$ARTEFACTS/VST3/StarDust.vst3"
APP_BUNDLE="$ARTEFACTS/Standalone/StarDust.app"

if [ ! -d "$VST3_BUNDLE" ]; then
    echo "ERROR: VST3 bundle not found at $VST3_BUNDLE"
    exit 1
fi

if [ ! -d "$APP_BUNDLE" ]; then
    echo "ERROR: Standalone app not found at $APP_BUNDLE"
    exit 1
fi

echo "  VST3: $VST3_BUNDLE"
echo "  App:  $APP_BUNDLE"

# ---- Step 3: Create package root hierarchy -----------------------------------
echo "[3/5] Preparing package root..."

rm -rf "$PKG_ROOT"
mkdir -p "$PKG_ROOT/Library/Audio/Plug-Ins/VST3"
mkdir -p "$PKG_ROOT/Applications"

# Copy bundles into install locations
cp -R "$VST3_BUNDLE" "$PKG_ROOT/Library/Audio/Plug-Ins/VST3/"
cp -R "$APP_BUNDLE"  "$PKG_ROOT/Applications/"

# ---- Step 4: Build the .pkg -------------------------------------------------
echo "[4/5] Building .pkg installer..."

# Create a component plist for customization
COMPONENT_PLIST="$BUILD_DIR/component.plist"
cat > "$COMPONENT_PLIST" << 'PLIST'
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<array>
    <dict>
        <key>BundleHasStrictIdentifier</key>
        <true/>
        <key>BundleIsRelocatable</key>
        <false/>
        <key>BundleOverwriteAction</key>
        <string>upgrade</string>
        <key>RootRelativeBundlePath</key>
        <string>Library/Audio/Plug-Ins/VST3/StarDust.vst3</string>
    </dict>
    <dict>
        <key>BundleHasStrictIdentifier</key>
        <true/>
        <key>BundleIsRelocatable</key>
        <false/>
        <key>BundleOverwriteAction</key>
        <string>upgrade</string>
        <key>RootRelativeBundlePath</key>
        <string>Applications/StarDust.app</string>
    </dict>
</array>
</plist>
PLIST

# Build the flat package
SIGN_ARGS=""
if [ -n "$SIGN_IDENTITY" ]; then
    SIGN_ARGS="--sign \"$SIGN_IDENTITY\""
    echo "  Signing with: $SIGN_IDENTITY"
fi

pkgbuild \
    --root "$PKG_ROOT" \
    --identifier "com.stardust.plugin" \
    --version "$VERSION" \
    --install-location "/" \
    --component-plist "$COMPONENT_PLIST" \
    "$PKG_OUTPUT"

# If we have a signing identity, sign the package
if [ -n "$SIGN_IDENTITY" ]; then
    SIGNED_PKG="$BUILD_DIR/StarDust-Installer-signed.pkg"
    productsign --sign "$SIGN_IDENTITY" "$PKG_OUTPUT" "$SIGNED_PKG"
    mv "$SIGNED_PKG" "$PKG_OUTPUT"
    echo "  Package signed successfully."
fi

# ---- Step 5: Summary --------------------------------------------------------
echo "[5/5] Done!"
echo ""
echo "  Installer: $PKG_OUTPUT"
echo "  Size:      $(du -sh "$PKG_OUTPUT" | cut -f1)"
echo ""
echo "  Installs:"
echo "    /Library/Audio/Plug-Ins/VST3/StarDust.vst3"
echo "    /Applications/StarDust.app"
echo ""
echo "  To sign for distribution, set DEVELOPER_ID_INSTALLER:"
echo "    DEVELOPER_ID_INSTALLER='Developer ID Installer: Name (TEAM)' $0"
