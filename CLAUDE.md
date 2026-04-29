# CLAUDE.md

This file provides guidance to coding agents working in this repository.

## Build Commands

```bash
# First-time CMake setup from the repo root
cd stardust && mkdir -p build && cd build && cmake ..

# Build the plugin from the repo root
cd stardust && cmake --build build --config Release

# Build and package the macOS installer
bash installer/build_installer.sh
```

The macOS installer is created at `installer/Stardust-Installer.pkg`. Build artifacts land in `stardust/build/Stardust_artefacts/Release/`. There are no automated tests.

## Project Overview

Stardust is a JUCE-based C++17 audio plugin available as AU, VST3, and standalone app. The release audio path is intentionally small:

1. GRIT: native-rate bit crushing, sample-rate reduction, jitter, and soft drive.
2. EXCITER: focused high-band harmonic enhancement.
3. Character macro: blends curated Dust, Glass, Rust, Heat, Broken, and Glow flavor targets into GRIT and EXCITER.
4. Output safety limiter and metering.

The old slot-based multi-effect architecture has been removed from the release build. Do not reintroduce legacy effect parameters unless the DSP and UI are intentionally restored together.

## Source Structure

```text
stardust/src/
  PluginProcessor.h/cpp       APVTS parameters, processBlock(), preset system
  PluginEditor.h/cpp          UI, look and feel, controls, preset browser wiring
  dsp/
    BitCrusher                GRIT sample-rate/bit-depth reduction
    HarmonicExciter           EXCITER harmonic generation
    FastMath                  Fast tanh approximations used by live DSP
  gui/
    StarfieldBackground       Parameter-reactive visualizer for GRIT/EXCITER
    PresetLibraryPanel        Factory/user preset browser
  presets/
    FactoryPresets            Init plus curated flavor-bank presets
```

## Parameters

All live release parameters are registered in `PluginProcessor::createParameterLayout()`:

- `characterMode`, `characterAmount`
- `destroyIn`, `destroyMix`, `destroyFader`, `destroyBits`, `destroyJitter`
- `exciterDrive`, `exciterTone`, `exciterMix`

When adding a parameter, update all of these places together:

1. `createParameterLayout()` in `PluginProcessor.cpp`
2. `processBlock()` if it affects audio
3. `setupKnob()` / UI layout in `PluginEditor.cpp`
4. `defaultPresetValues()` and factory preset overrides in `FactoryPresets.h/cpp`
5. `StarfieldBackground` if the visualizer should react to it

## Presets

Factory presets are compiled from `stardust/src/presets/FactoryPresets.cpp`. User presets are XML files stored under `~/Library/Application Support/Stardust/Presets/`, with optional bank subdirectories. The preset XML tag `StardustPreset` and user preset directory are release-facing persistence points; do not rename them without a migration plan.

## Realtime Safety

- Do not allocate memory in `processBlock()`.
- Use preallocated buffers for dry/wet blends and temporary audio data.
- Read APVTS values through `getRawParameterValue()` on the audio thread.
- Keep smoothing for user-controlled continuous parameters that can change during playback.
- Keep the limiter as a final guard against internal runaway values.

## Release Packaging

The canonical macOS packaging path is `installer/build_installer.sh`; it builds AU, VST3, and standalone artifacts before packaging. The Windows Inno Setup script is `stardust/installer/Stardust-installer.iss`, and `stardust/installer/build-windows-installer.bat` invokes that exact filename.
