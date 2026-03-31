#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="$PROJECT_DIR/stardust/build/Stardust_artefacts/Release"
STAGING="$SCRIPT_DIR/staging"
OUTPUT="$SCRIPT_DIR/Stardust-Installer.pkg"

# Plugin version
VERSION="1.0.0"

echo "=== Stardust Installer Builder ==="
echo "Build dir: $BUILD_DIR"

# Verify artifacts exist
for artifact in "$BUILD_DIR/AU/Stardust.component" "$BUILD_DIR/VST3/Stardust.vst3" "$BUILD_DIR/Standalone/Stardust.app"; do
    if [ ! -d "$artifact" ]; then
        echo "ERROR: Missing $artifact — run cmake --build first"
        exit 1
    fi
done

# Clean staging
rm -rf "$STAGING" "$OUTPUT"
mkdir -p "$STAGING/au" "$STAGING/vst3" "$STAGING/app"

# Stage artifacts into install locations
echo "Staging AU..."
mkdir -p "$STAGING/au/Library/Audio/Plug-Ins/Components"
cp -R "$BUILD_DIR/AU/Stardust.component" "$STAGING/au/Library/Audio/Plug-Ins/Components/"

echo "Staging VST3..."
mkdir -p "$STAGING/vst3/Library/Audio/Plug-Ins/VST3"
cp -R "$BUILD_DIR/VST3/Stardust.vst3" "$STAGING/vst3/Library/Audio/Plug-Ins/VST3/"

echo "Staging Standalone..."
mkdir -p "$STAGING/app/Applications"
cp -R "$BUILD_DIR/Standalone/Stardust.app" "$STAGING/app/Applications/"

# Build individual component packages
echo "Building AU package..."
pkgbuild --root "$STAGING/au" \
         --identifier "com.stardust.au" \
         --version "$VERSION" \
         --install-location "/" \
         "$STAGING/Stardust-AU.pkg"

echo "Building VST3 package..."
pkgbuild --root "$STAGING/vst3" \
         --identifier "com.stardust.vst3" \
         --version "$VERSION" \
         --install-location "/" \
         "$STAGING/Stardust-VST3.pkg"

echo "Building Standalone package..."
pkgbuild --root "$STAGING/app" \
         --identifier "com.stardust.app" \
         --version "$VERSION" \
         --install-location "/" \
         "$STAGING/Stardust-App.pkg"

# Create distribution XML for productbuild
cat > "$STAGING/distribution.xml" << 'DIST'
<?xml version="1.0" encoding="utf-8"?>
<installer-gui-script minSpecVersion="2">
    <title>Stardust</title>
    <welcome file="welcome.txt"/>
    <options customize="allow" require-scripts="false" hostArchitectures="x86_64,arm64"/>
    <choices-outline>
        <line choice="au"/>
        <line choice="vst3"/>
        <line choice="app"/>
    </choices-outline>
    <choice id="au" title="Audio Unit (AU)" description="Install Stardust AU plugin for Logic Pro, GarageBand, and other AU hosts.">
        <pkg-ref id="com.stardust.au"/>
    </choice>
    <choice id="vst3" title="VST3" description="Install Stardust VST3 plugin for Ableton Live, FL Studio, Reaper, and other VST3 hosts.">
        <pkg-ref id="com.stardust.vst3"/>
    </choice>
    <choice id="app" title="Standalone App" description="Install Stardust standalone application.">
        <pkg-ref id="com.stardust.app"/>
    </choice>
    <pkg-ref id="com.stardust.au" version="1.0.0">Stardust-AU.pkg</pkg-ref>
    <pkg-ref id="com.stardust.vst3" version="1.0.0">Stardust-VST3.pkg</pkg-ref>
    <pkg-ref id="com.stardust.app" version="1.0.0">Stardust-App.pkg</pkg-ref>
</installer-gui-script>
DIST

# Create welcome text
cat > "$STAGING/welcome.txt" << 'WELCOME'
Welcome to Stardust

A creative audio processor featuring four DSP sections you can chain,
reorder, and remove in any configuration:

  - Destroy:   SP-950 style 12-bit sampler emulation with bit-crusher,
               sample-rate reduction, and 6th-order Butterworth filter
  - Granular:  256-voice granular engine with Dattorro plate reverb,
               pitch morphing, BPM-synced grain size, and freeze
  - Multiply:  4-voice ensemble chorus with allpass phase diffusion
               and stereo spread control
  - Tape:      Jiles-Atherton hysteresis reel-to-reel emulation with
               wow, flutter, hiss, and NAB/IEC emphasis

This installer will place:
  - AU plugin in /Library/Audio/Plug-Ins/Components/
  - VST3 plugin in /Library/Audio/Plug-Ins/VST3/
  - Standalone app in /Applications/

Version 1.0.0
WELCOME

# Build final product installer
echo "Building combined installer..."
productbuild --distribution "$STAGING/distribution.xml" \
             --resources "$STAGING" \
             --package-path "$STAGING" \
             "$OUTPUT"

# Clean staging
rm -rf "$STAGING"

echo ""
echo "=== Installer built successfully ==="
echo "Output: $OUTPUT"
echo ""
ls -lh "$OUTPUT"
