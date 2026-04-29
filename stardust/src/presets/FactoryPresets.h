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
        // Character macro
        {"characterAmount", 0.0f}, {"characterMode", 0.0f},

        // GRIT
        {"destroyFader", 22082.0f}, {"destroyBits", 12.0f}, {"destroyIn", 0.0f},
        {"destroyMix", 1.0f}, {"destroyJitter", 0.10f},

        // EXCITER
        {"exciterDrive", 0.35f}, {"exciterTone", 6000.0f}, {"exciterMix", 0.25f},
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

// Returns all factory presets (Init + curated banks concatenated)
std::vector<Preset> getAllFactoryPresets();
