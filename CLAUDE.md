# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build Commands

```bash
# First-time CMake setup (from repo root)
cd stardust && mkdir -p build && cd build && cmake ..

# Build the plugin (from repo root)
cd stardust && cmake --build build --config Release

# Build + package the macOS installer (AU + VST3 + Standalone)
bash installer/build_installer.sh
# Output: installer/Stardust-Installer.pkg

# Open the installer
open installer/Stardust-Installer.pkg
```

Build artifacts land in `stardust/build/Stardust_artefacts/Release/`. There are no automated tests.

## Project Overview

Stardust is a JUCE-based audio effects plugin (AU/VST3/Standalone) built with C++17. It has a **slot-based FX chain** with 4 configurable slots, each assignable to one of 8 effects:

| Slot ID | Effect | Description |
|---------|--------|-------------|
| 1 | CRUSH | 12-bit BitCrusher (SP-950 emulation) + DestroyDrive saturation |
| 3 | CHORUS | Roland Juno-60 BBD ensemble (3 modes: I / II / I+II) |
| 4 | TAPE | Jiles-Atherton hysteresis reel-to-reel (RK4 solver, 5 formulations) |
| 5 | SATURATE | 6-mode distortion (Soft/Tube/Hard/Satin/Xfmr/Vari-Mu) with 8× oversampling |
| 6 | REVERB | Dattorro plate reverb (4 input diffusers, 2 feedback tanks, 14 output taps) |
| 7 | HAZE | Noise texture injection (Pink/White/Vinyl with color LP filter) |
| 8 | MULTIPLY | 4-voice unison thickener with FM/AM LFO + allpass diffusion |

Plus modulation (2 LFOs + envelope follower → 4 routing slots) and M/S stereo width.

## Source Structure

```
stardust/src/
  PluginProcessor.h/cpp   — APVTS parameters, processBlock(), preset system
  PluginEditor.h/cpp      — All UI: StardustLookAndFeel, LabeledKnob, StardustEditor
  dsp/                    — DSP modules (one class per file)
    TapeEngine            — Most complex; RK4 hysteresis, wow/flutter/hiss, emphasis, print-through
    BitCrusher            — Sample-rate + bit-depth reduction, TPDF dither, noise shaping, clock jitter
    DestroyDrive          — Asymmetric saturation with 4× oversampling
    Saturation            — 6 distortion modes with 8× oversampling, mode morphing crossfade
    ChorusEngine          — Juno-60 BBD with NE570 compander emulation
    MultiplyEngine        — 4-voice unison, Hermite-interpolated delay taps
    DattorroReverb        — Plate reverb (Griesinger/Dattorro 1997 topology)
    ModulationMatrix      — LFO/envelope→parameter router, incremental oscillators
    ButterworthFilter     — 6th-order (3 cascaded biquads) with LFO modulation
    Oscillator            — Shared incremental oscillator (complex-multiply rotation, zero trig/sample)
  gui/
    StarfieldBackground   — Animated parameter-reactive star field (Bayer dithering, 60 FPS)
    PresetLibraryPanel    — Multi-bank preset browser with search, favorites, import/export
```

## Architecture

### Signal Flow (processBlock)

`processBlock()` runs: InputGain → **4-slot FX chain** (each slot dispatched via `chainSlot{0-3}` parameter) → OutputGain → metering.

Slots are configured via APVTS choice parameters (`chainSlot0`–`chainSlot3`). Each slot can be empty (0) or assigned to any effect. The chain order is slot 0 → 1 → 2 → 3.

### APVTS Parameter System

All parameters registered in `PluginProcessor::createParameterLayout()` (~47 params). UI binds via `SliderAttachment` / `ButtonAttachment`. Audio thread reads via `getRawParameterValue()`.

Key patterns used throughout:
- `juce::SmoothedValue<float>` for click-free parameter transitions (0.01–0.02s ramps)
- Atomic mode flags for lock-free toggling between audio/GUI threads
- Power-of-2 circular buffers with bitmask indexing (tape delay, grain buffers)
- Incremental oscillators (complex-multiply rotation, zero `sin()`/`cos()` per sample) in `Oscillator.h` and throughout DSP

### Adding a New Parameter

1. Add to `createParameterLayout()` in `PluginProcessor.cpp`
2. Read in `processBlock()` via `*apvts.getRawParameterValue("paramId")`
3. Wire UI: call `setupKnob(knob, "paramId", "Label")` in the editor
4. Add to factory presets in `presets.txt`

### Adding a New Effect

1. Create `EffectName.h` in `src/dsp/` — implement `prepare()`, setters, `process()`
2. Add a new case to the FX chain switch in `processBlock()` (assign a slot ID)
3. Add parameters + UI knobs in processor/editor
4. Add source to `target_sources` in `CMakeLists.txt`

### UI (PluginEditor)

**`LabeledKnob`** — primary building block: `juce::Slider` (rotary) + `juce::Label` (title) + confirm button for inline text editing. Created via `setupKnob()` which sets per-parameter display formats (dB, %, Hz, kHz, st, ms).

**Layout** is a 2×2 grid in `resized()` with a left sidebar for the FX chain (4 rows of effect pills + mix faders + toggles). Grid coordinates: `leftX`, `rightX`, `panelTop`, `topRowBot`, `botRowTop`, `sectionH`.

**`StardustLookAndFeel`** — monochrome palette (black bg `0xFF050505`, white accent). `drawRotarySlider()` special-cases `"destroyFader"` (horizontal linear slider with RPM labels). `drawLinearSlider()` handles vertical dB faders and sidebar mix strips.

**`dimSection(toggle, {knobs...})`** — wires a toggle to enable/disable + alpha-dim a group of knobs.

**`paramToKnob`** map — used by `updateDoubleClickDefaults()` to set preset values as double-click reset targets.

### Preset System

Factory presets loaded from `presets.txt` (embedded via BinaryData). User presets in `~/Library/Application Support/Stardust/Presets/` with optional bank subdirectories. Access guarded by `std::recursive_mutex presetLock`. Grace period frame counter (`presetLoadGrace`) skips dirty detection after programmatic load.

### Realtime Safety Conventions

- `static_assert(std::atomic<float>::is_always_lock_free)` — enforced in TapeEngine
- Pre-allocated buffers in `prepareToPlay()` (8× declared block size headroom)
- No heap allocation in `processBlock()`
- Fast polynomial approximations (`fastTanh`, `fastSinh`, `langevin`) replace slow std:: math
- Oversampling stages: 2× for CRUSH section, 4× for DestroyDrive, 8× for Saturation — all using cascaded half-band decimation

### Oversampled Processing Pattern

```cpp
auto oversampledBlock = oversampling->processSamplesUp(block);
// Process at Nx rate
effect.process(oversampledBlock);
oversampling->processSamplesDown(block);
```

DSP modules that do internal oversampling (DestroyDrive 4×, Saturation 8×) handle their own up/down sampling with Hermite/Catmull-Rom interpolation and multi-stage decimation.
