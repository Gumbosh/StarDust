#include "FactoryPresets.h"

std::vector<Preset> getAbstractPresets()
{
    const juce::String bank = "Abstract";
    return {

        // ── Portal-inspired: warped, otherworldly textures ──────────────

        makePreset("Portal Open", bank, {
            {"chainSlot0", 13.0f},   // HALFTIME
            {"chainSlot1", 6.0f},    // REVERB
            {"halftimeEnabled", 1.0f},
            {"halftimeDivision", 5.0f},  // 1 Bar
            {"halftimeSpeed", 0.0f},     // 2x
            {"halftimeFade", 0.25f},
            {"halftimeMix", 0.75f},
            {"reverbEnabled", 1.0f},
            {"reverbDecay", 0.92f},
            {"reverbSize", 0.85f},
            {"reverbDamp", 0.3f},
            {"reverbPreDelay", 45.0f},
            {"reverbDiffusion", 0.9f},
            {"reverbMix", 0.55f},
        }),

        makePreset("Event Horizon", bank, {
            {"chainSlot0", 13.0f},   // HALFTIME
            {"chainSlot1", 11.0f},   // SHIFT
            {"chainSlot2", 6.0f},    // REVERB
            {"halftimeEnabled", 1.0f},
            {"halftimeDivision", 4.0f},  // 1/2
            {"halftimeSpeed", 1.0f},     // 4x (quarter speed)
            {"halftimeFade", 0.35f},
            {"halftimeMix", 0.85f},
            {"shiftEnabled", 1.0f},
            {"shiftPitch", 12.0f},       // octave up to compensate
            {"shiftFeedback", 0.2f},
            {"shiftTone", 6000.0f},
            {"shiftMix", 0.4f},
            {"reverbEnabled", 1.0f},
            {"reverbDecay", 0.95f},
            {"reverbSize", 0.9f},
            {"reverbDamp", 0.25f},
            {"reverbPreDelay", 60.0f},
            {"reverbDiffusion", 0.85f},
            {"reverbMix", 0.5f},
        }),

        makePreset("Wormhole", bank, {
            {"chainSlot0", 13.0f},   // HALFTIME
            {"chainSlot1", 9.0f},    // GRAIN
            {"chainSlot2", 6.0f},    // REVERB
            {"halftimeEnabled", 1.0f},
            {"halftimeDivision", 3.0f},  // 1/4
            {"halftimeSpeed", 0.0f},     // 2x
            {"halftimeFade", 0.2f},
            {"halftimeMix", 0.6f},
            {"grainEnabled", 1.0f},
            {"grainSize", 80.0f},
            {"grainDensity", 6.0f},
            {"grainPitch", 7.0f},        // fifth up
            {"grainScatter", 0.7f},
            {"grainReverse", 0.3f},
            {"grainMix", 0.35f},
            {"reverbEnabled", 1.0f},
            {"reverbDecay", 0.88f},
            {"reverbSize", 0.7f},
            {"reverbDamp", 0.4f},
            {"reverbPreDelay", 25.0f},
            {"reverbMix", 0.45f},
        }),

        makePreset("Singularity", bank, {
            {"chainSlot0", 13.0f},   // HALFTIME
            {"chainSlot1", 5.0f},    // SATURATE
            {"chainSlot2", 6.0f},    // REVERB
            {"halftimeEnabled", 1.0f},
            {"halftimeDivision", 6.0f},  // 2 Bars
            {"halftimeSpeed", 1.0f},     // 4x
            {"halftimeFade", 0.4f},
            {"halftimeMix", 0.9f},
            {"distortionEnabled", 1.0f},
            {"distortionDrive", 0.35f},
            {"distortionTone", 0.3f},
            {"distortionMode", 0.0f},    // Soft
            {"distortionMix", 0.6f},
            {"reverbEnabled", 1.0f},
            {"reverbDecay", 0.97f},
            {"reverbSize", 0.95f},
            {"reverbDamp", 0.15f},
            {"reverbPreDelay", 80.0f},
            {"reverbDiffusion", 0.95f},
            {"reverbMix", 0.6f},
        }),

        makePreset("Dimensional Rift", bank, {
            {"chainSlot0", 12.0f},   // REVERSER
            {"chainSlot1", 13.0f},   // HALFTIME
            {"chainSlot2", 7.0f},    // HAZE
            {"chainSlot3", 6.0f},    // REVERB
            {"reverserEnabled", 1.0f},
            {"reverserRepeat", 2.0f},
            {"reverserDivision", 5.0f},  // 1/8
            {"reverserCrossfade", 0.3f},
            {"reverserMix", 0.5f},
            {"halftimeEnabled", 1.0f},
            {"halftimeDivision", 4.0f},  // 1/2
            {"halftimeSpeed", 0.0f},     // 2x
            {"halftimeFade", 0.2f},
            {"halftimeMix", 0.65f},
            {"hazeEnabled", 1.0f},
            {"hazeColor", 0.3f},
            {"hazeType", 2.0f},          // Vinyl
            {"hazeMix", 0.08f},
            {"reverbEnabled", 1.0f},
            {"reverbDecay", 0.9f},
            {"reverbSize", 0.8f},
            {"reverbDamp", 0.35f},
            {"reverbMix", 0.45f},
        }),

        // ── Textural abstractions ───────────────────────────────────────

        makePreset("Frozen Light", bank, {
            {"chainSlot0", 9.0f},    // GRAIN
            {"chainSlot1", 6.0f},    // REVERB
            {"grainEnabled", 1.0f},
            {"grainSize", 150.0f},
            {"grainDensity", 2.0f},
            {"grainPitch", 0.0f},
            {"grainScatter", 0.9f},
            {"grainReverse", 0.5f},
            {"grainMix", 0.8f},
            {"reverbEnabled", 1.0f},
            {"reverbDecay", 0.96f},
            {"reverbSize", 0.95f},
            {"reverbDamp", 0.2f},
            {"reverbPreDelay", 30.0f},
            {"reverbDiffusion", 0.9f},
            {"reverbMix", 0.65f},
        }),

        makePreset("Ghost Frequency", bank, {
            {"chainSlot0", 11.0f},   // SHIFT
            {"chainSlot1", 12.0f},   // REVERSER
            {"chainSlot2", 6.0f},    // REVERB
            {"shiftEnabled", 1.0f},
            {"shiftPitch", -12.0f},  // octave down
            {"shiftFeedback", 0.4f},
            {"shiftTone", 3000.0f},
            {"shiftMix", 0.55f},
            {"reverserEnabled", 1.0f},
            {"reverserRepeat", 4.0f},
            {"reverserDivision", 3.0f},  // 1/16
            {"reverserCrossfade", 0.25f},
            {"reverserMix", 0.4f},
            {"reverbEnabled", 1.0f},
            {"reverbDecay", 0.85f},
            {"reverbSize", 0.6f},
            {"reverbDamp", 0.5f},
            {"reverbPreDelay", 15.0f},
            {"reverbMix", 0.5f},
        }),

        makePreset("Time Dilation", bank, {
            {"chainSlot0", 13.0f},   // HALFTIME
            {"chainSlot1", 3.0f},    // CHORUS
            {"chainSlot2", 6.0f},    // REVERB
            {"halftimeEnabled", 1.0f},
            {"halftimeDivision", 5.0f},  // 1 Bar
            {"halftimeSpeed", 0.0f},     // 2x
            {"halftimeFade", 0.3f},
            {"halftimeMix", 0.7f},
            {"chorusMix", 0.6f},
            {"junoMode", 2.0f},          // I+II
            {"reverbEnabled", 1.0f},
            {"reverbDecay", 0.8f},
            {"reverbSize", 0.65f},
            {"reverbDamp", 0.45f},
            {"reverbPreDelay", 20.0f},
            {"reverbMix", 0.4f},
        }),

        makePreset("Nebula Drift", bank, {
            {"chainSlot0", 8.0f},    // MULTIPLY
            {"chainSlot1", 13.0f},   // HALFTIME
            {"chainSlot2", 7.0f},    // HAZE
            {"chainSlot3", 6.0f},    // REVERB
            {"unisonEnabled", 1.0f},
            {"unisonSpeed", 0.6f},
            {"unisonOuter", 0.7f},
            {"unisonInner", 0.5f},
            {"unisonMix", 0.45f},
            {"halftimeEnabled", 1.0f},
            {"halftimeDivision", 4.0f},  // 1/2
            {"halftimeSpeed", 0.0f},     // 2x
            {"halftimeFade", 0.25f},
            {"halftimeMix", 0.55f},
            {"hazeEnabled", 1.0f},
            {"hazeColor", 0.6f},
            {"hazeType", 0.0f},          // Pink
            {"hazeMix", 0.06f},
            {"reverbEnabled", 1.0f},
            {"reverbDecay", 0.93f},
            {"reverbSize", 0.85f},
            {"reverbDamp", 0.3f},
            {"reverbPreDelay", 50.0f},
            {"reverbMix", 0.55f},
        }),

        makePreset("Dark Matter", bank, {
            {"chainSlot0", 13.0f},   // HALFTIME
            {"chainSlot1", 4.0f},    // TAPE
            {"chainSlot2", 6.0f},    // REVERB
            {"halftimeEnabled", 1.0f},
            {"halftimeDivision", 3.0f},  // 1/4
            {"halftimeSpeed", 1.0f},     // 4x
            {"halftimeFade", 0.3f},
            {"halftimeMix", 0.8f},
            {"tapeEnabled", 1.0f},
            {"tapeDrive", 0.4f},
            {"tapeGlue", 0.6f},
            {"tapeNoise", 0.12f},
            {"tapeWow", 0.15f},
            {"tapeMix", 0.7f},
            {"reverbEnabled", 1.0f},
            {"reverbDecay", 0.88f},
            {"reverbSize", 0.75f},
            {"reverbDamp", 0.5f},
            {"reverbMix", 0.4f},
        }),

        // ── Glitchy abstractions ────────────────────────────────────────

        makePreset("Quantum Stutter", bank, {
            {"chainSlot0", 10.0f},   // STUTTER
            {"chainSlot1", 11.0f},   // SHIFT
            {"chainSlot2", 6.0f},    // REVERB
            {"stutterEnabled", 1.0f},
            {"stutterRate", 200.0f},
            {"stutterDecay", 0.6f},
            {"stutterDepth", 0.8f},
            {"stutterMix", 0.65f},
            {"shiftEnabled", 1.0f},
            {"shiftPitch", 5.0f},    // fourth up
            {"shiftFeedback", 0.35f},
            {"shiftTone", 8000.0f},
            {"shiftMix", 0.3f},
            {"reverbEnabled", 1.0f},
            {"reverbDecay", 0.82f},
            {"reverbSize", 0.55f},
            {"reverbDamp", 0.6f},
            {"reverbMix", 0.35f},
        }),

        makePreset("Fractal Echo", bank, {
            {"chainSlot0", 11.0f},   // SHIFT
            {"chainSlot1", 9.0f},    // GRAIN
            {"chainSlot2", 13.0f},   // HALFTIME
            {"shiftEnabled", 1.0f},
            {"shiftPitch", 7.0f},    // fifth up
            {"shiftFeedback", 0.55f},
            {"shiftTone", 5000.0f},
            {"shiftMix", 0.45f},
            {"grainEnabled", 1.0f},
            {"grainSize", 30.0f},
            {"grainDensity", 8.0f},
            {"grainPitch", -5.0f},
            {"grainScatter", 0.6f},
            {"grainMix", 0.35f},
            {"halftimeEnabled", 1.0f},
            {"halftimeDivision", 2.0f},  // 1/8
            {"halftimeSpeed", 0.0f},     // 2x
            {"halftimeFade", 0.2f},
            {"halftimeMix", 0.5f},
        }),

        makePreset("Void Collapse", bank, {
            {"chainSlot0", 1.0f},    // CRUSH
            {"chainSlot1", 13.0f},   // HALFTIME
            {"chainSlot2", 6.0f},    // REVERB
            {"destroyEnabled", 1.0f},
            {"destroyFader", 8000.0f},
            {"destroyBits", 8.0f},
            {"destroyJitter", 0.3f},
            {"destroyMix", 0.5f},
            {"halftimeEnabled", 1.0f},
            {"halftimeDivision", 4.0f},  // 1/2
            {"halftimeSpeed", 1.0f},     // 4x
            {"halftimeFade", 0.35f},
            {"halftimeMix", 0.75f},
            {"reverbEnabled", 1.0f},
            {"reverbDecay", 0.9f},
            {"reverbSize", 0.8f},
            {"reverbDamp", 0.4f},
            {"reverbPreDelay", 35.0f},
            {"reverbMix", 0.5f},
        }),

        makePreset("Particle Cloud", bank, {
            {"chainSlot0", 9.0f},    // GRAIN
            {"chainSlot1", 8.0f},    // MULTIPLY
            {"chainSlot2", 6.0f},    // REVERB
            {"grainEnabled", 1.0f},
            {"grainSize", 20.0f},
            {"grainDensity", 12.0f},
            {"grainPitch", 0.0f},
            {"grainScatter", 0.85f},
            {"grainReverse", 0.4f},
            {"grainMix", 0.7f},
            {"unisonEnabled", 1.0f},
            {"unisonSpeed", 1.2f},
            {"unisonOuter", 0.9f},
            {"unisonInner", 0.6f},
            {"unisonMix", 0.4f},
            {"reverbEnabled", 1.0f},
            {"reverbDecay", 0.94f},
            {"reverbSize", 0.9f},
            {"reverbDamp", 0.25f},
            {"reverbPreDelay", 40.0f},
            {"reverbDiffusion", 0.85f},
            {"reverbMix", 0.6f},
        }),

        makePreset("Phase Shift", bank, {
            {"chainSlot0", 13.0f},   // HALFTIME
            {"chainSlot1", 12.0f},   // REVERSER
            {"halftimeEnabled", 1.0f},
            {"halftimeDivision", 2.0f},  // 1/8
            {"halftimeSpeed", 0.0f},     // 2x
            {"halftimeFade", 0.15f},
            {"halftimeMix", 0.6f},
            {"reverserEnabled", 1.0f},
            {"reverserRepeat", 8.0f},
            {"reverserDivision", 2.0f},  // 1/16T
            {"reverserCrossfade", 0.2f},
            {"reverserMix", 0.35f},
        }),

        // ── Cinematic / ambient abstractions ────────────────────────────

        makePreset("Astral Projection", bank, {
            {"chainSlot0", 11.0f},   // SHIFT
            {"chainSlot1", 13.0f},   // HALFTIME
            {"chainSlot2", 3.0f},    // CHORUS
            {"chainSlot3", 6.0f},    // REVERB
            {"shiftEnabled", 1.0f},
            {"shiftPitch", -7.0f},   // fifth down
            {"shiftFeedback", 0.15f},
            {"shiftTone", 4500.0f},
            {"shiftMix", 0.5f},
            {"halftimeEnabled", 1.0f},
            {"halftimeDivision", 5.0f},  // 1 Bar
            {"halftimeSpeed", 0.0f},     // 2x
            {"halftimeFade", 0.3f},
            {"halftimeMix", 0.6f},
            {"chorusMix", 0.5f},
            {"junoMode", 1.0f},          // II
            {"reverbEnabled", 1.0f},
            {"reverbDecay", 0.94f},
            {"reverbSize", 0.85f},
            {"reverbDamp", 0.2f},
            {"reverbPreDelay", 55.0f},
            {"reverbDiffusion", 0.9f},
            {"reverbWidth", 1.0f},
            {"reverbMix", 0.55f},
        }),

        makePreset("Subspace", bank, {
            {"chainSlot0", 13.0f},   // HALFTIME
            {"chainSlot1", 5.0f},    // SATURATE
            {"chainSlot2", 7.0f},    // HAZE
            {"halftimeEnabled", 1.0f},
            {"halftimeDivision", 7.0f},  // 4 Bars
            {"halftimeSpeed", 1.0f},     // 4x
            {"halftimeFade", 0.5f},
            {"halftimeMix", 0.9f},
            {"distortionEnabled", 1.0f},
            {"distortionDrive", 0.2f},
            {"distortionTone", 0.4f},
            {"distortionMode", 1.0f},    // Tube
            {"distortionMix", 0.45f},
            {"hazeEnabled", 1.0f},
            {"hazeColor", 0.7f},
            {"hazeType", 2.0f},          // Vinyl
            {"hazeMix", 0.1f},
        }),

        makePreset("Inverted Reality", bank, {
            {"chainSlot0", 12.0f},   // REVERSER
            {"chainSlot1", 11.0f},   // SHIFT
            {"chainSlot2", 9.0f},    // GRAIN
            {"chainSlot3", 6.0f},    // REVERB
            {"reverserEnabled", 1.0f},
            {"reverserRepeat", 1.0f},
            {"reverserDivision", 7.0f},  // 1/4
            {"reverserCrossfade", 0.4f},
            {"reverserMix", 0.7f},
            {"shiftEnabled", 1.0f},
            {"shiftPitch", 12.0f},       // octave up
            {"shiftFeedback", 0.3f},
            {"shiftTone", 7000.0f},
            {"shiftMix", 0.35f},
            {"grainEnabled", 1.0f},
            {"grainSize", 60.0f},
            {"grainDensity", 5.0f},
            {"grainPitch", -12.0f},      // octave down
            {"grainScatter", 0.5f},
            {"grainReverse", 0.6f},
            {"grainMix", 0.3f},
            {"reverbEnabled", 1.0f},
            {"reverbDecay", 0.91f},
            {"reverbSize", 0.8f},
            {"reverbDamp", 0.35f},
            {"reverbPreDelay", 25.0f},
            {"reverbMix", 0.5f},
        }),

        makePreset("Zero Gravity", bank, {
            {"chainSlot0", 13.0f},   // HALFTIME
            {"chainSlot1", 8.0f},    // MULTIPLY
            {"chainSlot2", 6.0f},    // REVERB
            {"halftimeEnabled", 1.0f},
            {"halftimeDivision", 5.0f},  // 1 Bar
            {"halftimeSpeed", 0.0f},     // 2x
            {"halftimeFade", 0.2f},
            {"halftimeMix", 0.65f},
            {"unisonEnabled", 1.0f},
            {"unisonSpeed", 0.7f},
            {"unisonOuter", 0.8f},
            {"unisonInner", 0.4f},
            {"unisonMix", 0.5f},
            {"reverbEnabled", 1.0f},
            {"reverbDecay", 0.96f},
            {"reverbSize", 0.9f},
            {"reverbDamp", 0.2f},
            {"reverbPreDelay", 70.0f},
            {"reverbDiffusion", 0.9f},
            {"reverbMix", 0.6f},
        }),

        makePreset("Antimatter", bank, {
            {"chainSlot0", 4.0f},    // TAPE
            {"chainSlot1", 13.0f},   // HALFTIME
            {"chainSlot2", 11.0f},   // SHIFT
            {"chainSlot3", 6.0f},    // REVERB
            {"tapeEnabled", 1.0f},
            {"tapeDrive", 0.55f},
            {"tapeGlue", 0.7f},
            {"tapeNoise", 0.15f},
            {"tapeWow", 0.2f},
            {"tapeMix", 0.65f},
            {"halftimeEnabled", 1.0f},
            {"halftimeDivision", 3.0f},  // 1/4
            {"halftimeSpeed", 0.0f},     // 2x
            {"halftimeFade", 0.2f},
            {"halftimeMix", 0.7f},
            {"shiftEnabled", 1.0f},
            {"shiftPitch", -5.0f},       // fourth down
            {"shiftFeedback", 0.2f},
            {"shiftTone", 5000.0f},
            {"shiftMix", 0.3f},
            {"reverbEnabled", 1.0f},
            {"reverbDecay", 0.87f},
            {"reverbSize", 0.7f},
            {"reverbDamp", 0.45f},
            {"reverbMix", 0.4f},
        }),

        // ── Slow-motion textures ────────────────────────────────────────

        makePreset("Slow Motion", bank, {
            {"chainSlot0", 13.0f},
            {"halftimeEnabled", 1.0f},
            {"halftimeDivision", 5.0f},  // 1 Bar
            {"halftimeSpeed", 0.0f},     // 2x
            {"halftimeFade", 0.2f},
            {"halftimeMix", 1.0f},
        }),

        makePreset("Deep Freeze", bank, {
            {"chainSlot0", 9.0f},    // GRAIN
            {"chainSlot1", 13.0f},   // HALFTIME
            {"chainSlot2", 6.0f},    // REVERB
            {"grainEnabled", 1.0f},
            {"grainSize", 180.0f},
            {"grainDensity", 1.5f},
            {"grainPitch", -12.0f},
            {"grainScatter", 0.3f},
            {"grainReverse", 0.7f},
            {"grainMix", 0.6f},
            {"halftimeEnabled", 1.0f},
            {"halftimeDivision", 6.0f},  // 2 Bars
            {"halftimeSpeed", 1.0f},     // 4x
            {"halftimeFade", 0.45f},
            {"halftimeMix", 0.7f},
            {"reverbEnabled", 1.0f},
            {"reverbDecay", 0.97f},
            {"reverbSize", 0.95f},
            {"reverbDamp", 0.15f},
            {"reverbMix", 0.55f},
        }),

        makePreset("Stretched Tape", bank, {
            {"chainSlot0", 4.0f},    // TAPE
            {"chainSlot1", 13.0f},   // HALFTIME
            {"tapeEnabled", 1.0f},
            {"tapeDrive", 0.3f},
            {"tapeGlue", 0.5f},
            {"tapeNoise", 0.2f},
            {"tapeWow", 0.25f},
            {"tapeMix", 0.8f},
            {"halftimeEnabled", 1.0f},
            {"halftimeDivision", 4.0f},  // 1/2
            {"halftimeSpeed", 0.0f},     // 2x
            {"halftimeFade", 0.15f},
            {"halftimeMix", 0.85f},
        }),

        makePreset("Molasses", bank, {
            {"chainSlot0", 13.0f},   // HALFTIME
            {"chainSlot1", 5.0f},    // SATURATE
            {"halftimeEnabled", 1.0f},
            {"halftimeDivision", 5.0f},  // 1 Bar
            {"halftimeSpeed", 1.0f},     // 4x
            {"halftimeFade", 0.5f},
            {"halftimeMix", 0.95f},
            {"distortionEnabled", 1.0f},
            {"distortionDrive", 0.45f},
            {"distortionTone", 0.35f},
            {"distortionMode", 1.0f},    // Tube
            {"distortionMix", 0.5f},
        }),

        makePreset("Gravity Well", bank, {
            {"chainSlot0", 13.0f},   // HALFTIME
            {"chainSlot1", 11.0f},   // SHIFT
            {"chainSlot2", 6.0f},    // REVERB
            {"halftimeEnabled", 1.0f},
            {"halftimeDivision", 7.0f},  // 4 Bars
            {"halftimeSpeed", 1.0f},     // 4x
            {"halftimeFade", 0.6f},
            {"halftimeMix", 0.85f},
            {"shiftEnabled", 1.0f},
            {"shiftPitch", -24.0f},      // 2 octaves down
            {"shiftFeedback", 0.1f},
            {"shiftTone", 2000.0f},
            {"shiftMix", 0.3f},
            {"reverbEnabled", 1.0f},
            {"reverbDecay", 0.95f},
            {"reverbSize", 0.9f},
            {"reverbDamp", 0.2f},
            {"reverbMix", 0.5f},
        }),

        // ── Reversed / mirrored worlds ──────────────────────────────────

        makePreset("Mirror Dimension", bank, {
            {"chainSlot0", 12.0f},   // REVERSER
            {"chainSlot1", 6.0f},    // REVERB
            {"reverserEnabled", 1.0f},
            {"reverserRepeat", 1.0f},
            {"reverserDivision", 5.0f},  // 1/8
            {"reverserCrossfade", 0.35f},
            {"reverserMix", 0.8f},
            {"reverbEnabled", 1.0f},
            {"reverbDecay", 0.92f},
            {"reverbSize", 0.85f},
            {"reverbDamp", 0.3f},
            {"reverbPreDelay", 40.0f},
            {"reverbMix", 0.5f},
        }),

        makePreset("Palindrome", bank, {
            {"chainSlot0", 12.0f},   // REVERSER
            {"chainSlot1", 13.0f},   // HALFTIME
            {"chainSlot2", 3.0f},    // CHORUS
            {"reverserEnabled", 1.0f},
            {"reverserRepeat", 2.0f},
            {"reverserDivision", 7.0f},  // 1/4
            {"reverserCrossfade", 0.25f},
            {"reverserMix", 0.6f},
            {"halftimeEnabled", 1.0f},
            {"halftimeDivision", 3.0f},  // 1/4
            {"halftimeSpeed", 0.0f},
            {"halftimeFade", 0.2f},
            {"halftimeMix", 0.5f},
            {"chorusMix", 0.4f},
            {"junoMode", 2.0f},          // I+II
        }),

        makePreset("Backwards Rain", bank, {
            {"chainSlot0", 12.0f},   // REVERSER
            {"chainSlot1", 9.0f},    // GRAIN
            {"chainSlot2", 7.0f},    // HAZE
            {"chainSlot3", 6.0f},    // REVERB
            {"reverserEnabled", 1.0f},
            {"reverserRepeat", 8.0f},
            {"reverserDivision", 1.0f},  // 1/32
            {"reverserCrossfade", 0.1f},
            {"reverserMix", 0.55f},
            {"grainEnabled", 1.0f},
            {"grainSize", 15.0f},
            {"grainDensity", 14.0f},
            {"grainPitch", 5.0f},
            {"grainScatter", 0.8f},
            {"grainMix", 0.3f},
            {"hazeEnabled", 1.0f},
            {"hazeColor", 0.4f},
            {"hazeType", 0.0f},          // Pink
            {"hazeMix", 0.05f},
            {"reverbEnabled", 1.0f},
            {"reverbDecay", 0.88f},
            {"reverbSize", 0.7f},
            {"reverbDamp", 0.4f},
            {"reverbMix", 0.45f},
        }),

        // ── Degraded / corrupted signals ────────────────────────────────

        makePreset("Corrupted Memory", bank, {
            {"chainSlot0", 1.0f},    // CRUSH
            {"chainSlot1", 10.0f},   // STUTTER
            {"chainSlot2", 6.0f},    // REVERB
            {"destroyEnabled", 1.0f},
            {"destroyFader", 4000.0f},
            {"destroyBits", 6.0f},
            {"destroyJitter", 0.5f},
            {"destroyMix", 0.6f},
            {"stutterEnabled", 1.0f},
            {"stutterRate", 333.0f},
            {"stutterDecay", 0.5f},
            {"stutterDepth", 0.9f},
            {"stutterMix", 0.5f},
            {"reverbEnabled", 1.0f},
            {"reverbDecay", 0.8f},
            {"reverbSize", 0.5f},
            {"reverbDamp", 0.6f},
            {"reverbMix", 0.35f},
        }),

        makePreset("Data Loss", bank, {
            {"chainSlot0", 1.0f},    // CRUSH
            {"chainSlot1", 13.0f},   // HALFTIME
            {"chainSlot2", 7.0f},    // HAZE
            {"destroyEnabled", 1.0f},
            {"destroyFader", 6000.0f},
            {"destroyBits", 10.0f},
            {"destroyJitter", 0.2f},
            {"destroyMix", 0.45f},
            {"halftimeEnabled", 1.0f},
            {"halftimeDivision", 2.0f},  // 1/8
            {"halftimeSpeed", 0.0f},
            {"halftimeFade", 0.15f},
            {"halftimeMix", 0.6f},
            {"hazeEnabled", 1.0f},
            {"hazeColor", 0.8f},
            {"hazeType", 2.0f},          // Vinyl
            {"hazeMix", 0.12f},
        }),

        makePreset("Broken Broadcast", bank, {
            {"chainSlot0", 4.0f},    // TAPE
            {"chainSlot1", 1.0f},    // CRUSH
            {"chainSlot2", 10.0f},   // STUTTER
            {"tapeEnabled", 1.0f},
            {"tapeDrive", 0.6f},
            {"tapeGlue", 0.8f},
            {"tapeNoise", 0.25f},
            {"tapeWow", 0.3f},
            {"tapeMix", 0.75f},
            {"destroyEnabled", 1.0f},
            {"destroyFader", 12000.0f},
            {"destroyBits", 8.0f},
            {"destroyMix", 0.35f},
            {"stutterEnabled", 1.0f},
            {"stutterRate", 80.0f},
            {"stutterDecay", 0.7f},
            {"stutterDepth", 0.6f},
            {"stutterMix", 0.4f},
        }),

        // ── Shimmer / ethereal ──────────────────────────────────────────

        makePreset("Shimmer Gate", bank, {
            {"chainSlot0", 11.0f},   // SHIFT
            {"chainSlot1", 6.0f},    // REVERB
            {"shiftEnabled", 1.0f},
            {"shiftPitch", 12.0f},
            {"shiftFeedback", 0.6f},
            {"shiftTone", 8000.0f},
            {"shiftMix", 0.4f},
            {"reverbEnabled", 1.0f},
            {"reverbDecay", 0.95f},
            {"reverbSize", 0.9f},
            {"reverbDamp", 0.2f},
            {"reverbPreDelay", 30.0f},
            {"reverbDiffusion", 0.9f},
            {"reverbMix", 0.6f},
        }),

        makePreset("Crystal Cascade", bank, {
            {"chainSlot0", 11.0f},   // SHIFT
            {"chainSlot1", 3.0f},    // CHORUS
            {"chainSlot2", 6.0f},    // REVERB
            {"shiftEnabled", 1.0f},
            {"shiftPitch", 7.0f},    // fifth up
            {"shiftFeedback", 0.45f},
            {"shiftTone", 12000.0f},
            {"shiftMix", 0.35f},
            {"chorusMix", 0.5f},
            {"junoMode", 1.0f},      // II
            {"reverbEnabled", 1.0f},
            {"reverbDecay", 0.9f},
            {"reverbSize", 0.8f},
            {"reverbDamp", 0.25f},
            {"reverbPreDelay", 20.0f},
            {"reverbMix", 0.5f},
        }),

        makePreset("Aurora", bank, {
            {"chainSlot0", 8.0f},    // MULTIPLY
            {"chainSlot1", 11.0f},   // SHIFT
            {"chainSlot2", 6.0f},    // REVERB
            {"unisonEnabled", 1.0f},
            {"unisonSpeed", 0.55f},
            {"unisonOuter", 1.0f},
            {"unisonInner", 0.7f},
            {"unisonMix", 0.5f},
            {"shiftEnabled", 1.0f},
            {"shiftPitch", 12.0f},
            {"shiftFeedback", 0.3f},
            {"shiftTone", 6000.0f},
            {"shiftMix", 0.25f},
            {"reverbEnabled", 1.0f},
            {"reverbDecay", 0.93f},
            {"reverbSize", 0.85f},
            {"reverbDamp", 0.3f},
            {"reverbPreDelay", 45.0f},
            {"reverbMix", 0.55f},
        }),

        // ── Rhythmic / pulsing abstractions ─────────────────────────────

        makePreset("Pulse Engine", bank, {
            {"chainSlot0", 10.0f},   // STUTTER
            {"chainSlot1", 5.0f},    // SATURATE
            {"chainSlot2", 6.0f},    // REVERB
            {"stutterEnabled", 1.0f},
            {"stutterRate", 500.0f},
            {"stutterDecay", 0.4f},
            {"stutterDepth", 1.0f},
            {"stutterMix", 0.7f},
            {"distortionEnabled", 1.0f},
            {"distortionDrive", 0.3f},
            {"distortionTone", 0.6f},
            {"distortionMode", 2.0f},    // Hard
            {"distortionMix", 0.4f},
            {"reverbEnabled", 1.0f},
            {"reverbDecay", 0.75f},
            {"reverbSize", 0.5f},
            {"reverbDamp", 0.55f},
            {"reverbMix", 0.3f},
        }),

        makePreset("Temporal Loop", bank, {
            {"chainSlot0", 13.0f},   // HALFTIME
            {"chainSlot1", 10.0f},   // STUTTER
            {"chainSlot2", 6.0f},    // REVERB
            {"halftimeEnabled", 1.0f},
            {"halftimeDivision", 2.0f},  // 1/8
            {"halftimeSpeed", 0.0f},
            {"halftimeFade", 0.1f},
            {"halftimeMix", 0.7f},
            {"stutterEnabled", 1.0f},
            {"stutterRate", 250.0f},
            {"stutterDecay", 0.65f},
            {"stutterDepth", 0.7f},
            {"stutterMix", 0.4f},
            {"reverbEnabled", 1.0f},
            {"reverbDecay", 0.82f},
            {"reverbSize", 0.6f},
            {"reverbDamp", 0.5f},
            {"reverbMix", 0.35f},
        }),

        makePreset("Glitch Matrix", bank, {
            {"chainSlot0", 10.0f},   // STUTTER
            {"chainSlot1", 12.0f},   // REVERSER
            {"chainSlot2", 1.0f},    // CRUSH
            {"chainSlot3", 6.0f},    // REVERB
            {"stutterEnabled", 1.0f},
            {"stutterRate", 150.0f},
            {"stutterDecay", 0.55f},
            {"stutterDepth", 0.85f},
            {"stutterMix", 0.6f},
            {"reverserEnabled", 1.0f},
            {"reverserRepeat", 4.0f},
            {"reverserDivision", 3.0f},
            {"reverserCrossfade", 0.15f},
            {"reverserMix", 0.35f},
            {"destroyEnabled", 1.0f},
            {"destroyFader", 16000.0f},
            {"destroyBits", 12.0f},
            {"destroyJitter", 0.15f},
            {"destroyMix", 0.3f},
            {"reverbEnabled", 1.0f},
            {"reverbDecay", 0.78f},
            {"reverbSize", 0.45f},
            {"reverbMix", 0.3f},
        }),

        // ── Layered / complex textures ──────────────────────────────────

        makePreset("Parallel Universe", bank, {
            {"chainSlot0", 11.0f},   // SHIFT
            {"chainSlot1", 8.0f},    // MULTIPLY
            {"chainSlot2", 13.0f},   // HALFTIME
            {"chainSlot3", 6.0f},    // REVERB
            {"shiftEnabled", 1.0f},
            {"shiftPitch", -7.0f},
            {"shiftFeedback", 0.25f},
            {"shiftTone", 5500.0f},
            {"shiftMix", 0.4f},
            {"unisonEnabled", 1.0f},
            {"unisonSpeed", 0.8f},
            {"unisonOuter", 0.6f},
            {"unisonInner", 0.4f},
            {"unisonMix", 0.35f},
            {"halftimeEnabled", 1.0f},
            {"halftimeDivision", 4.0f},
            {"halftimeSpeed", 0.0f},
            {"halftimeFade", 0.25f},
            {"halftimeMix", 0.55f},
            {"reverbEnabled", 1.0f},
            {"reverbDecay", 0.9f},
            {"reverbSize", 0.75f},
            {"reverbDamp", 0.35f},
            {"reverbMix", 0.45f},
        }),

        makePreset("Dream Logic", bank, {
            {"chainSlot0", 3.0f},    // CHORUS
            {"chainSlot1", 13.0f},   // HALFTIME
            {"chainSlot2", 9.0f},    // GRAIN
            {"chainSlot3", 6.0f},    // REVERB
            {"chorusMix", 0.55f},
            {"junoMode", 2.0f},      // I+II
            {"halftimeEnabled", 1.0f},
            {"halftimeDivision", 3.0f},  // 1/4
            {"halftimeSpeed", 0.0f},
            {"halftimeFade", 0.2f},
            {"halftimeMix", 0.6f},
            {"grainEnabled", 1.0f},
            {"grainSize", 40.0f},
            {"grainDensity", 6.0f},
            {"grainPitch", 12.0f},
            {"grainScatter", 0.4f},
            {"grainReverse", 0.2f},
            {"grainMix", 0.25f},
            {"reverbEnabled", 1.0f},
            {"reverbDecay", 0.89f},
            {"reverbSize", 0.75f},
            {"reverbDamp", 0.35f},
            {"reverbMix", 0.45f},
        }),

        makePreset("Hypnosis", bank, {
            {"chainSlot0", 13.0f},   // HALFTIME
            {"chainSlot1", 4.0f},    // TAPE
            {"chainSlot2", 3.0f},    // CHORUS
            {"chainSlot3", 6.0f},    // REVERB
            {"halftimeEnabled", 1.0f},
            {"halftimeDivision", 5.0f},  // 1 Bar
            {"halftimeSpeed", 0.0f},
            {"halftimeFade", 0.35f},
            {"halftimeMix", 0.75f},
            {"tapeEnabled", 1.0f},
            {"tapeDrive", 0.2f},
            {"tapeGlue", 0.4f},
            {"tapeNoise", 0.1f},
            {"tapeWow", 0.12f},
            {"tapeMix", 0.6f},
            {"chorusMix", 0.45f},
            {"junoMode", 0.0f},      // I
            {"reverbEnabled", 1.0f},
            {"reverbDecay", 0.86f},
            {"reverbSize", 0.65f},
            {"reverbDamp", 0.4f},
            {"reverbMix", 0.4f},
        }),

        // ── Dark / industrial ───────────────────────────────────────────

        makePreset("Black Hole Sun", bank, {
            {"chainSlot0", 5.0f},    // SATURATE
            {"chainSlot1", 13.0f},   // HALFTIME
            {"chainSlot2", 6.0f},    // REVERB
            {"distortionEnabled", 1.0f},
            {"distortionDrive", 0.6f},
            {"distortionTone", 0.25f},
            {"distortionMode", 2.0f},    // Hard
            {"distortionMix", 0.55f},
            {"halftimeEnabled", 1.0f},
            {"halftimeDivision", 4.0f},  // 1/2
            {"halftimeSpeed", 1.0f},     // 4x
            {"halftimeFade", 0.3f},
            {"halftimeMix", 0.8f},
            {"reverbEnabled", 1.0f},
            {"reverbDecay", 0.9f},
            {"reverbSize", 0.8f},
            {"reverbDamp", 0.5f},
            {"reverbMix", 0.45f},
        }),

        makePreset("Rust", bank, {
            {"chainSlot0", 1.0f},    // CRUSH
            {"chainSlot1", 4.0f},    // TAPE
            {"chainSlot2", 7.0f},    // HAZE
            {"destroyEnabled", 1.0f},
            {"destroyFader", 10000.0f},
            {"destroyBits", 10.0f},
            {"destroyJitter", 0.35f},
            {"destroyMix", 0.5f},
            {"tapeEnabled", 1.0f},
            {"tapeDrive", 0.5f},
            {"tapeGlue", 0.65f},
            {"tapeNoise", 0.2f},
            {"tapeWow", 0.18f},
            {"tapeMix", 0.7f},
            {"hazeEnabled", 1.0f},
            {"hazeColor", 0.5f},
            {"hazeType", 2.0f},          // Vinyl
            {"hazeMix", 0.1f},
        }),

        makePreset("Void Static", bank, {
            {"chainSlot0", 7.0f},    // HAZE
            {"chainSlot1", 13.0f},   // HALFTIME
            {"chainSlot2", 5.0f},    // SATURATE
            {"chainSlot3", 6.0f},    // REVERB
            {"hazeEnabled", 1.0f},
            {"hazeColor", 0.2f},
            {"hazeType", 1.0f},          // White
            {"hazeMix", 0.2f},
            {"halftimeEnabled", 1.0f},
            {"halftimeDivision", 6.0f},  // 2 Bars
            {"halftimeSpeed", 1.0f},     // 4x
            {"halftimeFade", 0.4f},
            {"halftimeMix", 0.7f},
            {"distortionEnabled", 1.0f},
            {"distortionDrive", 0.25f},
            {"distortionTone", 0.3f},
            {"distortionMode", 0.0f},    // Soft
            {"distortionMix", 0.4f},
            {"reverbEnabled", 1.0f},
            {"reverbDecay", 0.85f},
            {"reverbSize", 0.6f},
            {"reverbDamp", 0.55f},
            {"reverbMix", 0.4f},
        }),

        // ── Minimal / subtle abstractions ───────────────────────────────

        makePreset("Subtle Warp", bank, {
            {"chainSlot0", 13.0f},   // HALFTIME
            {"halftimeEnabled", 1.0f},
            {"halftimeDivision", 3.0f},  // 1/4
            {"halftimeSpeed", 0.0f},     // 2x
            {"halftimeFade", 0.3f},
            {"halftimeMix", 0.35f},
        }),

        makePreset("Gentle Drift", bank, {
            {"chainSlot0", 8.0f},    // MULTIPLY
            {"chainSlot1", 6.0f},    // REVERB
            {"unisonEnabled", 1.0f},
            {"unisonSpeed", 0.5f},
            {"unisonOuter", 0.5f},
            {"unisonInner", 0.3f},
            {"unisonMix", 0.3f},
            {"reverbEnabled", 1.0f},
            {"reverbDecay", 0.85f},
            {"reverbSize", 0.7f},
            {"reverbDamp", 0.35f},
            {"reverbPreDelay", 25.0f},
            {"reverbMix", 0.35f},
        }),

        makePreset("Micro Grain", bank, {
            {"chainSlot0", 9.0f},    // GRAIN
            {"chainSlot1", 6.0f},    // REVERB
            {"grainEnabled", 1.0f},
            {"grainSize", 8.0f},
            {"grainDensity", 16.0f},
            {"grainPitch", 0.0f},
            {"grainScatter", 0.2f},
            {"grainReverse", 0.1f},
            {"grainMix", 0.4f},
            {"reverbEnabled", 1.0f},
            {"reverbDecay", 0.8f},
            {"reverbSize", 0.55f},
            {"reverbDamp", 0.45f},
            {"reverbMix", 0.3f},
        }),

        makePreset("Soft Disintegration", bank, {
            {"chainSlot0", 9.0f},    // GRAIN
            {"chainSlot1", 13.0f},   // HALFTIME
            {"chainSlot2", 7.0f},    // HAZE
            {"chainSlot3", 6.0f},    // REVERB
            {"grainEnabled", 1.0f},
            {"grainSize", 100.0f},
            {"grainDensity", 3.0f},
            {"grainPitch", 0.0f},
            {"grainScatter", 0.95f},
            {"grainReverse", 0.5f},
            {"grainMix", 0.5f},
            {"halftimeEnabled", 1.0f},
            {"halftimeDivision", 5.0f},
            {"halftimeSpeed", 0.0f},
            {"halftimeFade", 0.3f},
            {"halftimeMix", 0.4f},
            {"hazeEnabled", 1.0f},
            {"hazeColor", 0.35f},
            {"hazeType", 0.0f},
            {"hazeMix", 0.04f},
            {"reverbEnabled", 1.0f},
            {"reverbDecay", 0.91f},
            {"reverbSize", 0.8f},
            {"reverbDamp", 0.3f},
            {"reverbMix", 0.5f},
        }),

        // ── Pitch-warped spaces ─────────────────────────────────────────

        makePreset("Octave Below", bank, {
            {"chainSlot0", 11.0f},   // SHIFT
            {"chainSlot1", 6.0f},    // REVERB
            {"shiftEnabled", 1.0f},
            {"shiftPitch", -12.0f},
            {"shiftFeedback", 0.0f},
            {"shiftTone", 8000.0f},
            {"shiftMix", 0.5f},
            {"reverbEnabled", 1.0f},
            {"reverbDecay", 0.88f},
            {"reverbSize", 0.75f},
            {"reverbDamp", 0.4f},
            {"reverbMix", 0.4f},
        }),

        makePreset("Harmonic Ghost", bank, {
            {"chainSlot0", 11.0f},   // SHIFT
            {"chainSlot1", 11.0f},   // SHIFT (stacked)
            {"chainSlot2", 6.0f},    // REVERB
            {"shiftEnabled", 1.0f},
            {"shiftPitch", 7.0f},    // fifth
            {"shiftFeedback", 0.5f},
            {"shiftTone", 6000.0f},
            {"shiftMix", 0.35f},
            {"reverbEnabled", 1.0f},
            {"reverbDecay", 0.92f},
            {"reverbSize", 0.85f},
            {"reverbDamp", 0.25f},
            {"reverbPreDelay", 35.0f},
            {"reverbDiffusion", 0.85f},
            {"reverbMix", 0.55f},
        }),

        makePreset("Detuned Realm", bank, {
            {"chainSlot0", 8.0f},    // MULTIPLY
            {"chainSlot1", 13.0f},   // HALFTIME
            {"chainSlot2", 7.0f},    // HAZE
            {"chainSlot3", 6.0f},    // REVERB
            {"unisonEnabled", 1.0f},
            {"unisonSpeed", 1.5f},
            {"unisonOuter", 1.0f},
            {"unisonInner", 0.9f},
            {"unisonMix", 0.55f},
            {"halftimeEnabled", 1.0f},
            {"halftimeDivision", 3.0f},
            {"halftimeSpeed", 0.0f},
            {"halftimeFade", 0.2f},
            {"halftimeMix", 0.5f},
            {"hazeEnabled", 1.0f},
            {"hazeColor", 0.45f},
            {"hazeType", 2.0f},
            {"hazeMix", 0.07f},
            {"reverbEnabled", 1.0f},
            {"reverbDecay", 0.87f},
            {"reverbSize", 0.7f},
            {"reverbDamp", 0.4f},
            {"reverbMix", 0.4f},
        }),

        makePreset("Infinite Staircase", bank, {
            {"chainSlot0", 11.0f},   // SHIFT
            {"chainSlot1", 13.0f},   // HALFTIME
            {"chainSlot2", 6.0f},    // REVERB
            {"shiftEnabled", 1.0f},
            {"shiftPitch", 12.0f},
            {"shiftFeedback", 0.65f},
            {"shiftTone", 10000.0f},
            {"shiftMix", 0.3f},
            {"halftimeEnabled", 1.0f},
            {"halftimeDivision", 4.0f},
            {"halftimeSpeed", 0.0f},
            {"halftimeFade", 0.25f},
            {"halftimeMix", 0.6f},
            {"reverbEnabled", 1.0f},
            {"reverbDecay", 0.94f},
            {"reverbSize", 0.9f},
            {"reverbDamp", 0.2f},
            {"reverbPreDelay", 50.0f},
            {"reverbMix", 0.55f},
        }),
    };
}
