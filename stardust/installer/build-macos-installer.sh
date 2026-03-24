#!/bin/bash
# =============================================================================
# StarDust — macOS .pkg Installer Builder
# Creates a signed .pkg that installs:
#   - Stardust.vst3 → /Library/Audio/Plug-Ins/VST3/
#   - Stardust.app  → /Applications/
# =============================================================================

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="$PROJECT_DIR/build"
ARTEFACTS="$BUILD_DIR/StarDust_artefacts/Release"
PKG_ROOT="$BUILD_DIR/pkg-root"
PKG_OUTPUT="$BUILD_DIR/Stardust-Installer.pkg"
VERSION=$(grep 'project(StarDust VERSION' "$PROJECT_DIR/CMakeLists.txt" | sed 's/.*VERSION \([0-9.]*\)).*/\1/')

# Optional: set DEVELOPER_ID for signing (e.g. "Developer ID Installer: Your Name (TEAMID)")
SIGN_IDENTITY="${DEVELOPER_ID_INSTALLER:-}"

echo "=== StarDust macOS Installer Builder ==="
echo ""

# ---- Step 1: Build if needed ------------------------------------------------
if [ ! -d "$ARTEFACTS/VST3/Stardust.vst3" ]; then
    echo "[1/6] Building StarDust..."
    cmake -B "$BUILD_DIR" -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release "$PROJECT_DIR"
    cmake --build "$BUILD_DIR" --config Release -j"$(sysctl -n hw.ncpu)"
else
    echo "[1/6] Build artefacts found, skipping build."
fi

# ---- Step 2: Verify artefacts exist -----------------------------------------
echo "[2/6] Verifying build artefacts..."

VST3_BUNDLE="$ARTEFACTS/VST3/Stardust.vst3"
APP_BUNDLE="$ARTEFACTS/Standalone/Stardust.app"

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

# ---- Step 3: Clean up old installations --------------------------------------
echo "[3/6] Removing old installations..."

# Remove user-level VST3 copy (created by COPY_PLUGIN_AFTER_BUILD or previous installs)
USER_VST3="$HOME/Library/Audio/Plug-Ins/VST3/Stardust.vst3"
if [ -d "$USER_VST3" ]; then
    rm -rf "$USER_VST3"
    echo "  Removed user-level VST3: $USER_VST3"
fi

# Remove system-level VST3 if it exists (will be replaced by installer)
SYSTEM_VST3="/Library/Audio/Plug-Ins/VST3/Stardust.vst3"
if [ -d "$SYSTEM_VST3" ]; then
    rm -rf "$SYSTEM_VST3" 2>/dev/null || sudo rm -rf "$SYSTEM_VST3" 2>/dev/null || true
    echo "  Removed system-level VST3: $SYSTEM_VST3"
fi

# ---- Step 4: Create package root hierarchy -----------------------------------
echo "[4/6] Preparing package root..."

rm -rf "$PKG_ROOT"
mkdir -p "$PKG_ROOT/Library/Audio/Plug-Ins/VST3"
mkdir -p "$PKG_ROOT/Applications"

# Copy bundles into install locations
cp -R "$VST3_BUNDLE" "$PKG_ROOT/Library/Audio/Plug-Ins/VST3/"
cp -R "$APP_BUNDLE"  "$PKG_ROOT/Applications/"

# ---- Step 5: Build the .pkg -------------------------------------------------
echo "[5/6] Building .pkg installer..."

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
        <string>Library/Audio/Plug-Ins/VST3/Stardust.vst3</string>
    </dict>
    <dict>
        <key>BundleHasStrictIdentifier</key>
        <true/>
        <key>BundleIsRelocatable</key>
        <false/>
        <key>BundleOverwriteAction</key>
        <string>upgrade</string>
        <key>RootRelativeBundlePath</key>
        <string>Applications/Stardust.app</string>
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
    SIGNED_PKG="$BUILD_DIR/Stardust-Installer-signed.pkg"
    productsign --sign "$SIGN_IDENTITY" "$PKG_OUTPUT" "$SIGNED_PKG"
    mv "$SIGNED_PKG" "$PKG_OUTPUT"
    echo "  Package signed successfully."
fi

# ---- Step 6: Summary --------------------------------------------------------
echo "[6/6] Done!"
echo ""
echo "  Installer: $PKG_OUTPUT"
echo "  Size:      $(du -sh "$PKG_OUTPUT" | cut -f1)"
echo ""
echo "  Installs:"
echo "    /Library/Audio/Plug-Ins/VST3/Stardust.vst3"
echo "    /Applications/Stardust.app"
echo ""
echo "  To sign for distribution, set DEVELOPER_ID_INSTALLER:"
echo "    DEVELOPER_ID_INSTALLER='Developer ID Installer: Name (TEAM)' $0"
