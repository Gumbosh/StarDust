#include "FactoryPresets.h"

// Chain slot IDs: 0=empty, 1=CRUSH, 3=CHORUS, 4=TAPE, 5=SATURATE,
//                 6=REVERB, 7=HAZE, 8=MULTIPLY, 9=GRAIN, 10=STUTTER,
//                 11=SHIFT, 12=REVERSER

std::vector<Preset> getLoFiPresets()
{
    const juce::String bank = "Lo-Fi";
    return {
        // --- SP-950 Sampler Emulations ---
        makePreset("SP-950 Classic", bank, {
            {"chainSlot0", 1.0f}, {"destroyEnabled", 1.0f},
            {"destroyFader", 26040.0f}, {"destroyBits", 12.0f},
            {"destroyIn", 6.0f}, {"destroyOut", -3.0f}, {"destroyJitter", 0.15f}
        }),
        makePreset("SP-950 Scorched", bank, {
            {"chainSlot0", 1.0f}, {"destroyEnabled", 1.0f},
            {"destroyFader", 26040.0f}, {"destroyBits", 12.0f},
            {"destroyIn", 12.0f}, {"destroyOut", -6.0f}, {"destroyJitter", 0.2f}
        }),
        makePreset("SP-950 Dark", bank, {
            {"chainSlot0", 1.0f}, {"destroyEnabled", 1.0f},
            {"destroyFader", 19000.0f}, {"destroyBits", 12.0f},
            {"destroyIn", 8.0f}, {"destroyOut", -4.0f}, {"destroyJitter", 0.18f}
        }),
        makePreset("SP-950 Bright", bank, {
            {"chainSlot0", 1.0f}, {"destroyEnabled", 1.0f},
            {"destroyFader", 32000.0f}, {"destroyBits", 12.0f},
            {"destroyIn", 4.0f}, {"destroyOut", -2.0f}, {"destroyJitter", 0.1f}
        }),
        makePreset("SP-950 Gritty", bank, {
            {"chainSlot0", 1.0f}, {"destroyEnabled", 1.0f},
            {"destroyFader", 19000.0f}, {"destroyBits", 10.0f},
            {"destroyIn", 10.0f}, {"destroyOut", -5.0f}, {"destroyJitter", 0.25f}
        }),

        // --- Vinyl / Record Degradation ---
        makePreset("Warm Vinyl", bank, {
            {"chainSlot0", 1.0f}, {"destroyEnabled", 1.0f},
            {"destroyFader", 44100.0f}, {"destroyBits", 16.0f},
            {"destroyIn", 2.0f}, {"destroyOut", -1.0f}, {"destroyMix", 0.7f},
            {"destroyJitter", 0.05f}
        }),
        makePreset("33 RPM Haze", bank, {
            {"chainSlot0", 1.0f}, {"destroyEnabled", 1.0f},
            {"destroyFader", 26040.0f}, {"destroyBits", 14.0f},
            {"destroyIn", 3.0f}, {"destroyOut", -1.5f}, {"destroyMix", 0.8f},
            {"destroyJitter", 0.08f}
        }),
        makePreset("78 RPM Shellac", bank, {
            {"chainSlot0", 1.0f}, {"destroyEnabled", 1.0f},
            {"destroyFader", 4000.0f}, {"destroyBits", 12.0f},
            {"destroyIn", 4.0f}, {"destroyOut", -2.0f}, {"destroyJitter", 0.3f}
        }),
        makePreset("Dusty Record", bank, {
            {"chainSlot0", 1.0f}, {"chainSlot1", 7.0f},
            {"destroyEnabled", 1.0f}, {"hazeEnabled", 1.0f},
            {"destroyFader", 26040.0f}, {"destroyBits", 14.0f},
            {"destroyIn", 2.0f}, {"destroyOut", -1.0f}, {"destroyMix", 0.6f},
            {"hazeType", 2.0f}, {"hazeMix", 0.12f}, {"hazeColor", 0.4f}
        }),
        makePreset("Worn Groove", bank, {
            {"chainSlot0", 1.0f}, {"chainSlot1", 7.0f},
            {"destroyEnabled", 1.0f}, {"hazeEnabled", 1.0f},
            {"destroyFader", 22000.0f}, {"destroyBits", 13.0f},
            {"destroyIn", 5.0f}, {"destroyOut", -2.5f}, {"destroyJitter", 0.12f},
            {"hazeType", 2.0f}, {"hazeMix", 0.18f}, {"hazeColor", 0.35f}
        }),

        // --- Radio / Broadcast ---
        makePreset("Lo-Fi Radio", bank, {
            {"chainSlot0", 1.0f}, {"destroyEnabled", 1.0f},
            {"destroyFader", 8000.0f}, {"destroyBits", 12.0f},
            {"destroyIn", 5.0f}, {"destroyOut", -2.0f}, {"destroyJitter", 0.2f}
        }),
        makePreset("AM Broadcast", bank, {
            {"chainSlot0", 1.0f}, {"destroyEnabled", 1.0f},
            {"destroyFader", 5000.0f}, {"destroyBits", 10.0f},
            {"destroyIn", 6.0f}, {"destroyOut", -3.0f}, {"destroyJitter", 0.25f}
        }),
        makePreset("Shortwave", bank, {
            {"chainSlot0", 1.0f}, {"chainSlot1", 7.0f},
            {"destroyEnabled", 1.0f}, {"hazeEnabled", 1.0f},
            {"destroyFader", 4000.0f}, {"destroyBits", 8.0f},
            {"destroyIn", 8.0f}, {"destroyOut", -4.0f}, {"destroyJitter", 0.35f},
            {"hazeType", 1.0f}, {"hazeMix", 0.2f}, {"hazeColor", 0.6f}
        }),
        makePreset("Telephone Line", bank, {
            {"chainSlot0", 1.0f}, {"destroyEnabled", 1.0f},
            {"destroyFader", 3500.0f}, {"destroyBits", 8.0f},
            {"destroyIn", 6.0f}, {"destroyOut", -3.0f}, {"destroyJitter", 0.3f}
        }),
        makePreset("Transistor Radio", bank, {
            {"chainSlot0", 1.0f}, {"destroyEnabled", 1.0f},
            {"destroyFader", 6000.0f}, {"destroyBits", 10.0f},
            {"destroyIn", 7.0f}, {"destroyOut", -3.5f}, {"destroyJitter", 0.22f}
        }),

        // --- Bitcrushed / Digital Degradation ---
        makePreset("8-Bit Nostalgia", bank, {
            {"chainSlot0", 1.0f}, {"destroyEnabled", 1.0f},
            {"destroyFader", 6000.0f}, {"destroyBits", 8.0f},
            {"destroyIn", 4.0f}, {"destroyOut", -2.0f}
        }),
        makePreset("4-Bit Crunch", bank, {
            {"chainSlot0", 1.0f}, {"destroyEnabled", 1.0f},
            {"destroyFader", 6000.0f}, {"destroyBits", 4.0f},
            {"destroyIn", 6.0f}, {"destroyOut", -4.0f}
        }),
        makePreset("Digital Grit", bank, {
            {"chainSlot0", 1.0f}, {"destroyEnabled", 1.0f},
            {"destroyFader", 8000.0f}, {"destroyBits", 8.0f},
            {"destroyIn", 8.0f}, {"destroyOut", -4.0f}, {"destroyJitter", 0.2f}
        }),
        makePreset("Aliased", bank, {
            {"chainSlot0", 1.0f}, {"destroyEnabled", 1.0f},
            {"destroyFader", 2000.0f}, {"destroyBits", 12.0f},
            {"destroyIn", 3.0f}, {"destroyOut", -1.5f}
        }),
        makePreset("Sample Trash", bank, {
            {"chainSlot0", 1.0f}, {"destroyEnabled", 1.0f},
            {"destroyFader", 3000.0f}, {"destroyBits", 6.0f},
            {"destroyIn", 10.0f}, {"destroyOut", -5.0f}, {"destroyJitter", 0.4f}
        }),

        // --- Subtle / Mix-Ready ---
        makePreset("Subtle Color", bank, {
            {"chainSlot0", 1.0f}, {"destroyEnabled", 1.0f},
            {"destroyFader", 44100.0f}, {"destroyBits", 16.0f},
            {"destroyIn", 2.0f}, {"destroyOut", -1.0f}, {"destroyMix", 0.35f}
        }),
        makePreset("Gentle Reduction", bank, {
            {"chainSlot0", 1.0f}, {"destroyEnabled", 1.0f},
            {"destroyFader", 32000.0f}, {"destroyBits", 14.0f},
            {"destroyIn", 1.0f}, {"destroyOut", -0.5f}, {"destroyMix", 0.4f}
        }),
        makePreset("Soft Crumble", bank, {
            {"chainSlot0", 1.0f}, {"destroyEnabled", 1.0f},
            {"destroyFader", 26040.0f}, {"destroyBits", 12.0f},
            {"destroyIn", 3.0f}, {"destroyOut", -1.5f}, {"destroyMix", 0.5f}
        }),
        makePreset("Touch Of Grime", bank, {
            {"chainSlot0", 1.0f}, {"destroyEnabled", 1.0f},
            {"destroyFader", 22000.0f}, {"destroyBits", 12.0f},
            {"destroyIn", 4.0f}, {"destroyOut", -2.0f}, {"destroyMix", 0.3f},
            {"destroyJitter", 0.08f}
        }),
        makePreset("Mellow Sampler", bank, {
            {"chainSlot0", 1.0f}, {"destroyEnabled", 1.0f},
            {"destroyFader", 26040.0f}, {"destroyBits", 12.0f},
            {"destroyIn", 2.0f}, {"destroyOut", -1.0f}, {"destroyMix", 0.55f},
            {"destroyJitter", 0.1f}
        }),

        // --- Saturated / Driven ---
        makePreset("ADC Overdrive", bank, {
            {"chainSlot0", 1.0f}, {"destroyEnabled", 1.0f},
            {"destroyFader", 26040.0f}, {"destroyBits", 12.0f},
            {"destroyIn", 14.0f}, {"destroyOut", -7.0f}
        }),
        makePreset("Muffled Sampler", bank, {
            {"chainSlot0", 1.0f}, {"destroyEnabled", 1.0f},
            {"destroyFader", 12000.0f}, {"destroyBits", 12.0f},
            {"destroyIn", 3.0f}, {"destroyOut", -1.5f}
        }),
        makePreset("Saturated Bits", bank, {
            {"chainSlot0", 1.0f}, {"chainSlot1", 5.0f},
            {"destroyEnabled", 1.0f}, {"distortionEnabled", 1.0f},
            {"destroyFader", 19000.0f}, {"destroyBits", 10.0f},
            {"destroyIn", 8.0f}, {"destroyOut", -4.0f},
            {"distortionDrive", 0.4f}, {"distortionTone", 0.6f}, {"distortionMix", 0.7f}
        }),
        makePreset("Fried Circuit", bank, {
            {"chainSlot0", 1.0f}, {"chainSlot1", 5.0f},
            {"destroyEnabled", 1.0f}, {"distortionEnabled", 1.0f},
            {"destroyFader", 8000.0f}, {"destroyBits", 6.0f},
            {"destroyIn", 12.0f}, {"destroyOut", -6.0f},
            {"distortionDrive", 0.7f}, {"distortionTone", 0.4f}, {"distortionMix", 0.8f}
        }),
        makePreset("Warm Overdrive", bank, {
            {"chainSlot0", 1.0f}, {"chainSlot1", 5.0f},
            {"destroyEnabled", 1.0f}, {"distortionEnabled", 1.0f},
            {"destroyFader", 26040.0f}, {"destroyBits", 12.0f},
            {"destroyIn", 6.0f}, {"destroyOut", -3.0f},
            {"distortionDrive", 0.35f}, {"distortionTone", 0.45f},
            {"distortionMode", 1.0f}, {"distortionMix", 0.6f}
        }),

        // --- Lo-Fi + Haze Combos ---
        makePreset("Hiss And Crush", bank, {
            {"chainSlot0", 1.0f}, {"chainSlot1", 7.0f},
            {"destroyEnabled", 1.0f}, {"hazeEnabled", 1.0f},
            {"destroyFader", 19000.0f}, {"destroyBits", 10.0f},
            {"destroyIn", 6.0f}, {"destroyOut", -3.0f},
            {"hazeType", 0.0f}, {"hazeMix", 0.2f}, {"hazeColor", 0.5f}
        }),
        makePreset("Pink Noise Wash", bank, {
            {"chainSlot0", 1.0f}, {"chainSlot1", 7.0f},
            {"destroyEnabled", 1.0f}, {"hazeEnabled", 1.0f},
            {"destroyFader", 22000.0f}, {"destroyBits", 12.0f},
            {"destroyIn", 3.0f}, {"destroyOut", -1.5f},
            {"hazeType", 0.0f}, {"hazeMix", 0.22f}, {"hazeColor", 0.55f}
        }),

        // --- Lo-Fi + Tape Combos ---
        makePreset("Dusty Cassette", bank, {
            {"chainSlot0", 1.0f}, {"chainSlot1", 4.0f},
            {"destroyEnabled", 1.0f}, {"tapeEnabled", 1.0f},
            {"destroyFader", 19000.0f}, {"destroyBits", 12.0f},
            {"destroyIn", 4.0f}, {"destroyOut", -2.0f},
            {"tapeDrive", 0.25f}, {"tapeWow", 0.15f}, {"tapeNoise", 0.12f}
        }),
        makePreset("VHS Memories", bank, {
            {"chainSlot0", 1.0f}, {"chainSlot1", 4.0f},
            {"destroyEnabled", 1.0f}, {"tapeEnabled", 1.0f},
            {"destroyFader", 14000.0f}, {"destroyBits", 10.0f},
            {"destroyIn", 5.0f}, {"destroyOut", -2.5f},
            {"tapeDrive", 0.3f}, {"tapeWow", 0.2f}, {"tapeNoise", 0.15f},
            {"tapeGlue", 0.6f}
        }),
        makePreset("Broken Walkman", bank, {
            {"chainSlot0", 4.0f}, {"chainSlot1", 1.0f},
            {"tapeEnabled", 1.0f}, {"destroyEnabled", 1.0f},
            {"tapeDrive", 0.35f}, {"tapeWow", 0.35f}, {"tapeNoise", 0.2f},
            {"tapeGlue", 0.7f},
            {"destroyFader", 12000.0f}, {"destroyBits", 10.0f},
            {"destroyIn", 6.0f}, {"destroyOut", -3.0f}
        }),
        makePreset("Sampler Through Tape", bank, {
            {"chainSlot0", 1.0f}, {"chainSlot1", 4.0f},
            {"destroyEnabled", 1.0f}, {"tapeEnabled", 1.0f},
            {"destroyFader", 26040.0f}, {"destroyBits", 12.0f},
            {"destroyIn", 6.0f}, {"destroyOut", -3.0f},
            {"tapeDrive", 0.2f}, {"tapeWow", 0.08f}, {"tapeNoise", 0.06f}
        }),
        makePreset("Lo-Fi Beat Tape", bank, {
            {"chainSlot0", 1.0f}, {"chainSlot1", 4.0f}, {"chainSlot2", 7.0f},
            {"destroyEnabled", 1.0f}, {"tapeEnabled", 1.0f}, {"hazeEnabled", 1.0f},
            {"destroyFader", 19000.0f}, {"destroyBits", 12.0f},
            {"destroyIn", 5.0f}, {"destroyOut", -2.5f},
            {"tapeDrive", 0.22f}, {"tapeWow", 0.1f}, {"tapeNoise", 0.1f},
            {"hazeType", 2.0f}, {"hazeMix", 0.1f}
        }),

        // --- Character Presets ---
        makePreset("Cheap Converter", bank, {
            {"chainSlot0", 1.0f}, {"destroyEnabled", 1.0f},
            {"destroyFader", 22050.0f}, {"destroyBits", 16.0f},
            {"destroyIn", 8.0f}, {"destroyOut", -4.0f}, {"destroyJitter", 0.3f}
        }),
        makePreset("Codec Artifacts", bank, {
            {"chainSlot0", 1.0f}, {"destroyEnabled", 1.0f},
            {"destroyFader", 11025.0f}, {"destroyBits", 8.0f},
            {"destroyIn", 3.0f}, {"destroyOut", -1.5f}, {"destroyJitter", 0.15f}
        }),
        makePreset("Degraded Memory", bank, {
            {"chainSlot0", 1.0f}, {"destroyEnabled", 1.0f},
            {"destroyFader", 16000.0f}, {"destroyBits", 10.0f},
            {"destroyIn", 5.0f}, {"destroyOut", -2.5f}, {"destroyMix", 0.75f},
            {"destroyJitter", 0.2f}
        }),
        makePreset("Quantized Noise", bank, {
            {"chainSlot0", 1.0f}, {"destroyEnabled", 1.0f},
            {"destroyFader", 8000.0f}, {"destroyBits", 4.0f},
            {"destroyIn", 2.0f}, {"destroyOut", -1.0f}, {"destroyJitter", 0.5f}
        }),
        makePreset("Vintage DAC", bank, {
            {"chainSlot0", 1.0f}, {"destroyEnabled", 1.0f},
            {"destroyFader", 44100.0f}, {"destroyBits", 12.0f},
            {"destroyIn", 4.0f}, {"destroyOut", -2.0f}, {"destroyJitter", 0.12f}
        }),

        // --- Extreme Lo-Fi ---
        makePreset("2-Bit Wasteland", bank, {
            {"chainSlot0", 1.0f}, {"destroyEnabled", 1.0f},
            {"destroyFader", 4000.0f}, {"destroyBits", 3.0f},
            {"destroyIn", 8.0f}, {"destroyOut", -6.0f}, {"destroyJitter", 0.4f}
        }),
        makePreset("1-Bit Square", bank, {
            {"chainSlot0", 1.0f}, {"destroyEnabled", 1.0f},
            {"destroyFader", 8000.0f}, {"destroyBits", 3.0f},
            {"destroyIn", 4.0f}, {"destroyOut", -6.0f}
        }),
        makePreset("Destroyed Signal", bank, {
            {"chainSlot0", 1.0f}, {"chainSlot1", 5.0f},
            {"destroyEnabled", 1.0f}, {"distortionEnabled", 1.0f},
            {"destroyFader", 2000.0f}, {"destroyBits", 4.0f},
            {"destroyIn", 14.0f}, {"destroyOut", -7.0f},
            {"distortionDrive", 0.8f}, {"distortionTone", 0.3f},
            {"distortionMode", 2.0f}
        }),
        makePreset("Digital Wasteland", bank, {
            {"chainSlot0", 1.0f}, {"chainSlot1", 7.0f},
            {"destroyEnabled", 1.0f}, {"hazeEnabled", 1.0f},
            {"destroyFader", 3000.0f}, {"destroyBits", 3.0f},
            {"destroyIn", 10.0f}, {"destroyOut", -5.0f}, {"destroyJitter", 0.45f},
            {"hazeType", 1.0f}, {"hazeMix", 0.25f}
        }),
        makePreset("Mangled Bits", bank, {
            {"chainSlot0", 1.0f}, {"destroyEnabled", 1.0f},
            {"destroyFader", 5000.0f}, {"destroyBits", 3.0f},
            {"destroyIn", 12.0f}, {"destroyOut", -6.0f}, {"destroyJitter", 0.5f}
        }),

        // --- Special Character ---
        makePreset("Intercom", bank, {
            {"chainSlot0", 1.0f}, {"destroyEnabled", 1.0f},
            {"destroyFader", 4500.0f}, {"destroyBits", 8.0f},
            {"destroyIn", 10.0f}, {"destroyOut", -5.0f}, {"destroyJitter", 0.18f}
        }),
        makePreset("Underwater Radio", bank, {
            {"chainSlot0", 1.0f}, {"chainSlot1", 7.0f},
            {"destroyEnabled", 1.0f}, {"hazeEnabled", 1.0f},
            {"destroyFader", 3000.0f}, {"destroyBits", 10.0f},
            {"destroyIn", 4.0f}, {"destroyOut", -2.0f},
            {"hazeType", 0.0f}, {"hazeMix", 0.3f}, {"hazeColor", 0.2f}
        }),
        makePreset("Distant Broadcast", bank, {
            {"chainSlot0", 1.0f}, {"chainSlot1", 6.0f},
            {"destroyEnabled", 1.0f}, {"reverbEnabled", 1.0f},
            {"destroyFader", 5000.0f}, {"destroyBits", 8.0f},
            {"destroyIn", 6.0f}, {"destroyOut", -3.0f},
            {"reverbMix", 0.3f}, {"reverbSize", 0.6f}, {"reverbDecay", 0.4f}
        }),
    };
}
