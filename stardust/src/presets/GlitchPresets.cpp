#include "FactoryPresets.h"

// Chain slot IDs: 0=empty, 1=CRUSH, 3=CHORUS, 4=TAPE, 5=SATURATE,
//                 6=REVERB, 7=HAZE, 8=MULTIPLY, 9=GRAIN, 10=STUTTER,
//                 11=SHIFT, 12=REVERSER

std::vector<Preset> getGlitchPresets()
{
    const juce::String bank = "Glitch";
    return {
        // --- Stutter / Buffer Repeat ---
        makePreset("Classic Stutter", bank, {
            {"chainSlot0", 10.0f}, {"stutterEnabled", 1.0f},
            {"stutterRate", 125.0f}, {"stutterDecay", 0.8f},
            {"stutterDepth", 1.0f}, {"stutterMix", 0.8f}
        }),
        makePreset("Fast Glitch", bank, {
            {"chainSlot0", 10.0f}, {"stutterEnabled", 1.0f},
            {"stutterRate", 500.0f}, {"stutterDecay", 0.6f},
            {"stutterDepth", 1.0f}, {"stutterMix", 0.9f}
        }),
        makePreset("Slow Stutter", bank, {
            {"chainSlot0", 10.0f}, {"stutterEnabled", 1.0f},
            {"stutterRate", 50.0f}, {"stutterDecay", 0.9f},
            {"stutterDepth", 0.8f}, {"stutterMix", 0.7f}
        }),
        makePreset("Rapid Fire", bank, {
            {"chainSlot0", 10.0f}, {"stutterEnabled", 1.0f},
            {"stutterRate", 800.0f}, {"stutterDecay", 0.5f},
            {"stutterDepth", 1.0f}, {"stutterMix", 1.0f}
        }),
        makePreset("Decaying Repeat", bank, {
            {"chainSlot0", 10.0f}, {"stutterEnabled", 1.0f},
            {"stutterRate", 200.0f}, {"stutterDecay", 0.3f},
            {"stutterDepth", 1.0f}, {"stutterMix", 0.85f}
        }),

        // --- Grain Scatter / Chaos ---
        makePreset("Scattered Signal", bank, {
            {"chainSlot0", 9.0f}, {"grainEnabled", 1.0f},
            {"grainSize", 10.0f}, {"grainDensity", 8.0f},
            {"grainPitch", 0.0f}, {"grainScatter", 1.0f}, {"grainMix", 0.8f}
        }),
        makePreset("Grain Mayhem", bank, {
            {"chainSlot0", 9.0f}, {"grainEnabled", 1.0f},
            {"grainSize", 8.0f}, {"grainDensity", 14.0f},
            {"grainPitch", 5.0f}, {"grainScatter", 0.9f}, {"grainMix", 0.85f}
        }),
        makePreset("Pixelated", bank, {
            {"chainSlot0", 9.0f}, {"chainSlot1", 1.0f},
            {"grainEnabled", 1.0f}, {"destroyEnabled", 1.0f},
            {"grainSize", 5.0f}, {"grainDensity", 16.0f},
            {"grainPitch", 0.0f}, {"grainScatter", 0.7f}, {"grainMix", 0.75f},
            {"destroyFader", 8000.0f}, {"destroyBits", 8.0f},
            {"destroyIn", 4.0f}, {"destroyOut", -2.0f}
        }),
        makePreset("Broken Transmit", bank, {
            {"chainSlot0", 9.0f}, {"grainEnabled", 1.0f},
            {"grainSize", 12.0f}, {"grainDensity", 4.0f},
            {"grainPitch", 0.0f}, {"grainScatter", 1.0f}, {"grainMix", 0.9f}
        }),
        makePreset("Static Burst", bank, {
            {"chainSlot0", 9.0f}, {"chainSlot1", 7.0f},
            {"grainEnabled", 1.0f}, {"hazeEnabled", 1.0f},
            {"grainSize", 6.0f}, {"grainDensity", 12.0f},
            {"grainPitch", 3.0f}, {"grainScatter", 0.85f}, {"grainMix", 0.7f},
            {"hazeType", 1.0f}, {"hazeMix", 0.15f}
        }),

        // --- Reverser / Tape Reverse ---
        makePreset("Reverse Drift", bank, {
            {"chainSlot0", 12.0f}, {"reverserEnabled", 1.0f},
            {"reverserRepeat", 4.0f}, {"reverserDivision", 3.0f},
            {"reverserCrossfade", 0.15f}, {"reverserMix", 0.6f}
        }),
        makePreset("Quick Reverse", bank, {
            {"chainSlot0", 12.0f}, {"reverserEnabled", 1.0f},
            {"reverserRepeat", 2.0f}, {"reverserDivision", 1.0f},
            {"reverserCrossfade", 0.1f}, {"reverserMix", 0.7f}
        }),
        makePreset("Long Reverse", bank, {
            {"chainSlot0", 12.0f}, {"reverserEnabled", 1.0f},
            {"reverserRepeat", 8.0f}, {"reverserDivision", 5.0f},
            {"reverserCrossfade", 0.2f}, {"reverserMix", 0.5f}
        }),
        makePreset("Reverse Swell", bank, {
            {"chainSlot0", 12.0f}, {"chainSlot1", 6.0f},
            {"reverserEnabled", 1.0f}, {"reverbEnabled", 1.0f},
            {"reverserRepeat", 4.0f}, {"reverserDivision", 4.0f},
            {"reverserCrossfade", 0.25f}, {"reverserMix", 0.55f},
            {"reverbMix", 0.4f}, {"reverbDecay", 0.6f}
        }),
        makePreset("Staccato Reverse", bank, {
            {"chainSlot0", 12.0f}, {"reverserEnabled", 1.0f},
            {"reverserRepeat", 1.0f}, {"reverserDivision", 2.0f},
            {"reverserCrossfade", 0.05f}, {"reverserMix", 0.8f}
        }),

        // --- Stutter + Grain Combos ---
        makePreset("Glitch Cloud", bank, {
            {"chainSlot0", 10.0f}, {"chainSlot1", 9.0f},
            {"stutterEnabled", 1.0f}, {"grainEnabled", 1.0f},
            {"stutterRate", 250.0f}, {"stutterDecay", 0.7f},
            {"stutterDepth", 0.8f}, {"stutterMix", 0.7f},
            {"grainSize", 20.0f}, {"grainDensity", 8.0f},
            {"grainScatter", 0.6f}, {"grainMix", 0.5f}
        }),
        makePreset("Shattered Glass", bank, {
            {"chainSlot0", 9.0f}, {"chainSlot1", 10.0f},
            {"grainEnabled", 1.0f}, {"stutterEnabled", 1.0f},
            {"grainSize", 8.0f}, {"grainDensity", 12.0f},
            {"grainPitch", 7.0f}, {"grainScatter", 0.8f}, {"grainMix", 0.7f},
            {"stutterRate", 400.0f}, {"stutterDecay", 0.5f}, {"stutterMix", 0.6f}
        }),
        makePreset("Buffer Overflow", bank, {
            {"chainSlot0", 10.0f}, {"chainSlot1", 9.0f}, {"chainSlot2", 10.0f},
            {"stutterEnabled", 1.0f}, {"grainEnabled", 1.0f},
            {"stutterRate", 300.0f}, {"stutterDecay", 0.6f}, {"stutterMix", 0.75f},
            {"grainSize", 15.0f}, {"grainDensity", 10.0f},
            {"grainScatter", 0.9f}, {"grainMix", 0.6f}
        }),
        makePreset("Micro Stutter", bank, {
            {"chainSlot0", 10.0f}, {"stutterEnabled", 1.0f},
            {"stutterRate", 1000.0f}, {"stutterDecay", 0.4f},
            {"stutterDepth", 1.0f}, {"stutterMix", 0.9f}
        }),
        makePreset("Grain Stutter", bank, {
            {"chainSlot0", 9.0f}, {"chainSlot1", 10.0f},
            {"grainEnabled", 1.0f}, {"stutterEnabled", 1.0f},
            {"grainSize", 25.0f}, {"grainDensity", 6.0f},
            {"grainScatter", 0.5f}, {"grainMix", 0.6f},
            {"stutterRate", 150.0f}, {"stutterDecay", 0.75f}, {"stutterMix", 0.5f}
        }),

        // --- Pitch Glitch ---
        makePreset("Pitch Staircase", bank, {
            {"chainSlot0", 11.0f}, {"shiftEnabled", 1.0f},
            {"shiftPitch", 5.0f}, {"shiftFeedback", 0.6f},
            {"shiftTone", 8000.0f}, {"shiftMix", 0.6f}
        }),
        makePreset("Falling Pitch", bank, {
            {"chainSlot0", 11.0f}, {"shiftEnabled", 1.0f},
            {"shiftPitch", -7.0f}, {"shiftFeedback", 0.5f},
            {"shiftTone", 6000.0f}, {"shiftMix", 0.55f}
        }),
        makePreset("Chromatic Cascade", bank, {
            {"chainSlot0", 11.0f}, {"chainSlot1", 6.0f},
            {"shiftEnabled", 1.0f}, {"reverbEnabled", 1.0f},
            {"shiftPitch", 7.0f}, {"shiftFeedback", 0.7f},
            {"shiftTone", 10000.0f}, {"shiftMix", 0.5f},
            {"reverbMix", 0.35f}, {"reverbDecay", 0.5f}
        }),
        makePreset("Octave Spiral", bank, {
            {"chainSlot0", 11.0f}, {"shiftEnabled", 1.0f},
            {"shiftPitch", 12.0f}, {"shiftFeedback", 0.8f},
            {"shiftTone", 12000.0f}, {"shiftMix", 0.45f}
        }),
        makePreset("Detuned Glitch", bank, {
            {"chainSlot0", 11.0f}, {"chainSlot1", 10.0f},
            {"shiftEnabled", 1.0f}, {"stutterEnabled", 1.0f},
            {"shiftPitch", 0.5f}, {"shiftFeedback", 0.3f}, {"shiftMix", 0.6f},
            {"stutterRate", 200.0f}, {"stutterDecay", 0.7f}, {"stutterMix", 0.5f}
        }),

        // --- Crushed Glitch ---
        makePreset("Crushed Stutter", bank, {
            {"chainSlot0", 1.0f}, {"chainSlot1", 10.0f},
            {"destroyEnabled", 1.0f}, {"stutterEnabled", 1.0f},
            {"destroyFader", 8000.0f}, {"destroyBits", 6.0f},
            {"destroyIn", 8.0f}, {"destroyOut", -4.0f},
            {"stutterRate", 200.0f}, {"stutterDecay", 0.7f}, {"stutterMix", 0.8f}
        }),
        makePreset("Digital Fracture", bank, {
            {"chainSlot0", 1.0f}, {"chainSlot1", 9.0f},
            {"destroyEnabled", 1.0f}, {"grainEnabled", 1.0f},
            {"destroyFader", 6000.0f}, {"destroyBits", 4.0f},
            {"destroyIn", 10.0f}, {"destroyOut", -5.0f},
            {"grainSize", 10.0f}, {"grainDensity", 12.0f},
            {"grainScatter", 0.8f}, {"grainMix", 0.7f}
        }),
        makePreset("Bit Scatter", bank, {
            {"chainSlot0", 9.0f}, {"chainSlot1", 1.0f},
            {"grainEnabled", 1.0f}, {"destroyEnabled", 1.0f},
            {"grainSize", 15.0f}, {"grainDensity", 8.0f},
            {"grainScatter", 0.9f}, {"grainMix", 0.75f},
            {"destroyFader", 12000.0f}, {"destroyBits", 8.0f},
            {"destroyIn", 6.0f}, {"destroyOut", -3.0f}
        }),
        makePreset("Codec Glitch", bank, {
            {"chainSlot0", 1.0f}, {"chainSlot1", 10.0f}, {"chainSlot2", 9.0f},
            {"destroyEnabled", 1.0f}, {"stutterEnabled", 1.0f}, {"grainEnabled", 1.0f},
            {"destroyFader", 11025.0f}, {"destroyBits", 8.0f},
            {"destroyIn", 4.0f}, {"destroyOut", -2.0f},
            {"stutterRate", 350.0f}, {"stutterDecay", 0.6f}, {"stutterMix", 0.6f},
            {"grainSize", 12.0f}, {"grainDensity", 6.0f},
            {"grainScatter", 0.7f}, {"grainMix", 0.4f}
        }),
        makePreset("Artifact Storm", bank, {
            {"chainSlot0", 1.0f}, {"chainSlot1", 9.0f}, {"chainSlot2", 7.0f},
            {"destroyEnabled", 1.0f}, {"grainEnabled", 1.0f}, {"hazeEnabled", 1.0f},
            {"destroyFader", 4000.0f}, {"destroyBits", 4.0f},
            {"destroyIn", 12.0f}, {"destroyOut", -6.0f},
            {"grainSize", 8.0f}, {"grainDensity", 14.0f},
            {"grainScatter", 1.0f}, {"grainMix", 0.8f},
            {"hazeType", 1.0f}, {"hazeMix", 0.15f}
        }),

        // --- Reverse + Glitch ---
        makePreset("Reverse Glitch", bank, {
            {"chainSlot0", 12.0f}, {"chainSlot1", 10.0f},
            {"reverserEnabled", 1.0f}, {"stutterEnabled", 1.0f},
            {"reverserRepeat", 4.0f}, {"reverserDivision", 3.0f},
            {"reverserMix", 0.6f},
            {"stutterRate", 250.0f}, {"stutterDecay", 0.7f}, {"stutterMix", 0.5f}
        }),
        makePreset("Reverse Scatter", bank, {
            {"chainSlot0", 12.0f}, {"chainSlot1", 9.0f},
            {"reverserEnabled", 1.0f}, {"grainEnabled", 1.0f},
            {"reverserRepeat", 3.0f}, {"reverserDivision", 2.0f},
            {"reverserMix", 0.5f},
            {"grainSize", 20.0f}, {"grainDensity", 6.0f},
            {"grainScatter", 0.7f}, {"grainMix", 0.5f}
        }),
        makePreset("Time Fracture", bank, {
            {"chainSlot0", 12.0f}, {"chainSlot1", 10.0f}, {"chainSlot2", 11.0f},
            {"reverserEnabled", 1.0f}, {"stutterEnabled", 1.0f}, {"shiftEnabled", 1.0f},
            {"reverserRepeat", 2.0f}, {"reverserDivision", 3.0f}, {"reverserMix", 0.5f},
            {"stutterRate", 300.0f}, {"stutterDecay", 0.6f}, {"stutterMix", 0.5f},
            {"shiftPitch", -5.0f}, {"shiftMix", 0.3f}
        }),

        // --- Experimental / Multi-Effect ---
        makePreset("Corrupted File", bank, {
            {"chainSlot0", 10.0f}, {"chainSlot1", 1.0f}, {"chainSlot2", 10.0f},
            {"stutterEnabled", 1.0f}, {"destroyEnabled", 1.0f},
            {"stutterRate", 600.0f}, {"stutterDecay", 0.4f}, {"stutterMix", 0.85f},
            {"destroyFader", 4000.0f}, {"destroyBits", 4.0f},
            {"destroyIn", 10.0f}, {"destroyOut", -5.0f}
        }),
        makePreset("Data Moshing", bank, {
            {"chainSlot0", 9.0f}, {"chainSlot1", 10.0f}, {"chainSlot2", 1.0f},
            {"grainEnabled", 1.0f}, {"stutterEnabled", 1.0f}, {"destroyEnabled", 1.0f},
            {"grainSize", 10.0f}, {"grainDensity", 16.0f},
            {"grainScatter", 1.0f}, {"grainMix", 0.8f},
            {"stutterRate", 400.0f}, {"stutterDecay", 0.5f}, {"stutterMix", 0.7f},
            {"destroyFader", 6000.0f}, {"destroyBits", 6.0f},
            {"destroyIn", 8.0f}, {"destroyOut", -4.0f}
        }),
        makePreset("Broken Machine", bank, {
            {"chainSlot0", 10.0f}, {"chainSlot1", 11.0f}, {"chainSlot2", 1.0f}, {"chainSlot3", 10.0f},
            {"stutterEnabled", 1.0f}, {"shiftEnabled", 1.0f}, {"destroyEnabled", 1.0f},
            {"stutterRate", 150.0f}, {"stutterDecay", 0.8f}, {"stutterMix", 0.7f},
            {"shiftPitch", -12.0f}, {"shiftFeedback", 0.4f}, {"shiftMix", 0.4f},
            {"destroyFader", 8000.0f}, {"destroyBits", 6.0f},
            {"destroyIn", 6.0f}, {"destroyOut", -3.0f}
        }),
        makePreset("System Error", bank, {
            {"chainSlot0", 1.0f}, {"chainSlot1", 10.0f}, {"chainSlot2", 11.0f}, {"chainSlot3", 9.0f},
            {"destroyEnabled", 1.0f}, {"stutterEnabled", 1.0f},
            {"shiftEnabled", 1.0f}, {"grainEnabled", 1.0f},
            {"destroyFader", 3000.0f}, {"destroyBits", 3.0f},
            {"destroyIn", 14.0f}, {"destroyOut", -7.0f},
            {"stutterRate", 500.0f}, {"stutterDecay", 0.5f}, {"stutterMix", 0.6f},
            {"shiftPitch", 5.0f}, {"shiftFeedback", 0.5f}, {"shiftMix", 0.35f},
            {"grainSize", 8.0f}, {"grainDensity", 10.0f},
            {"grainScatter", 0.9f}, {"grainMix", 0.4f}
        }),
        makePreset("Malfunction", bank, {
            {"chainSlot0", 10.0f}, {"chainSlot1", 12.0f}, {"chainSlot2", 10.0f},
            {"stutterEnabled", 1.0f}, {"reverserEnabled", 1.0f},
            {"stutterRate", 700.0f}, {"stutterDecay", 0.3f}, {"stutterMix", 0.9f},
            {"reverserRepeat", 2.0f}, {"reverserDivision", 1.0f}, {"reverserMix", 0.5f}
        }),

        // --- Subtle Glitch ---
        makePreset("Gentle Stutter", bank, {
            {"chainSlot0", 10.0f}, {"stutterEnabled", 1.0f},
            {"stutterRate", 100.0f}, {"stutterDecay", 0.9f},
            {"stutterDepth", 0.5f}, {"stutterMix", 0.35f}
        }),
        makePreset("Soft Scatter", bank, {
            {"chainSlot0", 9.0f}, {"grainEnabled", 1.0f},
            {"grainSize", 30.0f}, {"grainDensity", 4.0f},
            {"grainPitch", 0.0f}, {"grainScatter", 0.6f}, {"grainMix", 0.3f}
        }),
        makePreset("Micro Reverse", bank, {
            {"chainSlot0", 12.0f}, {"reverserEnabled", 1.0f},
            {"reverserRepeat", 2.0f}, {"reverserDivision", 0.0f},
            {"reverserCrossfade", 0.2f}, {"reverserMix", 0.3f}
        }),

        // --- Additional Glitch ---
        makePreset("Tape Glitch", bank, {
            {"chainSlot0", 4.0f}, {"chainSlot1", 10.0f},
            {"tapeEnabled", 1.0f}, {"stutterEnabled", 1.0f},
            {"tapeDrive", 0.3f}, {"tapeWow", 0.3f}, {"tapeMix", 0.8f},
            {"stutterRate", 200.0f}, {"stutterDecay", 0.7f}, {"stutterMix", 0.6f}
        }),
        makePreset("Grain Freeze", bank, {
            {"chainSlot0", 9.0f}, {"grainEnabled", 1.0f},
            {"grainSize", 200.0f}, {"grainDensity", 16.0f},
            {"grainPitch", 0.0f}, {"grainScatter", 0.05f}, {"grainMix", 0.95f}
        }),
        makePreset("Stutter Shimmer", bank, {
            {"chainSlot0", 10.0f}, {"chainSlot1", 11.0f}, {"chainSlot2", 6.0f},
            {"stutterEnabled", 1.0f}, {"shiftEnabled", 1.0f}, {"reverbEnabled", 1.0f},
            {"stutterRate", 150.0f}, {"stutterDecay", 0.8f}, {"stutterMix", 0.6f},
            {"shiftPitch", 12.0f}, {"shiftFeedback", 0.3f}, {"shiftMix", 0.3f},
            {"reverbMix", 0.4f}, {"reverbDecay", 0.6f}
        }),
        makePreset("Choppy Reverse", bank, {
            {"chainSlot0", 12.0f}, {"chainSlot1", 10.0f},
            {"reverserEnabled", 1.0f}, {"stutterEnabled", 1.0f},
            {"reverserRepeat", 2.0f}, {"reverserDivision", 5.0f},
            {"reverserMix", 0.6f},
            {"stutterRate", 350.0f}, {"stutterDecay", 0.5f}, {"stutterMix", 0.4f}
        }),
        makePreset("Granular Stutter", bank, {
            {"chainSlot0", 9.0f}, {"chainSlot1", 10.0f},
            {"grainEnabled", 1.0f}, {"stutterEnabled", 1.0f},
            {"grainSize", 15.0f}, {"grainDensity", 10.0f},
            {"grainPitch", 3.0f}, {"grainScatter", 0.7f}, {"grainMix", 0.65f},
            {"stutterRate", 250.0f}, {"stutterDecay", 0.65f}, {"stutterMix", 0.55f}
        }),
        makePreset("Pitch Drop Stutter", bank, {
            {"chainSlot0", 11.0f}, {"chainSlot1", 10.0f},
            {"shiftEnabled", 1.0f}, {"stutterEnabled", 1.0f},
            {"shiftPitch", -12.0f}, {"shiftFeedback", 0.3f}, {"shiftMix", 0.5f},
            {"stutterRate", 180.0f}, {"stutterDecay", 0.7f}, {"stutterMix", 0.6f}
        }),
        makePreset("Reverse Pitch Up", bank, {
            {"chainSlot0", 12.0f}, {"chainSlot1", 11.0f},
            {"reverserEnabled", 1.0f}, {"shiftEnabled", 1.0f},
            {"reverserRepeat", 4.0f}, {"reverserDivision", 3.0f}, {"reverserMix", 0.5f},
            {"shiftPitch", 7.0f}, {"shiftFeedback", 0.4f}, {"shiftMix", 0.35f}
        }),
        makePreset("Scatter Freeze", bank, {
            {"chainSlot0", 9.0f}, {"chainSlot1", 6.0f},
            {"grainEnabled", 1.0f}, {"reverbEnabled", 1.0f},
            {"grainSize", 180.0f}, {"grainDensity", 14.0f},
            {"grainPitch", 0.0f}, {"grainScatter", 0.8f}, {"grainMix", 0.85f},
            {"reverbMix", 0.3f}, {"reverbDecay", 0.5f}
        }),
        makePreset("Digital Stutter", bank, {
            {"chainSlot0", 1.0f}, {"chainSlot1", 10.0f},
            {"destroyEnabled", 1.0f}, {"stutterEnabled", 1.0f},
            {"destroyFader", 11025.0f}, {"destroyBits", 10.0f},
            {"destroyIn", 5.0f}, {"destroyOut", -2.5f},
            {"stutterRate", 300.0f}, {"stutterDecay", 0.6f}, {"stutterMix", 0.7f}
        }),
    };
}
