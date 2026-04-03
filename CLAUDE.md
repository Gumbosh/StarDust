# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build Commands

```bash
# Build the plugin (from repo root)
cd stardust
cmake --build build --config Release

# First-time CMake setup
mkdir -p build && cd build && cmake ..

# Build + package the macOS installer (AU + VST3 + Standalone)
bash installer/build_installer.sh
# Output: installer/Stardust-Installer.pkg

# Open the installer
open installer/Stardust-Installer.pkg
```

Build artifacts land in `stardust/build/Stardust_artefacts/Release/`. There are no automated tests.

## Project Overview

Stardust is a JUCE-based audio effects plugin (AU/VST3/Standalone) with four main processing sections:

- **Destroy** — SP-950 sampler emulation (12-bit BitCrusher + DestroyDrive + Saturation + Butterworth filter)
- **Granular** — 256-voice granular engine with integrated Dattorro plate reverb
- **Multiply** — 4-voice ensemble chorus with allpass phase diffusion
- **Tape** — Jiles-Atherton hysteresis reel-to-reel emulation with wow/flutter/hiss

## Source Structure

```
stardust/src/
  PluginProcessor.h/cpp   — APVTS parameters, processBlock(), preset system
  PluginEditor.h/cpp      — All UI: StardustLookAndFeel, LabeledKnob, StardustEditor
  dsp/                    — DSP modules (one class per file)
  gui/                    — StarfieldBackground, PresetLibraryPanel
```

## Architecture

### Audio Processing (PluginProcessor)

`processBlock()` runs the chain: InputGain → Destroy (optional) → Granular (optional) → Multiply/Chorus (optional) → Tape (optional) → OutputGain → metering.

All parameters are APVTS (`processorRef.apvts`). Use `SliderAttachment`, `ButtonAttachment` etc. to bind UI controls. Parameters are registered in `PluginProcessor::createParameterLayout()`.

Key patterns used throughout:
- `juce::SmoothedValue<float>` for click-free parameter transitions
- Atomic mode flags (destroyEnabled, granularEnabled, etc.) for lock-free toggling
- Power-of-2 circular buffers with bitmask indexing in grain/tape delay lines
- Incremental oscillators (zero trig per sample) in `Oscillator.h` and throughout DSP modules

### UI (PluginEditor)

**`LabeledKnob`** (defined in `PluginEditor.h`) is a `juce::Component` subclass containing a `juce::Slider` (rotary) + `juce::Label` (section title) + `juce::TextButton confirmBtn` (shown during value text editing). It is the primary building block for all parameter controls.

**`setupKnob(knob, paramId, labelText)`** — creates APVTS `SliderAttachment`, sets custom `textFromValueFunction` / `valueFromTextFunction` for each param's display format (dB, %, Hz, kHz, st, ms), and wires the `✓` confirm button for inline text editing (double-click on value → type → Enter or ✓).

**`layoutKnobInBounds(knob, bounds)`** — calls `knob.setBounds(bounds)`; `LabeledKnob::resized()` handles the internal 14px label-on-top + slider-below split.

**Layout** is a 2×2 equal grid drawn in `resized()`:
- Top-left: TAPE | Top-right: GRANULAR
- Bottom-left: DESTROY | Bottom-right: MULTIPLY
- Vertical divider is drawn only through the top half (bottom row is one unified band)
- Grid coordinates: `leftX`, `rightX`, `panelTop`, `topRowBot`, `botRowTop`, `sectionH`

**`drawRotarySlider`** in `StardustLookAndFeel` draws all knobs. Special-cased on `slider.getName()`:
- `"destroyFader"` — no tick marks, direction reversed (CW = higher sample rate)
- All others — 3 generic tick marks at 0%, 50%, 100% of arc

**`dimSection(toggle, {knobs...})`** — lambda that wires a `ToggleButton` to enable/disable + alpha-dim a group of `LabeledKnob*`.

**`paramToKnob`** — `std::map<String, LabeledKnob*>` used by `updateDoubleClickDefaults()` to set preset values as double-click reset targets.

### Preset System

Factory presets are loaded from `presets.txt` (embedded via BinaryData). User presets live in `~/Library/Application Support/Stardust/Presets/` (with optional bank subdirectories). Access is guarded by a `juce::RecursiveMutex`. Preset loading uses a "grace period" frame counter to skip dirty-state detection immediately after load.

### DSP Modules

- **`TapeEngine`** — most complex module; uses RK4 ODE to solve Jiles-Atherton hysteresis with 4× oversampling, per-channel PRNG for wow/flutter decorrelation, NAB/IEC pre/de-emphasis biquads.
- **`GranularEngine`** — up to 32 concurrent grains, 2¹⁷-sample circular buffer, optional HQ sinc interpolation, integrated `DattorroReverb` for space.
- **`BitCrusher`** — sample-rate reduction (continuous log-skewed range 250Hz–96kHz via `destroyFader`) + bit-depth reduction + jitter. Pre-S&H 4-pole Butterworth AA, TPDF dither, C10 bass-shelf pair, DC blocker.
- **`ModulationMatrix`** — 4-slot LFO/envelope→parameter router; incremental oscillators only (no `sin()`/`cos()` per sample).
