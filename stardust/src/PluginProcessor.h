#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <set>
#include <mutex>
#include "dsp/BitCrusher.h"
#include "dsp/HarmonicExciter.h"

struct Preset
{
    juce::String name;
    std::map<juce::String, float> values;
    bool isFactory = false;
    juce::String bank = {};  // subfolder name, empty = root USER
};

// RAII guard alias for preset mutex
using PresetLockGuard = std::lock_guard<std::recursive_mutex>;

class StardustProcessor : public juce::AudioProcessor
{
public:
    static constexpr int kGatePatternSlots = 1;
    static constexpr int kGatePatternSteps = 64;

    StardustProcessor();
    ~StardustProcessor() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.1; }

    int getNumPrograms() override { PresetLockGuard g(presetLock); return static_cast<int>(allPresets.size()); }
    int getCurrentProgram() override { return currentPresetIndex.load(std::memory_order_relaxed); }
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int /*index*/, const juce::String& /*newName*/) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::UndoManager undoManager;
    juce::AudioProcessorValueTreeState apvts;

    // For level metering (read from GUI thread)
    std::atomic<float> inputLevelLeft { 0.0f };
    std::atomic<float> inputLevelRight { 0.0f };
    std::atomic<float> outputLevelLeft { 0.0f };
    std::atomic<float> outputLevelRight { 0.0f };

    // Callers must hold presetLock when iterating the returned reference
    const std::vector<Preset>& getAllPresets() const { return allPresets; }
    mutable std::recursive_mutex presetLock;
    std::atomic<bool> presetDirty { false };
    std::atomic<bool> presetChangeReset { false }; // signals processBlock to flush DSP state
    // Snapshot of normalized param values after loading a preset (for dirty detection)
    std::map<juce::String, float> loadedPresetNormValues;
    std::atomic<int> presetLoadGrace { 0 }; // frames to skip dirty check after load
    int getPresetCount() const { PresetLockGuard g(presetLock); return static_cast<int>(allPresets.size()); }
    bool isFactoryPreset(int index) const;
    void loadPreset(int index);
    void saveUserPreset(const juce::String& name);
    void saveUserPreset(const juce::String& name, const juce::String& bank);
    void deleteUserPreset(int index);
    void refreshPresets();

    uint64_t getGatePatternEnabledMask(int slot) const;
    uint64_t getGatePatternTieMask(int slot) const;
    void setGatePatternMasks(int slot, uint64_t enabledMask, uint64_t tieMask);
    bool getGatePatternStepEnabled(int slot, int step) const;
    bool getGatePatternStepTied(int slot, int step) const;
    void setGatePatternStepEnabled(int slot, int step, bool enabled);
    void setGatePatternStepTied(int slot, int step, bool tied);
    bool getGatePatternStepEnabled(int step) const { return getGatePatternStepEnabled(0, step); }
    void setGatePatternStepEnabled(int step, bool enabled) { setGatePatternStepEnabled(0, step, enabled); }

    std::vector<juce::String> getUserBanks() const;
    void importBank(const juce::File& sourceFolder);
    void deleteUserBank(const juce::String& bankName);
    void renameUserBank(const juce::String& oldName, const juce::String& newName);

    static juce::File getUserPresetsDir();
    static std::set<juce::String> loadFavorites();
    static void saveFavorites(const std::set<juce::String>& favs);

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    void initFactoryPresets();

    BitCrusher bitCrusher;
    HarmonicExciter exciter; // Harmonic exciter (new)

    double currentSampleRate = 44100.0;
    std::vector<Preset> factoryPresets;
    std::vector<Preset> allPresets; // factory + user
    std::atomic<int> currentPresetIndex { 0 };

    std::array<std::atomic<uint64_t>, kGatePatternSlots> gatePatternEnabledMasks {};
    std::array<std::atomic<uint64_t>, kGatePatternSlots> gatePatternTieMasks {};

    void scanUserPresets();
    void rebuildAllPresets();
    void resetGatePatternsToDefaults();

    // Pre-allocated buffer for dry/wet mix (avoids audio-thread allocation)
    juce::AudioBuffer<float> dryBuffer;
    juce::AudioBuffer<float> masterDryBuffer;

    juce::SmoothedValue<float> msWidthSmoothed { 1.0f };
    juce::SmoothedValue<float> inputGainSmoothed { 1.0f };
    juce::SmoothedValue<float> outputGainSmoothed { 1.0f };
    juce::SmoothedValue<float> characterAmountSmoothed { 0.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StardustProcessor)
};
