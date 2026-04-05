#include "FactoryPresets.h"

// Chain slot IDs: 0=empty, 1=CRUSH, 3=CHORUS, 4=TAPE, 5=SATURATE,
//                 6=REVERB, 7=HAZE, 8=MULTIPLY, 9=GRAIN, 10=STUTTER,
//                 11=SHIFT, 12=REVERSER

std::vector<Preset> getGrainsPresets()
{
    const juce::String bank = "Grains";
    return {
        // --- Ambient Grain Clouds ---
        makePreset("Morning Fog", bank, {
            {"chainSlot0", 9.0f}, {"grainEnabled", 1.0f},
            {"grainSize", 80.0f}, {"grainDensity", 6.0f},
            {"grainPitch", 0.0f}, {"grainScatter", 0.3f}, {"grainMix", 0.6f}
        }),
        makePreset("Cloud Layer", bank, {
            {"chainSlot0", 9.0f}, {"grainEnabled", 1.0f},
            {"grainSize", 120.0f}, {"grainDensity", 8.0f},
            {"grainPitch", 0.0f}, {"grainScatter", 0.4f}, {"grainMix", 0.7f}
        }),
        makePreset("Soft Particles", bank, {
            {"chainSlot0", 9.0f}, {"grainEnabled", 1.0f},
            {"grainSize", 60.0f}, {"grainDensity", 10.0f},
            {"grainPitch", 0.0f}, {"grainScatter", 0.2f}, {"grainMix", 0.5f}
        }),
        makePreset("Grain Whisper", bank, {
            {"chainSlot0", 9.0f}, {"grainEnabled", 1.0f},
            {"grainSize", 40.0f}, {"grainDensity", 12.0f},
            {"grainPitch", 0.0f}, {"grainScatter", 0.15f}, {"grainMix", 0.35f}
        }),
        makePreset("Ambient Dust", bank, {
            {"chainSlot0", 9.0f}, {"grainEnabled", 1.0f},
            {"grainSize", 100.0f}, {"grainDensity", 5.0f},
            {"grainPitch", 0.0f}, {"grainScatter", 0.5f}, {"grainMix", 0.65f}
        }),

        // --- Pitch Shifted Grains ---
        makePreset("Octave Up Cloud", bank, {
            {"chainSlot0", 9.0f}, {"grainEnabled", 1.0f},
            {"grainSize", 60.0f}, {"grainDensity", 6.0f},
            {"grainPitch", 12.0f}, {"grainScatter", 0.3f}, {"grainMix", 0.5f}
        }),
        makePreset("Octave Down Cloud", bank, {
            {"chainSlot0", 9.0f}, {"grainEnabled", 1.0f},
            {"grainSize", 80.0f}, {"grainDensity", 6.0f},
            {"grainPitch", -12.0f}, {"grainScatter", 0.3f}, {"grainMix", 0.5f}
        }),
        makePreset("Fifth Above", bank, {
            {"chainSlot0", 9.0f}, {"grainEnabled", 1.0f},
            {"grainSize", 50.0f}, {"grainDensity", 8.0f},
            {"grainPitch", 7.0f}, {"grainScatter", 0.25f}, {"grainMix", 0.45f}
        }),
        makePreset("Fourth Below", bank, {
            {"chainSlot0", 9.0f}, {"grainEnabled", 1.0f},
            {"grainSize", 70.0f}, {"grainDensity", 7.0f},
            {"grainPitch", -5.0f}, {"grainScatter", 0.3f}, {"grainMix", 0.5f}
        }),
        makePreset("Shimmer Halo", bank, {
            {"chainSlot0", 9.0f}, {"chainSlot1", 6.0f},
            {"grainEnabled", 1.0f}, {"reverbEnabled", 1.0f},
            {"grainSize", 50.0f}, {"grainDensity", 8.0f},
            {"grainPitch", 12.0f}, {"grainScatter", 0.35f}, {"grainMix", 0.4f},
            {"reverbMix", 0.5f}, {"reverbDecay", 0.7f}, {"reverbSize", 0.7f}
        }),

        // --- Dense / Swarm Textures ---
        makePreset("Dense Swarm", bank, {
            {"chainSlot0", 9.0f}, {"grainEnabled", 1.0f},
            {"grainSize", 15.0f}, {"grainDensity", 16.0f},
            {"grainPitch", 0.0f}, {"grainScatter", 0.6f}, {"grainMix", 0.7f}
        }),
        makePreset("Micro Grains", bank, {
            {"chainSlot0", 9.0f}, {"grainEnabled", 1.0f},
            {"grainSize", 5.0f}, {"grainDensity", 16.0f},
            {"grainPitch", 0.0f}, {"grainScatter", 0.4f}, {"grainMix", 0.6f}
        }),
        makePreset("Insect Cloud", bank, {
            {"chainSlot0", 9.0f}, {"grainEnabled", 1.0f},
            {"grainSize", 8.0f}, {"grainDensity", 14.0f},
            {"grainPitch", 5.0f}, {"grainScatter", 0.7f}, {"grainMix", 0.55f}
        }),
        makePreset("Particle Storm", bank, {
            {"chainSlot0", 9.0f}, {"grainEnabled", 1.0f},
            {"grainSize", 10.0f}, {"grainDensity", 16.0f},
            {"grainPitch", 0.0f}, {"grainScatter", 0.8f}, {"grainMix", 0.75f}
        }),
        makePreset("Buzzing Hive", bank, {
            {"chainSlot0", 9.0f}, {"grainEnabled", 1.0f},
            {"grainSize", 12.0f}, {"grainDensity", 14.0f},
            {"grainPitch", 3.0f}, {"grainScatter", 0.65f}, {"grainMix", 0.6f}
        }),

        // --- Sparse / Droplet Textures ---
        makePreset("Sparse Droplets", bank, {
            {"chainSlot0", 9.0f}, {"grainEnabled", 1.0f},
            {"grainSize", 30.0f}, {"grainDensity", 1.5f},
            {"grainPitch", 0.0f}, {"grainScatter", 0.8f}, {"grainMix", 0.6f}
        }),
        makePreset("Scattered Glass", bank, {
            {"chainSlot0", 9.0f}, {"grainEnabled", 1.0f},
            {"grainSize", 20.0f}, {"grainDensity", 2.0f},
            {"grainPitch", 7.0f}, {"grainScatter", 0.9f}, {"grainMix", 0.5f}
        }),
        makePreset("Rain On Tin", bank, {
            {"chainSlot0", 9.0f}, {"grainEnabled", 1.0f},
            {"grainSize", 10.0f}, {"grainDensity", 3.0f},
            {"grainPitch", 5.0f}, {"grainScatter", 0.7f}, {"grainMix", 0.55f}
        }),
        makePreset("Dripping Cave", bank, {
            {"chainSlot0", 9.0f}, {"chainSlot1", 6.0f},
            {"grainEnabled", 1.0f}, {"reverbEnabled", 1.0f},
            {"grainSize", 15.0f}, {"grainDensity", 2.0f},
            {"grainPitch", 0.0f}, {"grainScatter", 0.85f}, {"grainMix", 0.5f},
            {"reverbMix", 0.6f}, {"reverbDecay", 0.75f}, {"reverbSize", 0.8f}
        }),
        makePreset("Pebble Scatter", bank, {
            {"chainSlot0", 9.0f}, {"grainEnabled", 1.0f},
            {"grainSize", 25.0f}, {"grainDensity", 2.5f},
            {"grainPitch", -2.0f}, {"grainScatter", 0.75f}, {"grainMix", 0.5f}
        }),

        // --- Frozen / Sustained ---
        makePreset("Frozen Cathedral", bank, {
            {"chainSlot0", 9.0f}, {"chainSlot1", 6.0f},
            {"grainEnabled", 1.0f}, {"reverbEnabled", 1.0f},
            {"grainSize", 150.0f}, {"grainDensity", 8.0f},
            {"grainPitch", 0.0f}, {"grainScatter", 0.2f}, {"grainMix", 0.8f},
            {"reverbMix", 0.6f}, {"reverbDecay", 0.85f}, {"reverbSize", 0.9f},
            {"reverbDiffusion", 0.9f}
        }),
        makePreset("Frozen Void", bank, {
            {"chainSlot0", 9.0f}, {"grainEnabled", 1.0f},
            {"grainSize", 200.0f}, {"grainDensity", 10.0f},
            {"grainPitch", 0.0f}, {"grainScatter", 0.1f}, {"grainMix", 0.9f}
        }),
        makePreset("Time Stop", bank, {
            {"chainSlot0", 9.0f}, {"grainEnabled", 1.0f},
            {"grainSize", 180.0f}, {"grainDensity", 12.0f},
            {"grainPitch", 0.0f}, {"grainScatter", 0.05f}, {"grainMix", 0.85f}
        }),
        makePreset("Infinite Hold", bank, {
            {"chainSlot0", 9.0f}, {"chainSlot1", 6.0f},
            {"grainEnabled", 1.0f}, {"reverbEnabled", 1.0f},
            {"grainSize", 200.0f}, {"grainDensity", 14.0f},
            {"grainPitch", 0.0f}, {"grainScatter", 0.08f}, {"grainMix", 0.9f},
            {"reverbMix", 0.5f}, {"reverbDecay", 0.95f}, {"reverbSize", 0.95f}
        }),
        makePreset("Suspended Animation", bank, {
            {"chainSlot0", 9.0f}, {"grainEnabled", 1.0f},
            {"grainSize", 160.0f}, {"grainDensity", 10.0f},
            {"grainPitch", 0.0f}, {"grainScatter", 0.15f}, {"grainMix", 0.8f}
        }),

        // --- Portal-Style Creative ---
        makePreset("Portal Drift", bank, {
            {"chainSlot0", 9.0f}, {"chainSlot1", 6.0f},
            {"grainEnabled", 1.0f}, {"reverbEnabled", 1.0f},
            {"grainSize", 45.0f}, {"grainDensity", 6.0f},
            {"grainPitch", 5.0f}, {"grainScatter", 0.5f}, {"grainMix", 0.6f},
            {"reverbMix", 0.4f}, {"reverbDecay", 0.6f}, {"reverbSize", 0.65f}
        }),
        makePreset("Dimensional Tear", bank, {
            {"chainSlot0", 9.0f}, {"chainSlot1", 11.0f},
            {"grainEnabled", 1.0f}, {"shiftEnabled", 1.0f},
            {"grainSize", 35.0f}, {"grainDensity", 8.0f},
            {"grainPitch", -7.0f}, {"grainScatter", 0.6f}, {"grainMix", 0.7f},
            {"shiftPitch", 12.0f}, {"shiftMix", 0.3f}, {"shiftFeedback", 0.2f}
        }),
        makePreset("Wormhole", bank, {
            {"chainSlot0", 9.0f}, {"chainSlot1", 11.0f}, {"chainSlot2", 6.0f},
            {"grainEnabled", 1.0f}, {"shiftEnabled", 1.0f}, {"reverbEnabled", 1.0f},
            {"grainSize", 25.0f}, {"grainDensity", 10.0f},
            {"grainPitch", 12.0f}, {"grainScatter", 0.7f}, {"grainMix", 0.65f},
            {"shiftPitch", -12.0f}, {"shiftMix", 0.25f}, {"shiftFeedback", 0.3f},
            {"reverbMix", 0.5f}, {"reverbDecay", 0.8f}
        }),
        makePreset("Event Horizon", bank, {
            {"chainSlot0", 9.0f}, {"chainSlot1", 6.0f},
            {"grainEnabled", 1.0f}, {"reverbEnabled", 1.0f},
            {"grainSize", 100.0f}, {"grainDensity", 12.0f},
            {"grainPitch", -12.0f}, {"grainScatter", 0.4f}, {"grainMix", 0.75f},
            {"reverbMix", 0.7f}, {"reverbDecay", 0.9f}, {"reverbSize", 0.85f}
        }),
        makePreset("Nebula Drift", bank, {
            {"chainSlot0", 9.0f}, {"chainSlot1", 8.0f},
            {"grainEnabled", 1.0f}, {"unisonEnabled", 1.0f},
            {"grainSize", 70.0f}, {"grainDensity", 6.0f},
            {"grainPitch", 0.0f}, {"grainScatter", 0.45f}, {"grainMix", 0.6f},
            {"unisonMix", 0.4f}, {"unisonSpeed", 0.8f}
        }),

        // --- Grain + Chorus/Multiply ---
        makePreset("Grain Ensemble", bank, {
            {"chainSlot0", 9.0f}, {"chainSlot1", 3.0f},
            {"grainEnabled", 1.0f},
            {"grainSize", 50.0f}, {"grainDensity", 8.0f},
            {"grainPitch", 0.0f}, {"grainScatter", 0.3f}, {"grainMix", 0.6f},
            {"chorusMix", 0.5f}, {"junoMode", 2.0f}
        }),
        makePreset("Thickened Grains", bank, {
            {"chainSlot0", 9.0f}, {"chainSlot1", 8.0f},
            {"grainEnabled", 1.0f}, {"unisonEnabled", 1.0f},
            {"grainSize", 40.0f}, {"grainDensity", 10.0f},
            {"grainPitch", 0.0f}, {"grainScatter", 0.25f}, {"grainMix", 0.55f},
            {"unisonMix", 0.5f}, {"unisonSpeed", 1.2f}
        }),
        makePreset("Wide Grain Pad", bank, {
            {"chainSlot0", 9.0f}, {"chainSlot1", 8.0f}, {"chainSlot2", 6.0f},
            {"grainEnabled", 1.0f}, {"unisonEnabled", 1.0f}, {"reverbEnabled", 1.0f},
            {"grainSize", 90.0f}, {"grainDensity", 8.0f},
            {"grainPitch", 0.0f}, {"grainScatter", 0.3f}, {"grainMix", 0.65f},
            {"unisonMix", 0.4f}, {"unisonOuter", 1.0f}, {"unisonInner", 0.5f},
            {"reverbMix", 0.35f}, {"reverbDecay", 0.55f}
        }),
        makePreset("Stereo Scatter", bank, {
            {"chainSlot0", 9.0f}, {"grainEnabled", 1.0f},
            {"grainSize", 30.0f}, {"grainDensity", 6.0f},
            {"grainPitch", 0.0f}, {"grainScatter", 0.9f}, {"grainMix", 0.7f},
            {"msWidth", 1.5f}
        }),
        makePreset("Chorus Cloud", bank, {
            {"chainSlot0", 9.0f}, {"chainSlot1", 3.0f},
            {"grainEnabled", 1.0f},
            {"grainSize", 80.0f}, {"grainDensity", 6.0f},
            {"grainPitch", 0.0f}, {"grainScatter", 0.35f}, {"grainMix", 0.55f},
            {"chorusMix", 0.6f}, {"junoMode", 1.0f}
        }),

        // --- Textured / Organic ---
        makePreset("Organic Texture", bank, {
            {"chainSlot0", 9.0f}, {"grainEnabled", 1.0f},
            {"grainSize", 55.0f}, {"grainDensity", 5.0f},
            {"grainPitch", 0.0f}, {"grainScatter", 0.55f}, {"grainMix", 0.5f}
        }),
        makePreset("Forest Floor", bank, {
            {"chainSlot0", 9.0f}, {"chainSlot1", 7.0f},
            {"grainEnabled", 1.0f}, {"hazeEnabled", 1.0f},
            {"grainSize", 65.0f}, {"grainDensity", 4.0f},
            {"grainPitch", -3.0f}, {"grainScatter", 0.6f}, {"grainMix", 0.55f},
            {"hazeType", 0.0f}, {"hazeMix", 0.1f}
        }),
        makePreset("Wind Chimes", bank, {
            {"chainSlot0", 9.0f}, {"chainSlot1", 6.0f},
            {"grainEnabled", 1.0f}, {"reverbEnabled", 1.0f},
            {"grainSize", 15.0f}, {"grainDensity", 3.0f},
            {"grainPitch", 12.0f}, {"grainScatter", 0.8f}, {"grainMix", 0.4f},
            {"reverbMix", 0.5f}, {"reverbDecay", 0.65f}, {"reverbSize", 0.7f}
        }),
        makePreset("Grain Breath", bank, {
            {"chainSlot0", 9.0f}, {"grainEnabled", 1.0f},
            {"grainSize", 35.0f}, {"grainDensity", 7.0f},
            {"grainPitch", 0.0f}, {"grainScatter", 0.4f}, {"grainMix", 0.45f}
        }),
        makePreset("Silk Threads", bank, {
            {"chainSlot0", 9.0f}, {"grainEnabled", 1.0f},
            {"grainSize", 45.0f}, {"grainDensity", 12.0f},
            {"grainPitch", 0.0f}, {"grainScatter", 0.2f}, {"grainMix", 0.4f}
        }),

        // --- Pitch Experiments ---
        makePreset("Detuned Cloud", bank, {
            {"chainSlot0", 9.0f}, {"grainEnabled", 1.0f},
            {"grainSize", 60.0f}, {"grainDensity", 8.0f},
            {"grainPitch", 0.5f}, {"grainScatter", 0.4f}, {"grainMix", 0.6f}
        }),
        makePreset("Minor Third Haze", bank, {
            {"chainSlot0", 9.0f}, {"grainEnabled", 1.0f},
            {"grainSize", 50.0f}, {"grainDensity", 6.0f},
            {"grainPitch", 3.0f}, {"grainScatter", 0.35f}, {"grainMix", 0.5f}
        }),
        makePreset("Suboctave Bloom", bank, {
            {"chainSlot0", 9.0f}, {"chainSlot1", 6.0f},
            {"grainEnabled", 1.0f}, {"reverbEnabled", 1.0f},
            {"grainSize", 100.0f}, {"grainDensity", 6.0f},
            {"grainPitch", -12.0f}, {"grainScatter", 0.3f}, {"grainMix", 0.6f},
            {"reverbMix", 0.4f}, {"reverbDecay", 0.6f}
        }),
        makePreset("Two Octave Spread", bank, {
            {"chainSlot0", 9.0f}, {"chainSlot1", 11.0f},
            {"grainEnabled", 1.0f}, {"shiftEnabled", 1.0f},
            {"grainSize", 60.0f}, {"grainDensity", 8.0f},
            {"grainPitch", 12.0f}, {"grainScatter", 0.3f}, {"grainMix", 0.5f},
            {"shiftPitch", -12.0f}, {"shiftMix", 0.4f}
        }),
        makePreset("Tritone Tension", bank, {
            {"chainSlot0", 9.0f}, {"grainEnabled", 1.0f},
            {"grainSize", 40.0f}, {"grainDensity", 10.0f},
            {"grainPitch", 6.0f}, {"grainScatter", 0.5f}, {"grainMix", 0.55f}
        }),

        // --- Subtle / Mix-Ready ---
        makePreset("Gentle Grain", bank, {
            {"chainSlot0", 9.0f}, {"grainEnabled", 1.0f},
            {"grainSize", 80.0f}, {"grainDensity", 8.0f},
            {"grainPitch", 0.0f}, {"grainScatter", 0.2f}, {"grainMix", 0.25f}
        }),
        makePreset("Background Texture", bank, {
            {"chainSlot0", 9.0f}, {"grainEnabled", 1.0f},
            {"grainSize", 100.0f}, {"grainDensity", 6.0f},
            {"grainPitch", 0.0f}, {"grainScatter", 0.3f}, {"grainMix", 0.2f}
        }),
        makePreset("Soft Focus", bank, {
            {"chainSlot0", 9.0f}, {"grainEnabled", 1.0f},
            {"grainSize", 120.0f}, {"grainDensity", 10.0f},
            {"grainPitch", 0.0f}, {"grainScatter", 0.15f}, {"grainMix", 0.3f}
        }),
        makePreset("Grain Veil", bank, {
            {"chainSlot0", 9.0f}, {"grainEnabled", 1.0f},
            {"grainSize", 70.0f}, {"grainDensity", 8.0f},
            {"grainPitch", 0.0f}, {"grainScatter", 0.25f}, {"grainMix", 0.2f}
        }),
        makePreset("Diffused Signal", bank, {
            {"chainSlot0", 9.0f}, {"chainSlot1", 6.0f},
            {"grainEnabled", 1.0f}, {"reverbEnabled", 1.0f},
            {"grainSize", 90.0f}, {"grainDensity", 10.0f},
            {"grainPitch", 0.0f}, {"grainScatter", 0.2f}, {"grainMix", 0.3f},
            {"reverbMix", 0.25f}, {"reverbDecay", 0.4f}, {"reverbDiffusion", 0.9f}
        }),
    };
}
