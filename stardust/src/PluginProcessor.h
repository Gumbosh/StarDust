#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <set>
#include <mutex>
#include "dsp/Saturation.h"
#include "dsp/BitCrusher.h"
#include "dsp/DestroyDrive.h"
#include "dsp/GranularEngine.h"
#include "dsp/ButterworthFilter.h"
#include "dsp/ChorusEngine.h"
#include "dsp/TapeEngine.h"

struct Preset
{
    juce::String name;
    std::map<juce::String, float> values;
    bool isFactory = false;
    juce::String bank;  // subfolder name, empty = root USER
};

// RAII guard alias for preset mutex
using PresetLockGuard = std::lock_guard<std::recursive_mutex>;

class StardustProcessor : public juce::AudioProcessor
{
public:
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
    double getTailLengthSeconds() const override { return 0.1; } // chorus/grain tail

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
    std::atomic<bool> loadingPreset { false };
    std::atomic<int> ignoreParamChanges { 0 };
    int getPresetCount() const { PresetLockGuard g(presetLock); return static_cast<int>(allPresets.size()); }
    bool isFactoryPreset(int index) const;
    void loadPreset(int index);
    void saveUserPreset(const juce::String& name);
    void saveUserPreset(const juce::String& name, const juce::String& bank);
    void deleteUserPreset(int index);
    void refreshPresets();

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

    Saturation saturation;
    BitCrusher bitCrusher;
    DestroyDrive destroyDrive;
    GranularEngine granularEngine;
    ButterworthFilter butterworthFilter;
    ChorusEngine chorusEngine;
    TapeEngine tapeEngine;

    double currentSampleRate = 44100.0;
    std::vector<Preset> factoryPresets;
    std::vector<Preset> allPresets; // factory + user
    std::atomic<int> currentPresetIndex { 0 };

    void scanUserPresets();
    void rebuildAllPresets();

    // 2x oversampling for distortion + destroy sections
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampling;

    // Pre-allocated buffer for dry/wet mix (avoids audio-thread allocation)
    juce::AudioBuffer<float> dryBuffer;
    juce::AudioBuffer<float> masterDryBuffer;

    // Tone filter state (1-pole low-pass per channel)
    float toneStateL = 0.0f;
    float toneStateR = 0.0f;

    juce::Random random;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StardustProcessor)
};
