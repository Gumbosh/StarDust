#pragma once
#include "../PluginProcessor.h"
#include <map>
#include <vector>
#include <initializer_list>
#include <utility>

// Default parameter values matching the Init preset (createParameterLayout defaults).
// Each bank preset only overrides values that differ from these.
inline std::map<juce::String, float> defaultPresetValues()
{
    return {
        // Chain slots — all empty
        {"chainSlot0", 0.0f}, {"chainSlot1", 0.0f}, {"chainSlot2", 0.0f}, {"chainSlot3", 0.0f},

        // Enable flags — all off
        {"destroyEnabled", 0.0f}, {"tapeEnabled", 0.0f}, {"multiplyEnabled", 0.0f},
        {"distortionEnabled", 0.0f}, {"reverbEnabled", 0.0f}, {"hazeEnabled", 0.0f},
        {"unisonEnabled", 0.0f}, {"stutterEnabled", 0.0f},
        {"shiftEnabled", 0.0f}, {"reverserEnabled", 0.0f},

        // Destroy
        {"destroyFader", 22082.0f}, {"destroyBits", 12.0f}, {"destroyIn", 0.0f},
        {"destroyOut", 0.0f}, {"destroyMix", 1.0f}, {"destroyJitter", 0.10f},

        // Tape
        {"tapeDrive", 0.17f}, {"tapeInput", 0.0f}, {"tapeGlue", 0.48f},
        {"tapeNoise", 0.07f}, {"tapeNoiseSpeed", 1.0f}, {"tapeMix", 1.0f},
        {"tapeOutput", 0.0f}, {"tapeWow", 0.07f}, {"tapePrintThrough", 0.3f},
        {"tapeEQStandard", 0.0f}, {"tapeMotorEnabled", 1.0f},

        // Chorus
        {"chorusMix", 1.0f}, {"junoMode", 0.0f},

        // Distortion
        {"distortionDrive", 0.5f}, {"distortionTone", 0.5f},
        {"distortionMode", 0.0f}, {"distortionMix", 1.0f},

        // Reverb
        {"reverbMix", 0.5f}, {"reverbSize", 0.5f}, {"reverbDecay", 0.5f},
        {"reverbDamp", 0.7f}, {"reverbPreDelay", 10.0f}, {"reverbDiffusion", 0.7f},
        {"reverbWidth", 1.0f},

        // Haze
        {"hazeColor", 0.5f}, {"hazeType", 0.0f}, {"hazeMix", 0.15f},

        // Multiply (unison)
        {"unisonMix", 0.5f}, {"unisonSpeed", 1.0f},
        {"unisonOuter", 1.0f}, {"unisonInner", 0.8f},

        // Stutter
        {"stutterRate", 16.0f}, {"stutterAttack", 35.0f},
        {"stutterDecay", 124.0f}, {"stutterDepth", 100.0f},
        {"stutterRelease", 65.0f}, {"stutterSwing", 10.0f},
        {"stutterResolution", 2.0f},
        {"stutterMix", 1.0f},

        // Shift
        {"shiftPitch", 0.0f}, {"shiftFeedback", 0.0f},
        {"shiftTone", 10000.0f}, {"shiftMix", 0.5f},

        // Reverser
        {"reverserRepeat", 4.0f}, {"reverserDivision", 3.0f},
        {"reverserCrossfade", 0.15f}, {"reverserMix", 0.5f},

        // Master
        {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}, {"msWidth", 1.0f},

        // Modulation — all off
        {"modLfo1Rate", 1.0f}, {"modLfo1Depth", 0.0f}, {"modLfo1Wave", 0.0f}, {"modLfo1Sync", 0.0f},
        {"modLfo2Rate", 1.0f}, {"modLfo2Depth", 0.0f}, {"modLfo2Wave", 0.0f}, {"modLfo2Sync", 0.0f},
        {"modSlot1Src", 0.0f}, {"modSlot1Tgt", 0.0f}, {"modSlot1Amt", 0.0f},
        {"modSlot2Src", 0.0f}, {"modSlot2Tgt", 0.0f}, {"modSlot2Amt", 0.0f},
        {"modSlot3Src", 0.0f}, {"modSlot3Tgt", 0.0f}, {"modSlot3Amt", 0.0f},
        {"modSlot4Src", 0.0f}, {"modSlot4Tgt", 0.0f}, {"modSlot4Amt", 0.0f},
    };
}

// Helper: create a Preset from bank name + parameter overrides
inline Preset makePreset(const juce::String& name, const juce::String& bank,
                         std::initializer_list<std::pair<const juce::String, float>> overrides)
{
    auto values = defaultPresetValues();
    for (const auto& [k, v] : overrides)
        values[k] = v;
    return { name, std::move(values), true, bank };
}

// Per-bank preset generators
std::vector<Preset> getLoFiPresets();
std::vector<Preset> getGlitchPresets();
std::vector<Preset> getTapePresets();
std::vector<Preset> getAtmospherePresets();
std::vector<Preset> getAbstractPresets();

// Returns all factory presets (Init + curated banks concatenated)
std::vector<Preset> getAllFactoryPresets();
