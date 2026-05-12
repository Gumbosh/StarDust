#pragma once

#include <array>
#include <cmath>
#include <juce_audio_processors/juce_audio_processors.h>

// Curated flavor targets blended with the user's GRIT/EXCITER settings when Character > 0.
// Skewed parameters (Rate, Freq) interpolate in normalised knob space so the macro follows
// the same perceptual mapping as the UI controls.

namespace CharacterMacro
{

struct FlavorTarget
{
    float gritRateHz;
    float gritBits;
    float gritDriveDb;
    float gritJitter;
    float gritMix;
    float exciterDrive;
    float exciterToneHz;
    float exciterMix;
};

constexpr std::array<FlavorTarget, 6> kFlavors {{
    { 26040.0f, 14.0f, 3.0f, 0.12f, 0.45f, 0.24f, 5200.0f, 0.12f },
    { 44100.0f, 18.0f, 1.0f, 0.02f, 0.16f, 0.28f, 9600.0f, 0.36f },
    { 16000.0f, 10.0f, 7.0f, 0.26f, 0.68f, 0.36f, 3600.0f, 0.14f },
    { 44100.0f, 16.0f, 8.0f, 0.04f, 0.52f, 0.34f, 6500.0f, 0.20f },
    { 7000.0f, 6.0f, 12.0f, 0.42f, 0.86f, 0.64f, 4300.0f, 0.30f },
    { 44100.0f, 18.0f, 2.0f, 0.02f, 0.22f, 0.30f, 8200.0f, 0.46f },
}};

inline const juce::NormalisableRange<float>& destroyRateRange()
{
    static const juce::NormalisableRange<float> range = [] {
        juce::NormalisableRange<float> r(250.0f, 96000.0f, 1.0f);
        r.setSkewForCentre(3000.0f);
        return r;
    }();
    return range;
}

inline const juce::NormalisableRange<float>& exciterToneRange()
{
    static const juce::NormalisableRange<float> range = [] {
        juce::NormalisableRange<float> r(800.0f, 16000.0f, 1.0f);
        r.setSkewForCentre(6000.0f);
        return r;
    }();
    return range;
}

inline float lerp(float a, float b, float t)
{
    return a + (b - a) * t;
}

inline float blendInRange(const juce::NormalisableRange<float>& range,
                          float userValue,
                          float flavorValue,
                          float characterAmount)
{
    const float u = range.snapToLegalValue(juce::jlimit(range.start, range.end, userValue));
    const float fv = range.snapToLegalValue(juce::jlimit(range.start, range.end, flavorValue));
    const float n0 = range.convertTo0to1(u);
    const float n1 = range.convertTo0to1(fv);
    return range.snapToLegalValue(range.convertFrom0to1(lerp(n0, n1, characterAmount)));
}

struct EffectiveMacroState
{
    float destroyMix = 0.0f;
    float destroyIn = 0.0f;
    float destroyFader = 22082.0f;
    float destroyBits = 12.0f;
    float destroyJitter = 0.0f;
    float exciterMix = 0.0f;
    float exciterDrive = 0.0f;
    float exciterTone = 6000.0f;
};

inline EffectiveMacroState computeEffective(const FlavorTarget& flavor,
                                            float destroyMix,
                                            float destroyIn,
                                            float destroyFader,
                                            float destroyBits,
                                            float destroyJitter,
                                            float exciterMix,
                                            float exciterDrive,
                                            float exciterTone,
                                            float characterAmount)
{
    const float a = juce::jlimit(0.0f, 1.0f, characterAmount);
    EffectiveMacroState out;
    out.destroyMix = lerp(destroyMix, flavor.gritMix, a);
    out.destroyIn = lerp(destroyIn, flavor.gritDriveDb, a);
    out.destroyFader = blendInRange(destroyRateRange(), destroyFader, flavor.gritRateHz, a);
    out.destroyBits = lerp(destroyBits, flavor.gritBits, a);
    out.destroyJitter = lerp(destroyJitter, flavor.gritJitter, a);
    out.exciterMix = lerp(exciterMix, flavor.exciterMix, a);
    out.exciterDrive = lerp(exciterDrive, flavor.exciterDrive, a);
    out.exciterTone = blendInRange(exciterToneRange(), exciterTone, flavor.exciterToneHz, a);
    return out;
}

} // namespace CharacterMacro
