#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "presets/FactoryPresets.h"
#include "dsp/FastMath.h"

namespace
{
constexpr uint64_t kGateStepMask = 0xFFFFFFFFFFFFFFFFull;
constexpr float kGateEnvMinMs = 5.0f;
constexpr float kGateEnvMaxMs = 600.0f;
constexpr float kGateSustainMinPct = 0.0f;
constexpr float kGateSustainMaxPct = 100.0f;
constexpr float kGateSwingMinPct = 10.0f;
constexpr float kGateSwingMaxPct = 90.0f;

struct CharacterFlavor
{
    float gritRate;
    float gritBits;
    float gritDriveDb;
    float gritOutDb;
    float gritJitter;
    float gritMix;
    float exciterDrive;
    float exciterToneHz;
    float exciterMix;
};

constexpr std::array<CharacterFlavor, 6> kCharacterFlavors {{
    // Dust
    { 26040.0f, 14.0f, 3.0f, -1.5f, 0.12f, 0.45f, 0.24f, 5200.0f, 0.12f },
    // Glass
    { 44100.0f, 18.0f, 1.0f, -0.5f, 0.02f, 0.16f, 0.28f, 9600.0f, 0.36f },
    // Rust
    { 16000.0f, 10.0f, 7.0f, -4.0f, 0.26f, 0.68f, 0.36f, 3600.0f, 0.14f },
    // Heat
    { 44100.0f, 16.0f, 8.0f, -4.0f, 0.04f, 0.52f, 0.34f, 6500.0f, 0.20f },
    // Broken
    { 7000.0f, 6.0f, 12.0f, -6.0f, 0.42f, 0.86f, 0.64f, 4300.0f, 0.30f },
    // Glow
    { 44100.0f, 18.0f, 2.0f, -1.0f, 0.02f, 0.22f, 0.30f, 8200.0f, 0.46f },
}};

float blendToward(float current, float target, float amount)
{
    return current + (target - current) * amount;
}

std::array<uint64_t, StardustProcessor::kGatePatternSlots> defaultGateEnabledMasks()
{
    return {
        0xAAAAAAAAAAAAAAAAull  // alternating
    };
}

int sanitizeChainSlotValue(float value)
{
    if (!std::isfinite(value))
        return 0;

    const int fx = juce::roundToInt(value);

    // Redesigned chain only supports GRIT and EXCITER.
    switch (fx)
    {
        case 1:
        case 2:
            return fx;
        default:
            return 0;
    }
}

void sanitizePresetValuesForRemovedEffects(std::map<juce::String, float>& values)
{
    // Legacy stutter presets stored rate in milliseconds; slot 10 now uses chunks.
    if (const auto it = values.find("stutterRate"); it != values.end())
    {
        if (!std::isfinite(it->second) || it->second < 2.0f)
            values["stutterRate"] = 16.0f;
        else if (it->second > 64.0f)
            values["stutterRate"] = 16.0f;
    }

    // Legacy gate envelope/swing presets stored normalized 0..1 values.
    const auto looksLikeLegacyGatePreset = [&values]() -> bool
    {
        static constexpr const char* kEnvelopeKeys[] = {
            "stutterAttack", "stutterDecay", "stutterRelease"
        };

        for (const auto* key : kEnvelopeKeys)
        {
            const auto it = values.find(key);
            if (it == values.end())
                continue;

            const float v = it->second;
            if (std::isfinite(v) && v >= 0.0f && v <= 1.0f)
                return true;
        }

        return false;
    }();

    auto remapLegacyNorm = [&values](const char* key, float minValue, float maxValue, float fallback)
    {
        if (const auto it = values.find(key); it != values.end())
        {
            const float v = it->second;
            if (!std::isfinite(v))
                values[key] = fallback;
            else if (v >= 0.0f && v <= 1.0f)
                values[key] = juce::jmap(v, 0.0f, 1.0f, minValue, maxValue);
        }
    };

    remapLegacyNorm("stutterAttack",  kGateEnvMinMs,      kGateEnvMaxMs,      35.0f);
    remapLegacyNorm("stutterDecay",   kGateEnvMinMs,      kGateEnvMaxMs,      124.0f);
    remapLegacyNorm("stutterRelease", kGateEnvMinMs,      kGateEnvMaxMs,      65.0f);
    remapLegacyNorm("stutterSwing",   kGateSwingMinPct,   kGateSwingMaxPct,   10.0f);

    if (looksLikeLegacyGatePreset)
    {
        if (const auto it = values.find("stutterDepth"); it != values.end())
        {
            const float v = it->second;
            if (!std::isfinite(v))
                values["stutterDepth"] = 100.0f;
            else if (v >= 0.0f && v <= 1.0f)
                values["stutterDepth"] = v * 100.0f;
        }
    }

    for (int slot = 0; slot < 4; ++slot)
    {
        const juce::String key = "chainSlot" + juce::String(slot);
        const auto it = values.find(key);
        if (it == values.end())
            continue;

        it->second = static_cast<float>(sanitizeChainSlotValue(it->second));
    }
}

void sanitizeChainSlotParameters(juce::AudioProcessorValueTreeState& apvts)
{
    for (int slot = 0; slot < 4; ++slot)
    {
        const juce::String paramId = "chainSlot" + juce::String(slot);
        auto* raw = apvts.getRawParameterValue(paramId);
        auto* param = dynamic_cast<juce::AudioParameterInt*>(apvts.getParameter(paramId));
        if (raw == nullptr || param == nullptr)
            continue;

        const int current = juce::roundToInt(raw->load(std::memory_order_relaxed));
        const int sanitized = sanitizeChainSlotValue(static_cast<float>(current));
        if (sanitized != current)
            param->setValueNotifyingHost(param->convertTo0to1(static_cast<float>(sanitized)));
    }
}

void sanitizeGateParameters(juce::AudioProcessorValueTreeState& apvts)
{
    auto* raw = apvts.getRawParameterValue("stutterRate");
    auto* param = dynamic_cast<juce::AudioParameterInt*>(apvts.getParameter("stutterRate"));
    if (raw == nullptr || param == nullptr)
        return;

    const int current = juce::roundToInt(raw->load(std::memory_order_relaxed));
    const int sanitized = (current < 2 || current > 64) ? 16 : current;
    if (sanitized != current)
        param->setValueNotifyingHost(param->convertTo0to1(static_cast<float>(sanitized)));
}
}

StardustProcessor::StardustProcessor()
    : AudioProcessor(BusesProperties()
          .withInput("Input", juce::AudioChannelSet::stereo(), true)
          .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, &undoManager, "Parameters", createParameterLayout())
{
    resetGatePatternsToDefaults();
    initFactoryPresets();
    refreshPresets();
    loadPreset(0);
}

juce::AudioProcessorValueTreeState::ParameterLayout StardustProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("destroyIn", 1), "Destroy In",
        juce::NormalisableRange<float>(0.0f, 24.0f, 0.1f), 0.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("destroyOut", 1), "Destroy Out",
        juce::NormalisableRange<float>(-12.0f, 12.0f, 0.1f), 0.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("destroyMix", 1), "Destroy Mix",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 1.0f));

    {
        juce::NormalisableRange<float> rateRange(250.0f, 96000.0f, 1.0f);
        rateRange.setSkewForCentre(3000.0f); // 3kHz at knob centre, log feel
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("destroyFader", 1), "Destroy Fader", rateRange, 22082.0f));
    }

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("destroyBits", 1), "Destroy Bits",
        juce::NormalisableRange<float>(3.0f, 24.0f, 0.001f), 12.0f));  // 12-bit SP-950 authentic

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("destroyJitter", 1), "Destroy Jitter",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.10f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("chorusMix", 1), "Chorus Mix",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 1.0f));

    // Juno-60 chorus mode: 0=I, 1=II, 2=I+II
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID("junoMode", 1), "Chorus Mode",
        juce::StringArray{"I", "II", "I+II"}, 0));

    // Modulation LFOs (2 LFOs x 4 params = 8 params)
    for (int lfo = 1; lfo <= 2; ++lfo)
    {
        const auto prefix = juce::String("modLfo") + juce::String(lfo);
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID(prefix + "Rate", 1), "LFO " + juce::String(lfo) + " Rate",
            juce::NormalisableRange<float>(0.01f, 20.0f, 0.01f, 0.4f), 1.0f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID(prefix + "Depth", 1), "LFO " + juce::String(lfo) + " Depth",
            juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.0f));
        params.push_back(std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID(prefix + "Wave", 1), "LFO " + juce::String(lfo) + " Wave",
            juce::StringArray{"Sine", "Triangle", "Saw", "Random"}, 0));
        params.push_back(std::make_unique<juce::AudioParameterBool>(
            juce::ParameterID(prefix + "Sync", 1), "LFO " + juce::String(lfo) + " Sync", false));
    }

    // Modulation routing slots (4 slots x 3 params = 12 params)
    const juce::StringArray sourceNames {"Off", "LFO 1", "LFO 2", "Env Follower"};
    const juce::StringArray targetNames {"Off",
                                         "Tape Drive", "Tape Wear", "Tape Mix"};
    for (int slot = 1; slot <= 4; ++slot)
    {
        const auto prefix = juce::String("modSlot") + juce::String(slot);
        params.push_back(std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID(prefix + "Src", 1), "Slot " + juce::String(slot) + " Source",
            sourceNames, 0));
        params.push_back(std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID(prefix + "Tgt", 1), "Slot " + juce::String(slot) + " Target",
            targetNames, 0));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID(prefix + "Amt", 1), "Slot " + juce::String(slot) + " Amount",
            juce::NormalisableRange<float>(-1.0f, 1.0f, 0.01f), 0.0f));
    }

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("tapeDrive", 1), "Tape Drive",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.17f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("tapeInput", 1), "Tape Input",
        juce::NormalisableRange<float>(-12.0f, 12.0f, 0.1f), 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("tapeGlue", 1), "Tape Glue",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.48f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("tapeNoise", 1), "Tape Noise",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.07f));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID("tapeNoiseSpeed", 1), "Tape Noise Speed",
        juce::StringArray{"7.5 ips", "15 ips", "30 ips"}, 1));

    // Tape formulation hardcoded to Ampex 456 (index 0) — no user selection
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID("tapeFormulation", 1), "Tape Formulation",
        juce::StringArray{"456"}, 0));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("tapeMix", 1), "Tape Mix",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 1.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("tapeOutput", 1), "Tape Output",
        juce::NormalisableRange<float>(-24.0f, 12.0f, 0.1f, 1.71f), 0.0f));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID("tapeEnabled", 1), "Tape Enabled", false));
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID("destroyEnabled", 1), "Destroy Enabled", false));
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID("multiplyEnabled", 1), "Multiply Enabled", false));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("inputGain", 1), "Input Gain",
        juce::NormalisableRange<float>(-24.0f, 12.0f, 0.1f, 1.71f), 0.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("outputGain", 1), "Output Gain",
        juce::NormalisableRange<float>(-24.0f, 12.0f, 0.1f, 1.71f), 0.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("masterMix", 1), "Master Mix",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 1.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("msWidth", 1), "M/S Width",
        juce::NormalisableRange<float>(0.0f, 2.0f, 0.01f), 1.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("distortionDrive", 1), "Distortion Drive",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.5f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("distortionTone", 1), "Distortion Tone",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.5f));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID("distortionMode", 1), "Distortion Mode",
        juce::StringArray{"Soft", "Tube", "Hard"}, 0));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("distortionMix", 1), "Distortion Mix",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 1.0f));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID("distortionEnabled", 1), "Distortion Enabled", false));

    // Legacy slot assignments are retained for session/preset compatibility.
    // Runtime now processes a fixed chain (GRIT -> EXCITER).
    for (int i = 0; i < 4; ++i)
        params.push_back(std::make_unique<juce::AudioParameterInt>(
            juce::ParameterID("chainSlot" + juce::String(i), 1),
            "FX Slot " + juce::String(i + 1), 0, 13, 0));

    // Deprecated removed-effect parameters retained only so older sessions/presets
    // can restore automation/state without changing the plugin parameter layout abruptly.
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID("reverserEnabled", 1), "Deprecated Reverser Enabled", false));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("reverserRepeat", 1), "Deprecated Reverser Repeat",
        juce::NormalisableRange<float>(1.0f, 16.0f, 1.0f), 4.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("reverserDivision", 1), "Deprecated Reverser Division",
        juce::NormalisableRange<float>(0.0f, 5.0f, 1.0f), 3.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("reverserCrossfade", 1), "Deprecated Reverser Crossfade",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.15f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("reverserMix", 1), "Deprecated Reverser Mix",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.5f));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID("flipbedEnabled", 1), "Deprecated Flipbed Enabled", false));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("flipbedSlice", 1), "Deprecated Flipbed Slice",
        juce::NormalisableRange<float>(20.0f, 500.0f, 1.0f), 160.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("flipbedFlip", 1), "Deprecated Flipbed Flip",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.65f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("flipbedOrbit", 1), "Deprecated Flipbed Orbit",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.35f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("flipbedSmear", 1), "Deprecated Flipbed Smear",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.40f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("flipbedDirt", 1), "Deprecated Flipbed Dirt",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.20f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("flipbedDuck", 1), "Deprecated Flipbed Duck",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.55f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("flipbedMix", 1), "Deprecated Flipbed Mix",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.25f));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID("orbitMirrorEnabled", 1), "Deprecated Orbit Mirror Enabled", false));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("orbitMirrorWindow", 1), "Deprecated Orbit Mirror Window",
        juce::NormalisableRange<float>(20.0f, 500.0f, 1.0f), 180.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("orbitMirrorSpeed", 1), "Deprecated Orbit Mirror Speed",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.50f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("orbitMirrorRadius", 1), "Deprecated Orbit Mirror Radius",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.35f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("orbitMirrorCrossfade", 1), "Deprecated Orbit Mirror Crossfade",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.40f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("orbitMirrorDuck", 1), "Deprecated Orbit Mirror Duck",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.50f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("orbitMirrorMix", 1), "Deprecated Orbit Mirror Mix",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.22f));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID("frayLoopEnabled", 1), "Deprecated Fray Loop Enabled", false));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("frayLoopSlice", 1), "Deprecated Fray Loop Slice",
        juce::NormalisableRange<float>(20.0f, 500.0f, 1.0f), 90.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("frayLoopSensitivity", 1), "Deprecated Fray Loop Sensitivity",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.60f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("frayLoopFray", 1), "Deprecated Fray Loop Fray",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.45f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("frayLoopDrift", 1), "Deprecated Fray Loop Drift",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.18f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("frayLoopSmear", 1), "Deprecated Fray Loop Smear",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.30f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("frayLoopMix", 1), "Deprecated Fray Loop Mix",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.28f));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID("halftimeEnabled", 1), "Deprecated Halftime Enabled", false));
    params.push_back(std::make_unique<juce::AudioParameterInt>(
        juce::ParameterID("halftimeDivision", 1), "Deprecated Halftime Division", 0, 7, 3));
    params.push_back(std::make_unique<juce::AudioParameterInt>(
        juce::ParameterID("halftimeSpeed", 1), "Deprecated Halftime Speed", 0, 1, 0));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("halftimeFade", 1), "Deprecated Halftime Fade",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.15f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("halftimeMix", 1), "Deprecated Halftime Mix",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.5f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("characterAmount", 1), "Character",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID("characterMode", 1), "Flavor",
        juce::StringArray{"Dust", "Glass", "Rust", "Heat", "Broken", "Glow"}, 0));

    // Reverb (standalone Dattorro plate)
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("reverbMix", 1), "Reverb Mix",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.5f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("reverbSize", 1), "Reverb Size",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.5f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("reverbDecay", 1), "Reverb Decay",
        juce::NormalisableRange<float>(0.0f, 0.99f, 0.01f), 0.5f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("reverbDamp", 1), "Reverb Damp",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.7f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("reverbPreDelay", 1), "Reverb Pre-Delay",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.5f), 10.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("reverbDiffusion", 1), "Reverb Diffusion",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.7f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("reverbWidth", 1), "Reverb Width",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 1.0f));
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID("reverbEnabled", 1), "Reverb Enabled", false));

    // Harmonic Exciter (non-reverse feature)
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID("exciterEnabled", 1), "Exciter Enabled", false));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("exciterDrive", 1), "Exciter Drive",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.35f));
    {
        juce::NormalisableRange<float> toneRange(800.0f, 16000.0f, 1.0f);
        toneRange.setSkewForCentre(6000.0f);
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("exciterTone", 1), "Exciter Tone", toneRange, 6000.0f));
    }
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("exciterMix", 1), "Exciter Mix",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.25f));

    // --- Feature additions ---

    // Tape: combined Wobble (drives both wow + flutter), Print-Through level, EQ standard, Motor
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("tapeWow", 1), "Tape Wobble",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.07f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("tapePrintThrough", 1), "Tape Print-Through",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.3f));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID("tapeEQStandard", 1), "Tape EQ Standard",
        juce::StringArray{"NAB", "IEC"}, 0));
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID("tapeMotorEnabled", 1), "Tape Motor", true));

    // NOISE: noise texture injection (slot 7)
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("hazeColor", 1), "Noise Color",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.5f));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID("hazeType", 1), "Noise Type",
        juce::StringArray{"Pink", "White", "Vinyl"}, 0));
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID("hazeEnabled", 1), "Noise Enabled", false));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("hazeMix", 1), "Noise Mix",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.15f));

    // MULTIPLY: simple 4-voice unison thickener (slot 8)
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID("unisonEnabled", 1), "Multiply Enabled", false));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("unisonMix", 1), "Multiply Mix",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.5f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("unisonSpeed", 1), "Multiply Speed",
        juce::NormalisableRange<float>(0.5f, 2.0f, 0.01f), 1.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("unisonOuter", 1), "Multiply 1+2",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 1.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("unisonInner", 1), "Multiply 3+4",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.8f));

    // GATE: trance-gate sequencer (slot 10)
    params.push_back(std::make_unique<juce::AudioParameterInt>(
        juce::ParameterID("stutterRate", 1), "Gate Chunks", 2, 64, 16));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("stutterAttack", 1), "Gate Attack",
        juce::NormalisableRange<float>(kGateEnvMinMs, kGateEnvMaxMs, 1.0f), 35.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("stutterDecay", 1), "Gate Decay",
        juce::NormalisableRange<float>(kGateEnvMinMs, kGateEnvMaxMs, 1.0f), 124.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("stutterDepth", 1), "Gate Sustain",
        juce::NormalisableRange<float>(kGateSustainMinPct, kGateSustainMaxPct, 1.0f), 100.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("stutterRelease", 1), "Gate Release",
        juce::NormalisableRange<float>(kGateEnvMinMs, kGateEnvMaxMs, 1.0f), 65.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("stutterSwing", 1), "Gate Swing",
        juce::NormalisableRange<float>(kGateSwingMinPct, kGateSwingMaxPct, 1.0f), 10.0f));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID("stutterResolution", 1), "Gate Resolution",
        juce::StringArray{"1/8", "1/8T", "1/16", "1/32", "1/32T", "1/64"}, 2));
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID("stutterEnabled", 1), "Gate Enabled", false));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("stutterMix", 1), "Gate Mix",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 1.0f));

    // SHIFT: pitch shifter (slot 11)
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("shiftPitch", 1), "Shift Pitch",
        juce::NormalisableRange<float>(-24.0f, 24.0f, 1.0f), 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("shiftFeedback", 1), "Shift Feedback",
        juce::NormalisableRange<float>(0.0f, 0.95f, 0.01f), 0.0f));
    {
        juce::NormalisableRange<float> toneRange(200.0f, 20000.0f, 1.0f);
        toneRange.setSkewForCentre(2000.0f);
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("shiftTone", 1), "Shift Tone", toneRange, 10000.0f));
    }
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID("shiftEnabled", 1), "Shift Enabled", false));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("shiftMix", 1), "Shift Mix",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.5f));

    
    return { params.begin(), params.end() };
}

void StardustProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;

    // CRUSH DSP modules run at native rate (Degrader-style: no oversampling)
    bitCrusher.prepare(sampleRate, samplesPerBlock);

    // Pre-allocate with generous headroom for oversampled data
    // Use 8x to handle hosts that exceed the declared block size
    dryBuffer.setSize(2, samplesPerBlock * 8, false, true, true);
    masterDryBuffer.setSize(2, samplesPerBlock * 2, false, true, true);
    msWidthSmoothed.reset(sampleRate, 0.02);
    inputGainSmoothed.reset(sampleRate, 0.01);
    outputGainSmoothed.reset(sampleRate, 0.01);
    characterAmountSmoothed.reset(sampleRate, 0.02);
    characterAmountSmoothed.setCurrentAndTargetValue(0.0f);

    // Prepare Harmonic Exciter
    exciter.prepare(sampleRate, samplesPerBlock);

    setLatencySamples(0);

}

void StardustProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                     juce::MidiBuffer& /*midiMessages*/)
{
    juce::ScopedNoDenormals noDenormals;

    const auto totalNumInputChannels = getTotalNumInputChannels();
    const auto totalNumOutputChannels = getTotalNumOutputChannels();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear(i, 0, buffer.getNumSamples());

    // Flush all DSP state on preset change — prevents old reverb tails and
    // delay contents from bleeding into the new preset.
    // Uses a short 4ms fade-out to avoid clicks from the hard buffer reset.
    if (presetChangeReset.exchange(false))
    {
        // Quick fade-out of current buffer to avoid click (4ms or remaining samples)
        const int fadeSamples = std::min(buffer.getNumSamples(),
                                         static_cast<int>(currentSampleRate * 0.004));
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            auto* data = buffer.getWritePointer(ch);
            for (int i = 0; i < fadeSamples; ++i)
                data[i] *= static_cast<float>(fadeSamples - i) / static_cast<float>(fadeSamples);
            for (int i = fadeSamples; i < buffer.getNumSamples(); ++i)
                data[i] = 0.0f;
        }

        // Reset active DSP modules (zeroes buffers, resets positions)
        const int blk = buffer.getNumSamples();
        bitCrusher.prepare(currentSampleRate, blk);

        // Reset new exciter as well
        exciter.prepare(currentSampleRate, blk);

        return; // skip processing this block — next block starts clean
    }

    // Main Input/Output/Mix parameters are kept for old sessions, but the
    // simplified UI fixes them to neutral behavior.
    inputGainSmoothed.setCurrentAndTargetValue(1.0f);
    outputGainSmoothed.setCurrentAndTargetValue(1.0f);

    // Measure input levels
    inputLevelLeft.store(buffer.getMagnitude(0, 0, buffer.getNumSamples()));
    if (buffer.getNumChannels() > 1)
        inputLevelRight.store(buffer.getMagnitude(1, 0, buffer.getNumSamples()));

    const auto numSamples = buffer.getNumSamples();
    const auto numChannels = buffer.getNumChannels();

    // Global dry/wet is fixed at 100% wet; section mix controls handle blending.
    constexpr float masterMixVal = 1.0f;
    constexpr bool masterDryOk = false;

    // Read parameters
    const float destroyInVal    = *apvts.getRawParameterValue("destroyIn");
    const float destroyOutVal   = *apvts.getRawParameterValue("destroyOut");
    const float destroyMixVal   = *apvts.getRawParameterValue("destroyMix");
    const float destroyFaderVal = *apvts.getRawParameterValue("destroyFader");
    const float destroyBitsVal  = *apvts.getRawParameterValue("destroyBits");
    const float destroyJitterVal = *apvts.getRawParameterValue("destroyJitter");
    const float exciterDriveVal = *apvts.getRawParameterValue("exciterDrive");
    const float exciterToneVal = *apvts.getRawParameterValue("exciterTone");
    const float exciterMixVal = *apvts.getRawParameterValue("exciterMix");
    const float characterAmountTarget = juce::jlimit(0.0f, 1.0f,
        apvts.getRawParameterValue("characterAmount")->load(std::memory_order_relaxed));
    characterAmountSmoothed.setTargetValue(characterAmountTarget);
    const float characterAmountVal = characterAmountSmoothed.getNextValue();
    if (numSamples > 1)
        characterAmountSmoothed.skip(numSamples - 1);
    const int characterModeVal = juce::jlimit(0, static_cast<int>(kCharacterFlavors.size()) - 1,
        juce::roundToInt(apvts.getRawParameterValue("characterMode")->load(std::memory_order_relaxed)));
    const auto& character = kCharacterFlavors[static_cast<size_t>(characterModeVal)];

    const float effectiveDestroyMix = blendToward(destroyMixVal,
                                                  character.gritMix,
                                                  characterAmountVal);
    const bool effectiveDestroyOn = effectiveDestroyMix > 0.001f;
    const float effectiveDestroyIn = blendToward(destroyInVal, character.gritDriveDb, characterAmountVal);
    const float effectiveDestroyOut = blendToward(destroyOutVal, character.gritOutDb, characterAmountVal);
    const float effectiveDestroyFader = blendToward(destroyFaderVal, character.gritRate, characterAmountVal);
    const float effectiveDestroyBits = blendToward(destroyBitsVal, character.gritBits, characterAmountVal);
    const float effectiveDestroyJitter = blendToward(destroyJitterVal, character.gritJitter, characterAmountVal);

    const float effectiveExciterMix = blendToward(exciterMixVal,
                                                  character.exciterMix,
                                                  characterAmountVal);
    const float effectiveExciterDrive = blendToward(exciterDriveVal, character.exciterDrive, characterAmountVal);
    const float effectiveExciterTone = blendToward(exciterToneVal, character.exciterToneHz, characterAmountVal);

    // If host exceeds our generous headroom (2x declared block size for
    // oversampling, 8x for dry buffer), skip oversampled processing rather
    // than allocating on the audio thread. Audio quality silently degrades
    // but no crash occurs.
    const bool dryBufferOk = dryBuffer.getNumSamples() >= numSamples * 4;

    // Fixed runtime chain order (independent of legacy chain slot params).
    static constexpr int kFixedFxChain[] = { 1, 2 };
    for (const int fx : kFixedFxChain)
    {
        switch (fx)
        {
            case 1: // CRUSH — Degrader-style: S&H → bit crush → saturation (no oversampling)
            {
                if (!effectiveDestroyOn) break;

                // Save dry copy for mix blending
                if (effectiveDestroyMix < 0.999f && dryBufferOk)
                    for (int ch = 0; ch < numChannels; ++ch)
                        dryBuffer.copyFrom(ch, 0, buffer, ch, 0, numSamples);

                // Guard against NaN/Inf from automation glitch
                const float safeFaderVal = (std::isfinite(effectiveDestroyFader)) ? effectiveDestroyFader : 26040.0f;
                const float effectiveRate = juce::jlimit(250.0f, 96000.0f, safeFaderVal);

                // 1. BitCrusher: S&H rate reduction → raw truncation (no dither)
                bitCrusher.setBitDepth(effectiveDestroyBits);
                bitCrusher.setSampleRate(effectiveRate);
                bitCrusher.setJitter(effectiveDestroyJitter);
                bitCrusher.process(buffer);

                // 2. Simple tanh saturation AFTER crush (Degrader-style soft clip)
                //    destroyIn (0–24 dB) drives into saturation, destroyOut adjusts output level
                if (effectiveDestroyIn > 0.1f)
                {
                    const float driveGain = std::pow(10.0f, effectiveDestroyIn / 20.0f);
                    const float outGain   = std::pow(10.0f, effectiveDestroyOut / 20.0f);
                    for (int ch = 0; ch < numChannels; ++ch)
                    {
                        auto* data = buffer.getWritePointer(ch);
                        for (int i = 0; i < numSamples; ++i)
                        {
                            const float x = data[i] * driveGain;
                            data[i] = FastMath::tanh(x) * outGain;
                        }
                    }
                }

                // Dry/wet mix
                if (effectiveDestroyMix < 0.999f && dryBufferOk)
                {
                    const float dryGain = std::cos(effectiveDestroyMix * juce::MathConstants<float>::halfPi);
                    const float wetGain = std::sin(effectiveDestroyMix * juce::MathConstants<float>::halfPi);
                    for (int ch = 0; ch < numChannels; ++ch)
                    {
                        auto* wet = buffer.getWritePointer(ch);
                        const auto* dry = dryBuffer.getReadPointer(ch);
                        for (int i = 0; i < numSamples; ++i)
                            wet[i] = dry[i] * dryGain + wet[i] * wetGain;
                    }
                }
                break;
            }

            case 2: // EXCITER - focused high-band harmonics after GRIT
            {
                exciter.process(buffer, effectiveExciterDrive, effectiveExciterTone, effectiveExciterMix);
                break;
            }
            
            default: break; // 0 = empty slot
        }
    }

    // M/S stereo width control (after all processing, before output gain)
    const float msWidthVal = *apvts.getRawParameterValue("msWidth");
    msWidthSmoothed.setTargetValue(msWidthVal);
    if (numChannels > 1 && std::abs(msWidthVal - 1.0f) > 0.01f)
    {
        auto* dataL = buffer.getWritePointer(0);
        auto* dataR = buffer.getWritePointer(1);
        for (int i = 0; i < numSamples; ++i)
        {
            const float width = msWidthSmoothed.getNextValue();
            const float mid  = (dataL[i] + dataR[i]) * 0.5f;
            const float side = (dataL[i] - dataR[i]) * 0.5f * width;
            dataL[i] = mid + side;
            dataR[i] = mid - side;
        }
    }

    // Master dry/wet blend (constant-power crossfade) — before limiter so dry path is also protected
    if (masterMixVal < 0.999f && masterDryOk)
    {
        // Apply output gain to dry buffer so both paths have consistent gain (fix 2.10)
        const float outGainLin = outputGainSmoothed.getCurrentValue();
        if (std::abs(outGainLin - 1.0f) > 0.001f)
        {
            for (int ch = 0; ch < numChannels; ++ch)
                masterDryBuffer.applyGain(ch, 0, numSamples, outGainLin);
        }

        const float dryGain = std::cos(masterMixVal * juce::MathConstants<float>::halfPi);
        const float wetGain = std::sin(masterMixVal * juce::MathConstants<float>::halfPi);
        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto* wet = buffer.getWritePointer(ch);
            const auto* dry = masterDryBuffer.getReadPointer(ch);
            for (int i = 0; i < numSamples; ++i)
                wet[i] = dry[i] * dryGain + wet[i] * wetGain;
        }
    }

    // Output safety limiter — catches runaway DSP only (threshold 2.0 = +6 dBFS).
    // Never engages on normal programme material; only prevents hard clipping from
    // internal processing bugs (e.g. J-A instability).
    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto* data = buffer.getWritePointer(ch);
        for (int i = 0; i < numSamples; ++i)
        {
            const float s = data[i];
            if (std::abs(s) > 2.0f)
                data[i] = FastMath::tanh(s * 0.5f) * 2.0f;
        }
    }

    // Measure output levels
    outputLevelLeft.store(buffer.getMagnitude(0, 0, buffer.getNumSamples()));
    if (buffer.getNumChannels() > 1)
        outputLevelRight.store(buffer.getMagnitude(1, 0, buffer.getNumSamples()));
}

void StardustProcessor::setCurrentProgram(int index)
{
    PresetLockGuard lock(presetLock);
    if (index < 0 || index >= static_cast<int>(allPresets.size())) return;
    loadPreset(index);
}

const juce::String StardustProcessor::getProgramName(int index)
{
    PresetLockGuard g(presetLock);
    if (index >= 0 && index < static_cast<int>(allPresets.size()))
        return allPresets[static_cast<size_t>(index)].name;
    return {};
}

void StardustProcessor::loadPreset(int index)
{
    // Copy values under lock, then apply outside lock to avoid deadlock
    // with host callbacks that may re-enter
    std::map<juce::String, float> values;
    {
        PresetLockGuard g(presetLock);
        if (index < 0 || index >= static_cast<int>(allPresets.size()))
            return;
        values = allPresets[static_cast<size_t>(index)].values;
        currentPresetIndex.store(index);
    }

    sanitizePresetValuesForRemovedEffects(values);

    for (const auto& [paramId, value] : values)
    {
        if (auto* param = apvts.getParameter(paramId))
            param->setValueNotifyingHost(param->convertTo0to1(value));
    }

    // Auto-enable any effect that is assigned to a chain slot.
    // Prevents presets from having visible but disabled effects.
    static const std::pair<int, const char*> slotEnableMap[] = {
        { 1,  "destroyEnabled" },
        { 2,  "exciterEnabled" },
    };
    for (int slot = 0; slot < 4; ++slot)
    {
        const auto key = "chainSlot" + juce::String(slot);
        const auto it = values.find(key);
        if (it == values.end()) continue;
        const int fx = static_cast<int>(it->second);
        if (fx == 0) continue;
        for (const auto& [id, enableParam] : slotEnableMap)
        {
            if (id == fx)
            {
                if (auto* param = apvts.getParameter(enableParam))
                    param->setValueNotifyingHost(1.0f);
                break;
            }
        }
    }

    // Snapshot actual normalized values after host has applied them
    {
        PresetLockGuard g(presetLock);
        loadedPresetNormValues.clear();
        for (const auto& [paramId, value] : values)
        {
            if (auto* param = apvts.getParameter(paramId))
                loadedPresetNormValues[paramId] = param->getValue();
        }
        presetLoadGrace.store(10); // skip dirty check for 10 frames (~330ms)
    }

    presetDirty.store(false);
    presetChangeReset.store(true); // tell processBlock to flush DSP buffers
}

bool StardustProcessor::isFactoryPreset(int index) const
{
    PresetLockGuard g(presetLock);
    if (index < 0 || index >= static_cast<int>(allPresets.size()))
        return false;
    return allPresets[static_cast<size_t>(index)].isFactory;
}

juce::File StardustProcessor::getUserPresetsDir()
{
    auto dir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                   .getChildFile("Stardust")
                   .getChildFile("Presets");
    if (!dir.exists())
    {
        auto result = dir.createDirectory();
        if (result.failed())
            DBG("Failed to create presets directory: " + result.getErrorMessage());
    }
    return dir;
}

std::set<juce::String> StardustProcessor::loadFavorites()
{
    std::set<juce::String> favs;
    auto file = getUserPresetsDir().getChildFile("favorites.json");
    if (!file.existsAsFile()) return favs;

    auto text = file.loadFileAsString();
    auto json = juce::JSON::parse(text);
    if (auto* arr = json.getArray())
    {
        for (const auto& item : *arr)
            favs.insert(item.toString());
    }
    return favs;
}

void StardustProcessor::saveFavorites(const std::set<juce::String>& favs)
{
    juce::Array<juce::var> arr;
    for (const auto& name : favs)
        arr.add(juce::var(name));

    auto file = getUserPresetsDir().getChildFile("favorites.json");
    file.replaceWithText(juce::JSON::toString(juce::var(arr)));
}

void StardustProcessor::saveUserPreset(const juce::String& name)
{
    saveUserPreset(name, "");
}

void StardustProcessor::saveUserPreset(const juce::String& name, const juce::String& bank)
{
    auto safeName = name.removeCharacters("/\\:*?\"<>|").trim().substring(0, 64);
    if (safeName.isEmpty()) return;
    auto safeBank = bank.removeCharacters("/\\:*?\"<>|").trim().substring(0, 64);

    auto xml = std::make_unique<juce::XmlElement>("StardustPreset");
    xml->setAttribute("name", safeName);
    xml->setAttribute("version", STARDUST_VERSION);

    for (auto* param : getParameters())
    {
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*>(param))
        {
            auto* child = xml->createNewChildElement("Parameter");
            child->setAttribute("id", ranged->getParameterID());
            child->setAttribute("value", ranged->convertFrom0to1(ranged->getValue()));
        }
    }

    auto dir = getUserPresetsDir();
    if (safeBank.isNotEmpty())
    {
        dir = dir.getChildFile(safeBank);
        if (!dir.exists())
            dir.createDirectory();
    }

    auto file = dir.getChildFile(safeName + ".xml");
    if (!xml->writeTo(file))
        DBG("Failed to save preset: " + file.getFullPathName());
    presetDirty.store(false);
    refreshPresets();
}

void StardustProcessor::deleteUserPreset(int index)
{
    juce::String fileName;
    juce::String bankName;
    {
        PresetLockGuard g(presetLock);
        if (index < 0 || index >= static_cast<int>(allPresets.size()))
            return;
        if (allPresets[static_cast<size_t>(index)].isFactory)
            return;
        fileName = allPresets[static_cast<size_t>(index)].name;
        bankName = allPresets[static_cast<size_t>(index)].bank;
    }

    auto dir = getUserPresetsDir();
    if (bankName.isNotEmpty())
        dir = dir.getChildFile(bankName);

    auto file = dir.getChildFile(fileName + ".xml");
    if (file.existsAsFile() && !file.deleteFile())
        DBG("Failed to delete preset: " + file.getFullPathName());

    refreshPresets();
    {
        PresetLockGuard g(presetLock);
        if (currentPresetIndex.load() >= static_cast<int>(allPresets.size()))
            currentPresetIndex.store(0);
    }
}

void StardustProcessor::scanUserPresets()
{
    auto dir = getUserPresetsDir();

    // Helper: parse XML files from a directory into allPresets
    auto scanDir = [this](const juce::File& folder, const juce::String& bankName)
    {
        auto files = folder.findChildFiles(juce::File::findFiles, false, "*.xml");
        files.sort();

        for (const auto& file : files)
        {
            auto xml = juce::XmlDocument::parse(file);
            if (xml == nullptr || !xml->hasTagName("StardustPreset"))
                continue;

            Preset preset;
            preset.name = xml->getStringAttribute("name", file.getFileNameWithoutExtension()).substring(0, 32);
            preset.isFactory = false;
            preset.bank = bankName;

            for (auto* child : xml->getChildIterator())
            {
                if (child->hasTagName("Parameter"))
                {
                    auto id = child->getStringAttribute("id");
                    auto val = static_cast<float>(child->getDoubleAttribute("value"));
                    if (id.isNotEmpty())
                        preset.values[id] = val;
                }
            }

            allPresets.push_back(std::move(preset));
        }
    };

    // Scan root directory (USER bank)
    scanDir(dir, "");

    // Scan subdirectories (custom banks)
    auto subdirs = dir.findChildFiles(juce::File::findDirectories, false);
    subdirs.sort();
    for (const auto& subdir : subdirs)
        scanDir(subdir, subdir.getFileName());
}

void StardustProcessor::rebuildAllPresets()
{
    allPresets.clear();
    for (const auto& fp : factoryPresets)
    {
        Preset p = { fp.name, fp.values, true, fp.bank };
        allPresets.push_back(std::move(p));
    }
    scanUserPresets();
}

void StardustProcessor::refreshPresets()
{
    PresetLockGuard g(presetLock);
    rebuildAllPresets();
    if (currentPresetIndex.load() >= static_cast<int>(allPresets.size()))
        currentPresetIndex.store(0);
}

uint64_t StardustProcessor::getGatePatternEnabledMask(int slot) const
{
    const int safeSlot = juce::jlimit(0, kGatePatternSlots - 1, slot);
    return gatePatternEnabledMasks[static_cast<size_t>(safeSlot)].load(std::memory_order_relaxed);
}

uint64_t StardustProcessor::getGatePatternTieMask(int slot) const
{
    const int safeSlot = juce::jlimit(0, kGatePatternSlots - 1, slot);
    return gatePatternTieMasks[static_cast<size_t>(safeSlot)].load(std::memory_order_relaxed);
}

void StardustProcessor::setGatePatternMasks(int slot, uint64_t enabledMask, uint64_t tieMask)
{
    const int safeSlot = juce::jlimit(0, kGatePatternSlots - 1, slot);
    const uint64_t clippedEnabled = enabledMask & kGateStepMask;
    gatePatternEnabledMasks[static_cast<size_t>(safeSlot)].store(clippedEnabled, std::memory_order_relaxed);
    gatePatternTieMasks[static_cast<size_t>(safeSlot)].store(tieMask & clippedEnabled & kGateStepMask, std::memory_order_relaxed);
    presetDirty.store(true);
}

bool StardustProcessor::getGatePatternStepEnabled(int slot, int step) const
{
    const int safeStep = juce::jlimit(0, kGatePatternSteps - 1, step);
    const uint64_t mask = getGatePatternEnabledMask(slot);
    return (mask & (uint64_t(1) << static_cast<uint32_t>(safeStep))) != 0u;
}

bool StardustProcessor::getGatePatternStepTied(int slot, int step) const
{
    const int safeStep = juce::jlimit(0, kGatePatternSteps - 1, step);
    const uint64_t mask = getGatePatternTieMask(slot);
    return (mask & (uint64_t(1) << static_cast<uint32_t>(safeStep))) != 0u;
}

void StardustProcessor::setGatePatternStepEnabled(int slot, int step, bool enabled)
{
    const int safeSlot = juce::jlimit(0, kGatePatternSlots - 1, slot);
    const int safeStep = juce::jlimit(0, kGatePatternSteps - 1, step);
    const uint64_t bit = (uint64_t(1) << static_cast<uint32_t>(safeStep));

    auto enabledMask = gatePatternEnabledMasks[static_cast<size_t>(safeSlot)].load(std::memory_order_relaxed);
    enabledMask = enabled ? (enabledMask | bit) : (enabledMask & ~bit);
    gatePatternEnabledMasks[static_cast<size_t>(safeSlot)].store(enabledMask & kGateStepMask, std::memory_order_relaxed);

    auto tieMask = gatePatternTieMasks[static_cast<size_t>(safeSlot)].load(std::memory_order_relaxed);
    // If a step is disabled, ties to and from that step are cleared.
    if (!enabled)
    {
        tieMask &= ~bit;
        if (safeStep > 0)
            tieMask &= ~(uint64_t(1) << static_cast<uint32_t>(safeStep - 1));
    }
    gatePatternTieMasks[static_cast<size_t>(safeSlot)].store(tieMask & enabledMask & kGateStepMask, std::memory_order_relaxed);
    presetDirty.store(true);
}

void StardustProcessor::setGatePatternStepTied(int slot, int step, bool tied)
{
    const int safeSlot = juce::jlimit(0, kGatePatternSlots - 1, slot);
    const int safeStep = juce::jlimit(0, kGatePatternSteps - 1, step);
    const uint64_t bit = (uint64_t(1) << static_cast<uint32_t>(safeStep));

    auto tieMask = gatePatternTieMasks[static_cast<size_t>(safeSlot)].load(std::memory_order_relaxed);
    const auto enabledMask = gatePatternEnabledMasks[static_cast<size_t>(safeSlot)].load(std::memory_order_relaxed);

    // Tie means "hold into next step", so we require source and next step enabled.
    const bool hasNext = safeStep < (kGatePatternSteps - 1);
    const uint64_t nextBit = hasNext ? (uint64_t(1) << static_cast<uint32_t>(safeStep + 1)) : 0u;
    const bool canTie = hasNext && ((enabledMask & bit) != 0u) && ((enabledMask & nextBit) != 0u);

    if (tied && canTie)
        tieMask |= bit;
    else
        tieMask &= ~bit;

    gatePatternTieMasks[static_cast<size_t>(safeSlot)].store(tieMask & enabledMask & kGateStepMask, std::memory_order_relaxed);
    presetDirty.store(true);
}

void StardustProcessor::resetGatePatternsToDefaults()
{
    const auto defaults = defaultGateEnabledMasks();
    for (int slot = 0; slot < kGatePatternSlots; ++slot)
    {
        gatePatternEnabledMasks[static_cast<size_t>(slot)].store(defaults[static_cast<size_t>(slot)] & kGateStepMask,
                                                                  std::memory_order_relaxed);
        gatePatternTieMasks[static_cast<size_t>(slot)].store(0ull, std::memory_order_relaxed);
    }
}

std::vector<juce::String> StardustProcessor::getUserBanks() const
{
    std::vector<juce::String> banks;
    auto dir = getUserPresetsDir();
    auto subdirs = dir.findChildFiles(juce::File::findDirectories, false);
    subdirs.sort();
    for (const auto& subdir : subdirs)
        banks.push_back(subdir.getFileName());
    return banks;
}

void StardustProcessor::importBank(const juce::File& sourceFolder)
{
    if (!sourceFolder.isDirectory())
        return;

    auto dir = getUserPresetsDir();
    auto bankName = sourceFolder.getFileName();

    // Avoid name collision
    auto target = dir.getChildFile(bankName);
    int suffix = 2;
    while (target.exists())
    {
        target = dir.getChildFile(bankName + " " + juce::String(suffix));
        ++suffix;
    }
    target.createDirectory();

    auto files = sourceFolder.findChildFiles(juce::File::findFiles, false, "*.xml");
    for (const auto& file : files)
    {
        if (file.isSymbolicLink() || file.getSize() > 1024 * 1024)
            continue;
        file.copyFileTo(target.getChildFile(file.getFileName()));
    }

    refreshPresets();
}

void StardustProcessor::deleteUserBank(const juce::String& bankName)
{
    if (bankName.isEmpty() || bankName.containsChar('/') || bankName.containsChar('\\') || bankName == ".." || bankName == ".")
        return;
    auto dir = getUserPresetsDir().getChildFile(bankName);
    if (!dir.isAChildOf(getUserPresetsDir()))
        return;
    if (dir.isDirectory())
        dir.deleteRecursively();
    refreshPresets();
}

void StardustProcessor::renameUserBank(const juce::String& oldName, const juce::String& newName)
{
    if (oldName.isEmpty() || newName.isEmpty() || oldName == newName)
        return;
    if (oldName.containsChar('/') || oldName.containsChar('\\') || oldName == ".." || oldName == ".")
        return;
    if (newName.containsChar('/') || newName.containsChar('\\') || newName == ".." || newName == ".")
        return;
    auto dir = getUserPresetsDir();
    auto oldDir = dir.getChildFile(oldName);
    auto newDir = dir.getChildFile(newName);
    if (!oldDir.isAChildOf(dir) || !newDir.isAChildOf(dir))
        return;
    if (oldDir.isDirectory() && !newDir.exists())
        oldDir.moveFileTo(newDir);
    refreshPresets();
}

void StardustProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    {
        PresetLockGuard g(presetLock);
        const int idx = currentPresetIndex.load();
        state.setProperty("presetIndex", idx, nullptr);
        if (idx >= 0 && idx < static_cast<int>(allPresets.size()))
            state.setProperty("presetName", allPresets[static_cast<size_t>(idx)].name, nullptr);
    }

    for (int slot = 0; slot < kGatePatternSlots; ++slot)
    {
        state.setProperty("gatePatternOn" + juce::String(slot),
            static_cast<juce::int64>(gatePatternEnabledMasks[static_cast<size_t>(slot)].load(std::memory_order_relaxed)),
            nullptr);
        state.setProperty("gatePatternTie" + juce::String(slot),
            static_cast<juce::int64>(gatePatternTieMasks[static_cast<size_t>(slot)].load(std::memory_order_relaxed)),
            nullptr);
    }

    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void StardustProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    if (xmlState != nullptr && xmlState->hasTagName(apvts.state.getType()))
    {
        auto tree = juce::ValueTree::fromXml(*xmlState);
        int savedIndex = tree.getProperty("presetIndex", 0);
        juce::String savedName = tree.getProperty("presetName", "");

        apvts.replaceState(tree);
        sanitizeChainSlotParameters(apvts);
        sanitizeGateParameters(apvts);

        resetGatePatternsToDefaults();
        for (int slot = 0; slot < kGatePatternSlots; ++slot)
        {
            const auto onKey = "gatePatternOn" + juce::String(slot);
            const auto tieKey = "gatePatternTie" + juce::String(slot);
            const auto defaultMasks = defaultGateEnabledMasks();

            const uint64_t enabledMask = static_cast<uint64_t>(
                static_cast<juce::uint64>(static_cast<juce::int64>(tree.getProperty(onKey, static_cast<juce::int64>(defaultMasks[static_cast<size_t>(slot)])))));
            const uint64_t tieMask = static_cast<uint64_t>(
                static_cast<juce::uint64>(static_cast<juce::int64>(tree.getProperty(tieKey, static_cast<juce::int64>(0)))));

            gatePatternEnabledMasks[static_cast<size_t>(slot)].store(enabledMask & kGateStepMask, std::memory_order_relaxed);
            gatePatternTieMasks[static_cast<size_t>(slot)].store(tieMask & enabledMask & kGateStepMask, std::memory_order_relaxed);
        }

        refreshPresets();
        {
            PresetLockGuard g(presetLock);
            bool found = false;
            if (savedName.isNotEmpty())
            {
                for (int i = 0; i < static_cast<int>(allPresets.size()); ++i)
                {
                    if (allPresets[static_cast<size_t>(i)].name == savedName)
                    {
                        currentPresetIndex.store(i);
                        found = true;
                        break;
                    }
                }
            }
            if (!found)
            {
                currentPresetIndex.store(juce::jlimit(0, juce::jmax(0, static_cast<int>(allPresets.size()) - 1), savedIndex));
            }

            // Snapshot current param values for dirty detection
            const int idx = currentPresetIndex.load();
            loadedPresetNormValues.clear();
            if (idx >= 0 && idx < static_cast<int>(allPresets.size()))
            {
                for (const auto& [paramId, val] : allPresets[static_cast<size_t>(idx)].values)
                {
                    if (auto* param = apvts.getParameter(paramId))
                        loadedPresetNormValues[paramId] = param->getValue();
                }
            }
            presetLoadGrace.store(10);
        }
    }
}

juce::AudioProcessorEditor* StardustProcessor::createEditor()
{
    return new StardustEditor(*this);
}

void StardustProcessor::initFactoryPresets()
{
    factoryPresets = getAllFactoryPresets();
}


juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new StardustProcessor();
}
