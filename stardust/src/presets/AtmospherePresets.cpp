#include "FactoryPresets.h"

// Chain slot IDs: 0=empty, 1=CRUSH, 3=CHORUS, 4=TAPE, 5=SATURATE,
//                 6=REVERB, 7=HAZE, 8=MULTIPLY, 10=STUTTER,
//                 11=SHIFT, 12=REVERSER

std::vector<Preset> getAtmospherePresets()
{
    const juce::String bank = "Atmosphere";
    return {
        // --- Shimmer / Reverb Pads ---
        makePreset("Shimmer Pad", bank, {
            {"chainSlot0", 11.0f}, {"chainSlot1", 6.0f},
            {"shiftEnabled", 1.0f}, {"reverbEnabled", 1.0f},
            {"shiftPitch", 12.0f}, {"shiftFeedback", 0.5f},
            {"shiftTone", 10000.0f}, {"shiftMix", 0.35f},
            {"reverbMix", 0.6f}, {"reverbDecay", 0.8f}, {"reverbSize", 0.85f},
            {"reverbDiffusion", 0.9f}, {"reverbWidth", 1.0f}
        }),
        makePreset("Ethereal Shimmer", bank, {
            {"chainSlot0", 11.0f}, {"chainSlot1", 6.0f}, {"chainSlot2", 3.0f},
            {"shiftEnabled", 1.0f}, {"reverbEnabled", 1.0f},
            {"shiftPitch", 12.0f}, {"shiftFeedback", 0.6f},
            {"shiftTone", 12000.0f}, {"shiftMix", 0.3f},
            {"reverbMix", 0.7f}, {"reverbDecay", 0.9f}, {"reverbSize", 0.9f},
            {"reverbDiffusion", 0.95f},
            {"chorusMix", 0.3f}, {"junoMode", 2.0f}
        }),
        makePreset("Celestial Plate", bank, {
            {"chainSlot0", 6.0f}, {"reverbEnabled", 1.0f},
            {"reverbMix", 0.7f}, {"reverbDecay", 0.85f}, {"reverbSize", 0.9f},
            {"reverbDamp", 0.5f}, {"reverbPreDelay", 25.0f},
            {"reverbDiffusion", 0.9f}, {"reverbWidth", 1.0f}
        }),
        makePreset("Cathedral", bank, {
            {"chainSlot0", 6.0f}, {"reverbEnabled", 1.0f},
            {"reverbMix", 0.6f}, {"reverbDecay", 0.9f}, {"reverbSize", 0.95f},
            {"reverbDamp", 0.6f}, {"reverbPreDelay", 30.0f},
            {"reverbDiffusion", 0.85f}, {"reverbWidth", 1.0f}
        }),
        makePreset("Infinite Plate", bank, {
            {"chainSlot0", 6.0f}, {"reverbEnabled", 1.0f},
            {"reverbMix", 0.75f}, {"reverbDecay", 0.98f}, {"reverbSize", 0.99f},
            {"reverbDamp", 0.4f}, {"reverbPreDelay", 15.0f},
            {"reverbDiffusion", 0.95f}, {"reverbWidth", 1.0f}
        }),

        // --- Space Pads ---
        makePreset("Space Choir", bank, {
            {"chainSlot0", 8.0f}, {"chainSlot1", 6.0f}, {"chainSlot2", 11.0f},
            {"unisonEnabled", 1.0f}, {"reverbEnabled", 1.0f}, {"shiftEnabled", 1.0f},
            {"unisonMix", 0.5f}, {"unisonSpeed", 0.7f},
            {"reverbMix", 0.6f}, {"reverbDecay", 0.8f}, {"reverbSize", 0.85f},
            {"shiftPitch", 12.0f}, {"shiftFeedback", 0.3f}, {"shiftMix", 0.2f}
        }),
        makePreset("Deep Space", bank, {
            {"chainSlot0", 6.0f}, {"chainSlot1", 11.0f},
            {"reverbEnabled", 1.0f}, {"shiftEnabled", 1.0f},
            {"reverbMix", 0.7f}, {"reverbDecay", 0.92f}, {"reverbSize", 0.95f},
            {"reverbDamp", 0.3f}, {"reverbWidth", 1.0f},
            {"shiftPitch", -12.0f}, {"shiftFeedback", 0.4f}, {"shiftMix", 0.25f}
        }),
        makePreset("Orbit", bank, {
            {"chainSlot0", 8.0f}, {"chainSlot1", 6.0f},
            {"unisonEnabled", 1.0f}, {"reverbEnabled", 1.0f},
            {"unisonMix", 0.6f}, {"unisonSpeed", 0.6f},
            {"unisonOuter", 1.0f}, {"unisonInner", 0.5f},
            {"reverbMix", 0.55f}, {"reverbDecay", 0.75f}, {"reverbSize", 0.8f},
            {"msWidth", 1.4f}
        }),
        makePreset("Floating", bank, {
            {"chainSlot0", 3.0f}, {"chainSlot1", 6.0f},
            {"reverbEnabled", 1.0f},
            {"chorusMix", 0.5f}, {"junoMode", 2.0f},
            {"reverbMix", 0.55f}, {"reverbDecay", 0.7f}, {"reverbSize", 0.75f},
            {"reverbDiffusion", 0.9f}
        }),
        makePreset("Zero Gravity", bank, {
            {"chainSlot0", 6.0f}, {"chainSlot1", 8.0f}, {"chainSlot2", 11.0f},
            {"reverbEnabled", 1.0f}, {"unisonEnabled", 1.0f}, {"shiftEnabled", 1.0f},
            {"reverbMix", 0.65f}, {"reverbDecay", 0.88f}, {"reverbSize", 0.9f},
            {"unisonMix", 0.4f}, {"unisonSpeed", 0.5f},
            {"shiftPitch", 7.0f}, {"shiftFeedback", 0.3f}, {"shiftMix", 0.2f}
        }),

        // --- Chorus / Ensemble Layers ---
        makePreset("Classic Chorus", bank, {
            {"chainSlot0", 3.0f},
            {"chorusMix", 0.5f}, {"junoMode", 0.0f}
        }),
        makePreset("Juno Ensemble", bank, {
            {"chainSlot0", 3.0f},
            {"chorusMix", 0.6f}, {"junoMode", 2.0f}
        }),
        makePreset("Slow Ensemble", bank, {
            {"chainSlot0", 3.0f}, {"chainSlot1", 6.0f},
            {"reverbEnabled", 1.0f},
            {"chorusMix", 0.5f}, {"junoMode", 1.0f},
            {"reverbMix", 0.35f}, {"reverbDecay", 0.5f}
        }),
        makePreset("Wide Stereo Wash", bank, {
            {"chainSlot0", 3.0f}, {"chainSlot1", 8.0f},
            {"unisonEnabled", 1.0f},
            {"chorusMix", 0.5f}, {"junoMode", 2.0f},
            {"unisonMix", 0.4f}, {"unisonOuter", 1.0f}, {"unisonInner", 0.6f},
            {"msWidth", 1.5f}
        }),
        makePreset("Vibrato Voice", bank, {
            {"chainSlot0", 3.0f},
            {"chorusMix", 0.7f}, {"junoMode", 0.0f}
        }),

        // --- Multiply / Unison Thickener ---
        makePreset("Tight Doubles", bank, {
            {"chainSlot0", 8.0f}, {"unisonEnabled", 1.0f},
            {"unisonMix", 0.5f}, {"unisonSpeed", 1.2f},
            {"unisonOuter", 0.8f}, {"unisonInner", 0.6f}
        }),
        makePreset("Wide Unison", bank, {
            {"chainSlot0", 8.0f}, {"unisonEnabled", 1.0f},
            {"unisonMix", 0.6f}, {"unisonSpeed", 0.8f},
            {"unisonOuter", 1.0f}, {"unisonInner", 1.0f},
            {"msWidth", 1.3f}
        }),
        makePreset("Detuned Spread", bank, {
            {"chainSlot0", 8.0f}, {"unisonEnabled", 1.0f},
            {"unisonMix", 0.55f}, {"unisonSpeed", 1.5f},
            {"unisonOuter", 1.0f}, {"unisonInner", 0.8f}
        }),
        makePreset("Mono Thicken", bank, {
            {"chainSlot0", 8.0f}, {"unisonEnabled", 1.0f},
            {"unisonMix", 0.45f}, {"unisonSpeed", 1.0f},
            {"unisonOuter", 0.5f}, {"unisonInner", 0.5f},
            {"msWidth", 0.8f}
        }),
        makePreset("Super Unison", bank, {
            {"chainSlot0", 8.0f}, {"chainSlot1", 3.0f},
            {"unisonEnabled", 1.0f},
            {"unisonMix", 0.6f}, {"unisonSpeed", 1.0f},
            {"unisonOuter", 1.0f}, {"unisonInner", 1.0f},
            {"chorusMix", 0.4f}, {"junoMode", 2.0f}
        }),

        // --- Cinematic ---
        makePreset("Cinematic Wash", bank, {
            {"chainSlot0", 4.0f}, {"chainSlot1", 6.0f}, {"chainSlot2", 8.0f},
            {"tapeEnabled", 1.0f}, {"reverbEnabled", 1.0f}, {"unisonEnabled", 1.0f},
            {"tapeDrive", 0.15f}, {"tapeGlue", 0.5f}, {"tapeWow", 0.03f}, {"tapeMix", 0.7f},
            {"reverbMix", 0.6f}, {"reverbDecay", 0.8f}, {"reverbSize", 0.85f},
            {"unisonMix", 0.35f}, {"unisonSpeed", 0.7f}
        }),
        makePreset("Film Score Pad", bank, {
            {"chainSlot0", 11.0f}, {"chainSlot1", 6.0f}, {"chainSlot2", 4.0f},
            {"shiftEnabled", 1.0f}, {"reverbEnabled", 1.0f}, {"tapeEnabled", 1.0f},
            {"shiftPitch", 12.0f}, {"shiftFeedback", 0.4f}, {"shiftMix", 0.25f},
            {"reverbMix", 0.65f}, {"reverbDecay", 0.85f}, {"reverbSize", 0.9f},
            {"tapeDrive", 0.12f}, {"tapeGlue", 0.45f}, {"tapeMix", 0.5f}
        }),
        makePreset("Suspense Bed", bank, {
            {"chainSlot0", 6.0f}, {"chainSlot1", 0.0f}, {"chainSlot2", 7.0f},
            {"reverbEnabled", 1.0f}, {"hazeEnabled", 1.0f},
            {"reverbMix", 0.5f}, {"reverbDecay", 0.75f}, {"reverbSize", 0.8f},
            {"hazeType", 0.0f}, {"hazeMix", 0.08f}
        }),
        makePreset("Drone Bed", bank, {
            {"chainSlot0", 0.0f}, {"chainSlot1", 6.0f}, {"chainSlot2", 11.0f}, {"reverbEnabled", 1.0f}, {"shiftEnabled", 1.0f},
            {"reverbMix", 0.6f}, {"reverbDecay", 0.9f}, {"reverbSize", 0.9f},
            {"shiftPitch", -12.0f}, {"shiftFeedback", 0.5f}, {"shiftMix", 0.3f}
        }),
        makePreset("Tension Riser", bank, {
            {"chainSlot0", 11.0f}, {"chainSlot1", 6.0f},
            {"shiftEnabled", 1.0f}, {"reverbEnabled", 1.0f},
            {"shiftPitch", 5.0f}, {"shiftFeedback", 0.7f},
            {"shiftTone", 8000.0f}, {"shiftMix", 0.5f},
            {"reverbMix", 0.5f}, {"reverbDecay", 0.7f}, {"reverbSize", 0.75f}
        }),

        // --- Lush Multi-Effect ---
        makePreset("Lush Layers", bank, {
            {"chainSlot0", 3.0f}, {"chainSlot1", 8.0f}, {"chainSlot2", 6.0f},
            {"unisonEnabled", 1.0f}, {"reverbEnabled", 1.0f},
            {"chorusMix", 0.5f}, {"junoMode", 2.0f},
            {"unisonMix", 0.45f}, {"unisonSpeed", 0.8f},
            {"reverbMix", 0.45f}, {"reverbDecay", 0.6f}, {"reverbSize", 0.7f}
        }),
        makePreset("Dream State", bank, {
            {"chainSlot0", 4.0f}, {"chainSlot1", 3.0f}, {"chainSlot2", 6.0f},
            {"tapeEnabled", 1.0f}, {"reverbEnabled", 1.0f},
            {"tapeDrive", 0.15f}, {"tapeWow", 0.08f}, {"tapeMix", 0.7f},
            {"chorusMix", 0.5f}, {"junoMode", 1.0f},
            {"reverbMix", 0.5f}, {"reverbDecay", 0.7f}
        }),
        makePreset("Vintage Dream", bank, {
            {"chainSlot0", 1.0f}, {"chainSlot1", 4.0f}, {"chainSlot2", 6.0f},
            {"destroyEnabled", 1.0f}, {"tapeEnabled", 1.0f}, {"reverbEnabled", 1.0f},
            {"destroyFader", 26040.0f}, {"destroyBits", 14.0f},
            {"destroyIn", 2.0f}, {"destroyOut", -1.0f}, {"destroyMix", 0.5f},
            {"tapeDrive", 0.15f}, {"tapeWow", 0.06f}, {"tapeMix", 0.7f},
            {"reverbMix", 0.45f}, {"reverbDecay", 0.6f}
        }),
        makePreset("Cosmic Swirl", bank, {
            {"chainSlot0", 3.0f}, {"chainSlot1", 11.0f}, {"chainSlot2", 6.0f},
            {"shiftEnabled", 1.0f}, {"reverbEnabled", 1.0f},
            {"chorusMix", 0.5f}, {"junoMode", 2.0f},
            {"shiftPitch", 7.0f}, {"shiftFeedback", 0.4f}, {"shiftMix", 0.25f},
            {"reverbMix", 0.55f}, {"reverbDecay", 0.75f}
        }),
        makePreset("Textured Shimmer", bank, {
            {"chainSlot0", 0.0f}, {"chainSlot1", 11.0f}, {"chainSlot2", 6.0f}, {"shiftEnabled", 1.0f}, {"reverbEnabled", 1.0f},
            {"shiftPitch", 12.0f}, {"shiftFeedback", 0.4f}, {"shiftMix", 0.25f},
            {"reverbMix", 0.55f}, {"reverbDecay", 0.8f}
        }),

        // --- Modulated Atmosphere ---
        makePreset("LFO Wash", bank, {
            {"chainSlot0", 4.0f}, {"chainSlot1", 6.0f},
            {"tapeEnabled", 1.0f}, {"reverbEnabled", 1.0f},
            {"tapeDrive", 0.2f}, {"tapeGlue", 0.5f}, {"tapeMix", 0.7f},
            {"reverbMix", 0.5f}, {"reverbDecay", 0.7f},
            {"modLfo1Rate", 0.5f}, {"modLfo1Depth", 0.6f}, {"modLfo1Wave", 0.0f},
            {"modSlot1Src", 1.0f}, {"modSlot1Tgt", 1.0f}, {"modSlot1Amt", 0.3f}
        }),
        makePreset("Breathing Pad", bank, {
            {"chainSlot0", 6.0f}, {"chainSlot1", 8.0f},
            {"reverbEnabled", 1.0f}, {"unisonEnabled", 1.0f},
            {"reverbMix", 0.55f}, {"reverbDecay", 0.75f}, {"reverbSize", 0.8f},
            {"unisonMix", 0.4f}, {"unisonSpeed", 0.7f},
            {"modLfo1Rate", 0.2f}, {"modLfo1Depth", 0.5f}, {"modLfo1Wave", 0.0f},
            {"modSlot1Src", 1.0f}, {"modSlot1Tgt", 3.0f}, {"modSlot1Amt", 0.4f}
        }),
        makePreset("Pulsing Reverb", bank, {
            {"chainSlot0", 6.0f}, {"reverbEnabled", 1.0f},
            {"reverbMix", 0.6f}, {"reverbDecay", 0.8f}, {"reverbSize", 0.85f},
            {"modLfo1Rate", 2.0f}, {"modLfo1Depth", 0.4f}, {"modLfo1Wave", 1.0f},
            {"modSlot1Src", 1.0f}, {"modSlot1Tgt", 3.0f}, {"modSlot1Amt", 0.5f}
        }),
        makePreset("Evolving Texture", bank, {
            {"chainSlot0", 0.0f}, {"chainSlot1", 6.0f}, {"reverbEnabled", 1.0f},
            {"reverbMix", 0.5f}, {"reverbDecay", 0.7f},
            {"modLfo1Rate", 0.1f}, {"modLfo1Depth", 0.5f}, {"modLfo1Wave", 0.0f},
            {"modLfo2Rate", 0.3f}, {"modLfo2Depth", 0.3f}, {"modLfo2Wave", 1.0f},
            {"modSlot1Src", 1.0f}, {"modSlot1Tgt", 1.0f}, {"modSlot1Amt", 0.25f},
            {"modSlot2Src", 2.0f}, {"modSlot2Tgt", 2.0f}, {"modSlot2Amt", 0.2f}
        }),
        makePreset("Slow Motion", bank, {
            {"chainSlot0", 4.0f}, {"chainSlot1", 3.0f}, {"chainSlot2", 6.0f},
            {"tapeEnabled", 1.0f}, {"reverbEnabled", 1.0f},
            {"tapeDrive", 0.2f}, {"tapeWow", 0.15f}, {"tapeMix", 0.8f},
            {"chorusMix", 0.4f}, {"junoMode", 1.0f},
            {"reverbMix", 0.55f}, {"reverbDecay", 0.75f}
        }),

        // --- Full Stack (All Slots) ---
        makePreset("Full Stack", bank, {
            {"chainSlot0", 1.0f}, {"chainSlot1", 4.0f}, {"chainSlot2", 3.0f}, {"chainSlot3", 6.0f},
            {"destroyEnabled", 1.0f}, {"tapeEnabled", 1.0f}, {"reverbEnabled", 1.0f},
            {"destroyFader", 26040.0f}, {"destroyBits", 14.0f},
            {"destroyIn", 3.0f}, {"destroyOut", -1.5f}, {"destroyMix", 0.5f},
            {"tapeDrive", 0.2f}, {"tapeWow", 0.05f}, {"tapeMix", 0.7f},
            {"chorusMix", 0.4f}, {"junoMode", 2.0f},
            {"reverbMix", 0.4f}, {"reverbDecay", 0.6f}
        }),
        makePreset("Everything Shimmer", bank, {
            {"chainSlot0", 0.0f}, {"chainSlot1", 11.0f}, {"chainSlot2", 8.0f}, {"chainSlot3", 6.0f}, {"shiftEnabled", 1.0f},
            {"unisonEnabled", 1.0f}, {"reverbEnabled", 1.0f},
            {"shiftPitch", 12.0f}, {"shiftFeedback", 0.4f}, {"shiftMix", 0.25f},
            {"unisonMix", 0.35f}, {"unisonSpeed", 0.8f},
            {"reverbMix", 0.55f}, {"reverbDecay", 0.8f}
        }),
        makePreset("Total Immersion", bank, {
            {"chainSlot0", 4.0f}, {"chainSlot1", 0.0f}, {"chainSlot2", 11.0f}, {"chainSlot3", 6.0f},
            {"tapeEnabled", 1.0f},
            {"shiftEnabled", 1.0f}, {"reverbEnabled", 1.0f},
            {"tapeDrive", 0.15f}, {"tapeWow", 0.04f}, {"tapeMix", 0.6f},
            {"shiftPitch", 7.0f}, {"shiftFeedback", 0.3f}, {"shiftMix", 0.2f},
            {"reverbMix", 0.55f}, {"reverbDecay", 0.8f}, {"reverbSize", 0.85f}
        }),

        // --- Subtle Atmosphere ---
        makePreset("Room Tone", bank, {
            {"chainSlot0", 6.0f}, {"reverbEnabled", 1.0f},
            {"reverbMix", 0.25f}, {"reverbDecay", 0.3f}, {"reverbSize", 0.4f},
            {"reverbPreDelay", 5.0f}
        }),
        makePreset("Small Hall", bank, {
            {"chainSlot0", 6.0f}, {"reverbEnabled", 1.0f},
            {"reverbMix", 0.35f}, {"reverbDecay", 0.45f}, {"reverbSize", 0.55f},
            {"reverbPreDelay", 10.0f}, {"reverbDiffusion", 0.8f}
        }),
        makePreset("Gentle Ambience", bank, {
            {"chainSlot0", 6.0f}, {"chainSlot1", 3.0f},
            {"reverbEnabled", 1.0f},
            {"reverbMix", 0.3f}, {"reverbDecay", 0.4f}, {"reverbSize", 0.5f},
            {"chorusMix", 0.25f}, {"junoMode", 0.0f}
        }),

        // --- Additional Atmosphere ---
        makePreset("Warm Plate", bank, {
            {"chainSlot0", 6.0f}, {"reverbEnabled", 1.0f},
            {"reverbMix", 0.5f}, {"reverbDecay", 0.6f}, {"reverbSize", 0.65f},
            {"reverbDamp", 0.5f}, {"reverbPreDelay", 15.0f}, {"reverbDiffusion", 0.85f}
        }),
        makePreset("Dark Reverb", bank, {
            {"chainSlot0", 6.0f}, {"reverbEnabled", 1.0f},
            {"reverbMix", 0.55f}, {"reverbDecay", 0.7f}, {"reverbSize", 0.75f},
            {"reverbDamp", 0.9f}, {"reverbDiffusion", 0.8f}
        }),
        makePreset("Bright Plate", bank, {
            {"chainSlot0", 6.0f}, {"reverbEnabled", 1.0f},
            {"reverbMix", 0.5f}, {"reverbDecay", 0.55f}, {"reverbSize", 0.6f},
            {"reverbDamp", 0.3f}, {"reverbDiffusion", 0.9f}
        }),
        makePreset("Ambient Swell", bank, {
            {"chainSlot0", 11.0f}, {"chainSlot1", 6.0f}, {"chainSlot2", 8.0f},
            {"shiftEnabled", 1.0f}, {"reverbEnabled", 1.0f}, {"unisonEnabled", 1.0f},
            {"shiftPitch", 7.0f}, {"shiftFeedback", 0.5f}, {"shiftMix", 0.3f},
            {"reverbMix", 0.6f}, {"reverbDecay", 0.8f}, {"reverbSize", 0.85f},
            {"unisonMix", 0.3f}, {"unisonSpeed", 0.7f}
        }),
        makePreset("Chorus Reverb", bank, {
            {"chainSlot0", 3.0f}, {"chainSlot1", 6.0f},
            {"reverbEnabled", 1.0f},
            {"chorusMix", 0.5f}, {"junoMode", 2.0f},
            {"reverbMix", 0.5f}, {"reverbDecay", 0.65f}, {"reverbSize", 0.7f}
        }),
        makePreset("Suboctave Pad", bank, {
            {"chainSlot0", 11.0f}, {"chainSlot1", 6.0f},
            {"shiftEnabled", 1.0f}, {"reverbEnabled", 1.0f},
            {"shiftPitch", -12.0f}, {"shiftFeedback", 0.4f}, {"shiftMix", 0.35f},
            {"reverbMix", 0.55f}, {"reverbDecay", 0.75f}, {"reverbSize", 0.8f}
        }),
        makePreset("Texture Reverb", bank, {
            {"chainSlot0", 0.0f}, {"chainSlot1", 6.0f}, {"reverbEnabled", 1.0f},
            {"reverbMix", 0.5f}, {"reverbDecay", 0.65f}
        }),
        makePreset("Tape Atmosphere", bank, {
            {"chainSlot0", 4.0f}, {"chainSlot1", 6.0f}, {"chainSlot2", 7.0f},
            {"tapeEnabled", 1.0f}, {"reverbEnabled", 1.0f}, {"hazeEnabled", 1.0f},
            {"tapeDrive", 0.15f}, {"tapeWow", 0.06f}, {"tapeMix", 0.7f},
            {"reverbMix", 0.45f}, {"reverbDecay", 0.6f},
            {"hazeType", 2.0f}, {"hazeMix", 0.08f}
        }),
        makePreset("Infinite Shimmer", bank, {
            {"chainSlot0", 11.0f}, {"chainSlot1", 6.0f},
            {"shiftEnabled", 1.0f}, {"reverbEnabled", 1.0f},
            {"shiftPitch", 12.0f}, {"shiftFeedback", 0.7f},
            {"shiftTone", 8000.0f}, {"shiftMix", 0.4f},
            {"reverbMix", 0.7f}, {"reverbDecay", 0.95f}, {"reverbSize", 0.95f},
            {"reverbDiffusion", 0.95f}
        }),
    };
}
