#include "FactoryPresets.h"

// Chain slot IDs: 0=empty, 1=CRUSH, 3=CHORUS, 4=TAPE, 5=SATURATE,
//                 6=REVERB, 7=HAZE, 8=MULTIPLY, 10=STUTTER,
//                 11=SHIFT, 12=REVERSER

std::vector<Preset> getTapePresets()
{
    const juce::String bank = "Tape";
    return {
        // --- Reel-to-Reel Machines ---
        makePreset("Reel To Reel", bank, {
            {"chainSlot0", 4.0f}, {"tapeEnabled", 1.0f},
            {"tapeDrive", 0.2f}, {"tapeGlue", 0.5f}, {"tapeWow", 0.05f},
            {"tapeNoise", 0.06f}, {"tapeNoiseSpeed", 1.0f}, {"tapeMix", 1.0f}
        }),
        makePreset("Ampex 456", bank, {
            {"chainSlot0", 4.0f}, {"tapeEnabled", 1.0f},
            {"tapeDrive", 0.25f}, {"tapeGlue", 0.55f}, {"tapeWow", 0.04f},
            {"tapeNoise", 0.05f}, {"tapeNoiseSpeed", 2.0f}, {"tapeMix", 1.0f},
            {"tapeEQStandard", 0.0f}
        }),
        makePreset("Studer A80", bank, {
            {"chainSlot0", 4.0f}, {"tapeEnabled", 1.0f},
            {"tapeDrive", 0.18f}, {"tapeGlue", 0.45f}, {"tapeWow", 0.03f},
            {"tapeNoise", 0.04f}, {"tapeNoiseSpeed", 2.0f}, {"tapeMix", 1.0f}
        }),
        makePreset("SM900 Clarity", bank, {
            {"chainSlot0", 4.0f}, {"tapeEnabled", 1.0f},
            {"tapeDrive", 0.15f}, {"tapeGlue", 0.4f}, {"tapeWow", 0.02f},
            {"tapeNoise", 0.03f}, {"tapeNoiseSpeed", 2.0f}, {"tapeMix", 1.0f}
        }),
        makePreset("30 IPS Punch", bank, {
            {"chainSlot0", 4.0f}, {"tapeEnabled", 1.0f},
            {"tapeDrive", 0.35f}, {"tapeGlue", 0.6f}, {"tapeWow", 0.02f},
            {"tapeNoise", 0.04f}, {"tapeNoiseSpeed", 2.0f}, {"tapeMix", 1.0f},
            {"tapeInput", 3.0f}
        }),

        // --- Tape Warmth / Saturation ---
        makePreset("Tape Warmth", bank, {
            {"chainSlot0", 4.0f}, {"tapeEnabled", 1.0f},
            {"tapeDrive", 0.3f}, {"tapeGlue", 0.55f}, {"tapeWow", 0.05f},
            {"tapeNoise", 0.05f}, {"tapeMix", 0.8f}
        }),
        makePreset("Tape Saturation", bank, {
            {"chainSlot0", 4.0f}, {"tapeEnabled", 1.0f},
            {"tapeDrive", 0.5f}, {"tapeGlue", 0.65f}, {"tapeWow", 0.04f},
            {"tapeNoise", 0.06f}, {"tapeInput", 4.0f}, {"tapeMix", 1.0f}
        }),
        makePreset("Hot Tape", bank, {
            {"chainSlot0", 4.0f}, {"tapeEnabled", 1.0f},
            {"tapeDrive", 0.7f}, {"tapeGlue", 0.7f}, {"tapeWow", 0.05f},
            {"tapeNoise", 0.08f}, {"tapeInput", 6.0f}, {"tapeOutput", -3.0f},
            {"tapeMix", 1.0f}
        }),
        makePreset("Tape Overdrive", bank, {
            {"chainSlot0", 4.0f}, {"tapeEnabled", 1.0f},
            {"tapeDrive", 0.85f}, {"tapeGlue", 0.8f}, {"tapeWow", 0.06f},
            {"tapeNoise", 0.1f}, {"tapeInput", 8.0f}, {"tapeOutput", -5.0f},
            {"tapeMix", 1.0f}
        }),
        makePreset("Gentle Saturation", bank, {
            {"chainSlot0", 4.0f}, {"tapeEnabled", 1.0f},
            {"tapeDrive", 0.22f}, {"tapeGlue", 0.45f}, {"tapeWow", 0.03f},
            {"tapeNoise", 0.04f}, {"tapeMix", 0.6f}
        }),

        // --- Cassette ---
        makePreset("Old Cassette", bank, {
            {"chainSlot0", 4.0f}, {"tapeEnabled", 1.0f},
            {"tapeDrive", 0.3f}, {"tapeGlue", 0.6f}, {"tapeWow", 0.2f},
            {"tapeNoise", 0.15f}, {"tapeNoiseSpeed", 0.0f}, {"tapeMix", 1.0f}
        }),
        makePreset("Chrome Cassette", bank, {
            {"chainSlot0", 4.0f}, {"tapeEnabled", 1.0f},
            {"tapeDrive", 0.2f}, {"tapeGlue", 0.5f}, {"tapeWow", 0.12f},
            {"tapeNoise", 0.1f}, {"tapeNoiseSpeed", 0.0f}, {"tapeMix", 1.0f},
            {"tapeEQStandard", 1.0f}
        }),
        makePreset("Type I Ferro", bank, {
            {"chainSlot0", 4.0f}, {"tapeEnabled", 1.0f},
            {"tapeDrive", 0.35f}, {"tapeGlue", 0.65f}, {"tapeWow", 0.18f},
            {"tapeNoise", 0.18f}, {"tapeNoiseSpeed", 0.0f}, {"tapeMix", 1.0f}
        }),
        makePreset("Worn Cassette", bank, {
            {"chainSlot0", 4.0f}, {"tapeEnabled", 1.0f},
            {"tapeDrive", 0.4f}, {"tapeGlue", 0.7f}, {"tapeWow", 0.3f},
            {"tapeNoise", 0.2f}, {"tapeNoiseSpeed", 0.0f}, {"tapeMix", 1.0f},
            {"tapePrintThrough", 0.5f}
        }),
        makePreset("Boombox", bank, {
            {"chainSlot0", 4.0f}, {"tapeEnabled", 1.0f},
            {"tapeDrive", 0.45f}, {"tapeGlue", 0.6f}, {"tapeWow", 0.15f},
            {"tapeNoise", 0.12f}, {"tapeNoiseSpeed", 0.0f},
            {"tapeInput", 3.0f}, {"tapeMix", 1.0f}
        }),

        // --- Wobble / Flutter ---
        makePreset("Wobble Machine", bank, {
            {"chainSlot0", 4.0f}, {"tapeEnabled", 1.0f},
            {"tapeDrive", 0.2f}, {"tapeGlue", 0.5f}, {"tapeWow", 0.4f},
            {"tapeNoise", 0.08f}, {"tapeMix", 1.0f}
        }),
        makePreset("Warped Tape", bank, {
            {"chainSlot0", 4.0f}, {"tapeEnabled", 1.0f},
            {"tapeDrive", 0.25f}, {"tapeGlue", 0.55f}, {"tapeWow", 0.6f},
            {"tapeNoise", 0.1f}, {"tapeMix", 1.0f}
        }),
        makePreset("Seasick Reel", bank, {
            {"chainSlot0", 4.0f}, {"tapeEnabled", 1.0f},
            {"tapeDrive", 0.3f}, {"tapeGlue", 0.6f}, {"tapeWow", 0.8f},
            {"tapeNoise", 0.12f}, {"tapeMix", 0.9f}
        }),
        makePreset("Gentle Flutter", bank, {
            {"chainSlot0", 4.0f}, {"tapeEnabled", 1.0f},
            {"tapeDrive", 0.15f}, {"tapeGlue", 0.4f}, {"tapeWow", 0.15f},
            {"tapeNoise", 0.05f}, {"tapeMix", 0.7f}
        }),
        makePreset("Slow Drift", bank, {
            {"chainSlot0", 4.0f}, {"tapeEnabled", 1.0f},
            {"tapeDrive", 0.2f}, {"tapeGlue", 0.5f}, {"tapeWow", 0.25f},
            {"tapeNoise", 0.06f}, {"tapeMix", 0.85f}
        }),

        // --- Tape + Haze (Noise Texture) ---
        makePreset("Hiss Machine", bank, {
            {"chainSlot0", 4.0f}, {"chainSlot1", 7.0f},
            {"tapeEnabled", 1.0f}, {"hazeEnabled", 1.0f},
            {"tapeDrive", 0.25f}, {"tapeGlue", 0.5f}, {"tapeWow", 0.06f},
            {"tapeNoise", 0.2f}, {"tapeMix", 1.0f},
            {"hazeType", 0.0f}, {"hazeMix", 0.15f}
        }),
        makePreset("Vinyl On Tape", bank, {
            {"chainSlot0", 4.0f}, {"chainSlot1", 7.0f},
            {"tapeEnabled", 1.0f}, {"hazeEnabled", 1.0f},
            {"tapeDrive", 0.2f}, {"tapeGlue", 0.5f}, {"tapeWow", 0.08f},
            {"tapeNoise", 0.1f}, {"tapeMix", 0.9f},
            {"hazeType", 2.0f}, {"hazeMix", 0.12f}
        }),
        makePreset("Noisy Reel", bank, {
            {"chainSlot0", 4.0f}, {"chainSlot1", 7.0f},
            {"tapeEnabled", 1.0f}, {"hazeEnabled", 1.0f},
            {"tapeDrive", 0.3f}, {"tapeGlue", 0.55f}, {"tapeWow", 0.05f},
            {"tapeNoise", 0.15f}, {"tapeMix", 1.0f},
            {"hazeType", 1.0f}, {"hazeMix", 0.1f}
        }),
        makePreset("Tape Room Tone", bank, {
            {"chainSlot0", 4.0f}, {"chainSlot1", 7.0f},
            {"tapeEnabled", 1.0f}, {"hazeEnabled", 1.0f},
            {"tapeDrive", 0.17f}, {"tapeGlue", 0.45f}, {"tapeWow", 0.04f},
            {"tapeNoise", 0.08f}, {"tapeMix", 0.7f},
            {"hazeType", 0.0f}, {"hazeMix", 0.08f}, {"hazeColor", 0.3f}
        }),

        // --- Mastering / Glue ---
        makePreset("Mastering Glue", bank, {
            {"chainSlot0", 4.0f}, {"tapeEnabled", 1.0f},
            {"tapeDrive", 0.12f}, {"tapeGlue", 0.7f}, {"tapeWow", 0.01f},
            {"tapeNoise", 0.02f}, {"tapeNoiseSpeed", 2.0f},
            {"tapeMix", 0.5f}
        }),
        makePreset("Bus Glue", bank, {
            {"chainSlot0", 4.0f}, {"tapeEnabled", 1.0f},
            {"tapeDrive", 0.15f}, {"tapeGlue", 0.65f}, {"tapeWow", 0.02f},
            {"tapeNoise", 0.03f}, {"tapeNoiseSpeed", 2.0f},
            {"tapeMix", 0.6f}
        }),
        makePreset("Subtle Tape Color", bank, {
            {"chainSlot0", 4.0f}, {"tapeEnabled", 1.0f},
            {"tapeDrive", 0.1f}, {"tapeGlue", 0.4f}, {"tapeWow", 0.01f},
            {"tapeNoise", 0.02f}, {"tapeMix", 0.35f}
        }),
        makePreset("Analog Summing", bank, {
            {"chainSlot0", 4.0f}, {"tapeEnabled", 1.0f},
            {"tapeDrive", 0.08f}, {"tapeGlue", 0.5f}, {"tapeWow", 0.01f},
            {"tapeNoise", 0.01f}, {"tapeNoiseSpeed", 2.0f},
            {"tapeMix", 0.4f}
        }),
        makePreset("Tape Polish", bank, {
            {"chainSlot0", 4.0f}, {"tapeEnabled", 1.0f},
            {"tapeDrive", 0.14f}, {"tapeGlue", 0.55f}, {"tapeWow", 0.02f},
            {"tapeNoise", 0.03f}, {"tapeMix", 0.45f}
        }),

        // --- EQ Standards ---
        makePreset("IEC Broadcast", bank, {
            {"chainSlot0", 4.0f}, {"tapeEnabled", 1.0f},
            {"tapeDrive", 0.2f}, {"tapeGlue", 0.5f}, {"tapeWow", 0.03f},
            {"tapeNoise", 0.05f}, {"tapeNoiseSpeed", 1.0f},
            {"tapeEQStandard", 1.0f}, {"tapeMix", 1.0f}
        }),
        makePreset("NAB Studio", bank, {
            {"chainSlot0", 4.0f}, {"tapeEnabled", 1.0f},
            {"tapeDrive", 0.22f}, {"tapeGlue", 0.5f}, {"tapeWow", 0.04f},
            {"tapeNoise", 0.05f}, {"tapeNoiseSpeed", 1.0f},
            {"tapeEQStandard", 0.0f}, {"tapeMix", 1.0f}
        }),

        // --- Print-Through ---
        makePreset("Ghost Print", bank, {
            {"chainSlot0", 4.0f}, {"tapeEnabled", 1.0f},
            {"tapeDrive", 0.2f}, {"tapeGlue", 0.5f}, {"tapeWow", 0.05f},
            {"tapeNoise", 0.06f}, {"tapePrintThrough", 0.7f}, {"tapeMix", 1.0f}
        }),
        makePreset("Echo Bleed", bank, {
            {"chainSlot0", 4.0f}, {"tapeEnabled", 1.0f},
            {"tapeDrive", 0.25f}, {"tapeGlue", 0.55f}, {"tapeWow", 0.06f},
            {"tapeNoise", 0.07f}, {"tapePrintThrough", 0.9f}, {"tapeMix", 1.0f}
        }),

        // --- Tape + Distortion Combos ---
        makePreset("Driven Reel", bank, {
            {"chainSlot0", 4.0f}, {"chainSlot1", 5.0f},
            {"tapeEnabled", 1.0f}, {"distortionEnabled", 1.0f},
            {"tapeDrive", 0.4f}, {"tapeGlue", 0.6f}, {"tapeWow", 0.04f},
            {"tapeMix", 1.0f},
            {"distortionDrive", 0.35f}, {"distortionTone", 0.5f},
            {"distortionMode", 1.0f}, {"distortionMix", 0.5f}
        }),
        makePreset("Saturated Cassette", bank, {
            {"chainSlot0", 4.0f}, {"chainSlot1", 5.0f},
            {"tapeEnabled", 1.0f}, {"distortionEnabled", 1.0f},
            {"tapeDrive", 0.45f}, {"tapeGlue", 0.65f}, {"tapeWow", 0.15f},
            {"tapeNoise", 0.12f}, {"tapeNoiseSpeed", 0.0f}, {"tapeMix", 1.0f},
            {"distortionDrive", 0.4f}, {"distortionTone", 0.45f},
            {"distortionMode", 0.0f}, {"distortionMix", 0.6f}
        }),
        makePreset("Tube Through Tape", bank, {
            {"chainSlot0", 5.0f}, {"chainSlot1", 4.0f},
            {"distortionEnabled", 1.0f}, {"tapeEnabled", 1.0f},
            {"distortionDrive", 0.3f}, {"distortionTone", 0.55f},
            {"distortionMode", 1.0f}, {"distortionMix", 0.7f},
            {"tapeDrive", 0.2f}, {"tapeGlue", 0.5f}, {"tapeWow", 0.05f},
            {"tapeMix", 0.8f}
        }),

        // --- Tape + Chorus ---
        makePreset("Chorus Tape", bank, {
            {"chainSlot0", 4.0f}, {"chainSlot1", 3.0f},
            {"tapeEnabled", 1.0f},
            {"tapeDrive", 0.2f}, {"tapeGlue", 0.5f}, {"tapeWow", 0.06f},
            {"tapeNoise", 0.05f}, {"tapeMix", 0.9f},
            {"chorusMix", 0.4f}, {"junoMode", 0.0f}
        }),
        makePreset("Ensemble Reel", bank, {
            {"chainSlot0", 4.0f}, {"chainSlot1", 3.0f},
            {"tapeEnabled", 1.0f},
            {"tapeDrive", 0.25f}, {"tapeGlue", 0.55f}, {"tapeWow", 0.05f},
            {"tapeMix", 1.0f},
            {"chorusMix", 0.5f}, {"junoMode", 2.0f}
        }),

        // --- Extreme Tape ---
        makePreset("Destroyed Tape", bank, {
            {"chainSlot0", 4.0f}, {"tapeEnabled", 1.0f},
            {"tapeDrive", 1.0f}, {"tapeGlue", 0.9f}, {"tapeWow", 0.5f},
            {"tapeNoise", 0.3f}, {"tapeInput", 10.0f}, {"tapeOutput", -6.0f},
            {"tapePrintThrough", 0.8f}, {"tapeMix", 1.0f}
        }),
        makePreset("Melting Reel", bank, {
            {"chainSlot0", 4.0f}, {"tapeEnabled", 1.0f},
            {"tapeDrive", 0.8f}, {"tapeGlue", 0.85f}, {"tapeWow", 0.7f},
            {"tapeNoise", 0.2f}, {"tapeInput", 6.0f}, {"tapeOutput", -4.0f},
            {"tapeMix", 1.0f}
        }),
        makePreset("Tape Meltdown", bank, {
            {"chainSlot0", 4.0f}, {"chainSlot1", 7.0f},
            {"tapeEnabled", 1.0f}, {"hazeEnabled", 1.0f},
            {"tapeDrive", 0.9f}, {"tapeGlue", 0.9f}, {"tapeWow", 0.9f},
            {"tapeNoise", 0.25f}, {"tapeInput", 8.0f}, {"tapeOutput", -5.0f},
            {"tapeMix", 1.0f},
            {"hazeType", 0.0f}, {"hazeMix", 0.2f}
        }),
        makePreset("Eaten By Machine", bank, {
            {"chainSlot0", 4.0f}, {"tapeEnabled", 1.0f},
            {"tapeDrive", 0.75f}, {"tapeGlue", 0.8f}, {"tapeWow", 1.0f},
            {"tapeNoise", 0.15f}, {"tapeMix", 1.0f},
            {"tapeMotorEnabled", 0.0f}
        }),

        // --- Motor / Speed Effects ---
        makePreset("Motor Off Decay", bank, {
            {"chainSlot0", 4.0f}, {"tapeEnabled", 1.0f},
            {"tapeDrive", 0.2f}, {"tapeGlue", 0.5f}, {"tapeWow", 0.1f},
            {"tapeNoise", 0.06f}, {"tapeMix", 1.0f},
            {"tapeMotorEnabled", 0.0f}
        }),
        makePreset("7.5 IPS Vintage", bank, {
            {"chainSlot0", 4.0f}, {"tapeEnabled", 1.0f},
            {"tapeDrive", 0.25f}, {"tapeGlue", 0.55f}, {"tapeWow", 0.1f},
            {"tapeNoise", 0.1f}, {"tapeNoiseSpeed", 0.0f}, {"tapeMix", 1.0f}
        }),

        // --- Additional Tape ---
        makePreset("Tape Reverb", bank, {
            {"chainSlot0", 4.0f}, {"chainSlot1", 6.0f},
            {"tapeEnabled", 1.0f}, {"reverbEnabled", 1.0f},
            {"tapeDrive", 0.2f}, {"tapeGlue", 0.5f}, {"tapeWow", 0.05f},
            {"tapeMix", 0.8f},
            {"reverbMix", 0.4f}, {"reverbDecay", 0.55f}, {"reverbSize", 0.6f}
        }),
        makePreset("Tape Shimmer", bank, {
            {"chainSlot0", 4.0f}, {"chainSlot1", 11.0f}, {"chainSlot2", 6.0f},
            {"tapeEnabled", 1.0f}, {"shiftEnabled", 1.0f}, {"reverbEnabled", 1.0f},
            {"tapeDrive", 0.18f}, {"tapeGlue", 0.5f}, {"tapeWow", 0.04f}, {"tapeMix", 0.7f},
            {"shiftPitch", 12.0f}, {"shiftFeedback", 0.3f}, {"shiftMix", 0.2f},
            {"reverbMix", 0.45f}, {"reverbDecay", 0.7f}
        }),
        makePreset("15 IPS Broadcast", bank, {
            {"chainSlot0", 4.0f}, {"tapeEnabled", 1.0f},
            {"tapeDrive", 0.2f}, {"tapeGlue", 0.5f}, {"tapeWow", 0.04f},
            {"tapeNoise", 0.05f}, {"tapeNoiseSpeed", 1.0f},
            {"tapeInput", 2.0f}, {"tapeMix", 1.0f}
        }),
        makePreset("Reel Wobble Pad", bank, {
            {"chainSlot0", 4.0f}, {"chainSlot1", 8.0f},
            {"tapeEnabled", 1.0f}, {"unisonEnabled", 1.0f},
            {"tapeDrive", 0.22f}, {"tapeGlue", 0.5f}, {"tapeWow", 0.12f},
            {"tapeMix", 0.8f},
            {"unisonMix", 0.4f}, {"unisonSpeed", 0.8f}
        }),
        makePreset("Tape Delay Feel", bank, {
            {"chainSlot0", 4.0f}, {"tapeEnabled", 1.0f},
            {"tapeDrive", 0.2f}, {"tapeGlue", 0.5f}, {"tapeWow", 0.08f},
            {"tapeNoise", 0.06f}, {"tapePrintThrough", 0.6f},
            {"tapeMix", 0.7f}
        }),
        makePreset("Warm And Wide", bank, {
            {"chainSlot0", 4.0f}, {"chainSlot1", 8.0f},
            {"tapeEnabled", 1.0f}, {"unisonEnabled", 1.0f},
            {"tapeDrive", 0.2f}, {"tapeGlue", 0.5f}, {"tapeWow", 0.05f},
            {"tapeMix", 0.8f},
            {"unisonMix", 0.5f}, {"unisonSpeed", 1.0f},
            {"unisonOuter", 1.0f}, {"unisonInner", 0.6f}, {"msWidth", 1.3f}
        }),
    };
}
