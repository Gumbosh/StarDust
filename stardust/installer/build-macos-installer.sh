#!/bin/bash
# =============================================================================
# Stardust — macOS .pkg Installer Builder
# Creates a signed .pkg that installs:
#   - Stardust.vst3 → /Library/Audio/Plug-Ins/VST3/
#   - Stardust.app  → /Applications/
# =============================================================================

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="$PROJECT_DIR/build"
ARTEFACTS="$BUILD_DIR/Stardust_artefacts/Release"
PKG_ROOT="$BUILD_DIR/pkg-root"
PKG_COMPONENT="$BUILD_DIR/Stardust-component.pkg"
PKG_OUTPUT="$BUILD_DIR/Stardust-Installer.pkg"
PKG_RESOURCES="$BUILD_DIR/pkg-resources"
PKG_DISTRIBUTION="$BUILD_DIR/Distribution.xml"
VERSION=$(grep 'project(Stardust VERSION' "$PROJECT_DIR/CMakeLists.txt" | sed 's/.*VERSION \([0-9.]*\)).*/\1/')

# Optional: set DEVELOPER_ID for signing (e.g. "Developer ID Installer: Your Name (TEAMID)")
SIGN_IDENTITY="${DEVELOPER_ID_INSTALLER:-}"

echo "=== Stardust macOS Installer Builder ==="
echo ""

# ---- Step 1: Always build latest source -------------------------------------
echo "[1/6] Building latest Stardust Character Engine..."
cmake -B "$BUILD_DIR" -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release "$PROJECT_DIR"
cmake --build "$BUILD_DIR" --config Release -j"$(sysctl -n hw.ncpu)"

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

rm -rf "$PKG_ROOT" "$PKG_RESOURCES" "$PKG_COMPONENT" "$PKG_DISTRIBUTION" "$PKG_OUTPUT"
mkdir -p "$PKG_ROOT/Library/Audio/Plug-Ins/VST3"
mkdir -p "$PKG_ROOT/Applications"
mkdir -p "$PKG_RESOURCES"

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

cat > "$PKG_RESOURCES/welcome.html" << 'HTML'
<!doctype html>
<html>
<body style="font-family: -apple-system, BlinkMacSystemFont, sans-serif;">
<h2>Stardust Character Engine</h2>
<p>Stardust turns GRIT and EXCITER into a fast character-shaping workflow for drums, vocals, loops, synths, and samples.</p>
<p>Choose a Flavor like Dust, Glass, Rust, Heat, Broken, or Glow, then raise the Character control to add musical color without chasing technical settings.</p>
</body>
</html>
HTML

cat > "$PKG_RESOURCES/readme.html" << 'HTML'
<!doctype html>
<html>
<body style="font-family: -apple-system, BlinkMacSystemFont, sans-serif;">
<h3>What gets installed</h3>
<ul>
  <li>Stardust.vst3 to /Library/Audio/Plug-Ins/VST3/</li>
  <li>Stardust.app to /Applications/</li>
</ul>
<h3>About this version</h3>
<p>This build packages the latest Stardust Character Engine with the new Character macro, Flavor selector, redesigned factory presets, and updated EXCITER workflow.</p>
<p>After installing, rescan plugins in your DAW if Stardust does not appear immediately.</p>
</body>
</html>
HTML

cat > "$PKG_DISTRIBUTION" << XML
<?xml version="1.0" encoding="utf-8"?>
<installer-gui-script minSpecVersion="1">
    <title>Stardust Character Engine</title>
    <organization>com.stardust</organization>
    <domains enable_anywhere="false" enable_currentUserHome="false" enable_localSystem="true"/>
    <options customize="never" require-scripts="false"/>
    <welcome file="welcome.html"/>
    <readme file="readme.html"/>
    <choices-outline>
        <line choice="stardust"/>
    </choices-outline>
    <choice id="stardust" title="Stardust Character Engine" description="Installs the Stardust VST3 plugin and standalone app.">
        <pkg-ref id="com.stardust.plugin.component"/>
    </choice>
    <pkg-ref id="com.stardust.plugin.component" version="$VERSION" onConclusion="none">Stardust-component.pkg</pkg-ref>
</installer-gui-script>
XML

pkgbuild \
    --root "$PKG_ROOT" \
    --identifier "com.stardust.plugin.component" \
    --version "$VERSION" \
    --install-location "/" \
    --component-plist "$COMPONENT_PLIST" \
    "$PKG_COMPONENT"

if [ -n "$SIGN_IDENTITY" ]; then
    echo "  Signing with: $SIGN_IDENTITY"
    productbuild \
        --distribution "$PKG_DISTRIBUTION" \
        --resources "$PKG_RESOURCES" \
        --package-path "$BUILD_DIR" \
        --sign "$SIGN_IDENTITY" \
        "$PKG_OUTPUT"
    echo "  Package signed successfully."
else
    productbuild \
        --distribution "$PKG_DISTRIBUTION" \
        --resources "$PKG_RESOURCES" \
        --package-path "$BUILD_DIR" \
        "$PKG_OUTPUT"
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
