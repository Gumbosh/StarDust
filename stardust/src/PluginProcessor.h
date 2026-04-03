#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <set>
#include <mutex>
#include "dsp/BitCrusher.h"
#include "dsp/DestroyDrive.h"
#include "dsp/ChorusEngine.h"
#include "dsp/MultiplyEngine.h"
#include "dsp/TapeEngine.h"
#include "dsp/ModulationMatrix.h"
#include "dsp/Saturation.h"
#include "dsp/DattorroReverb.h"

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
    double getTailLengthSeconds() const override { return 0.1; } // chorus tail

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
    // Snapshot of normalized param values after loading a preset (for dirty detection)
    std::map<juce::String, float> loadedPresetNormValues;
    std::atomic<int> presetLoadGrace { 0 }; // frames to skip dirty check after load
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

    BitCrusher bitCrusher;
    DestroyDrive destroyDrive;
    ChorusEngine chorusEngine;
    MultiplyEngine multiplyEngine;
    TapeEngine tapeEngine;
    Saturation saturation;
    DattorroReverb standaloneReverb;

    double currentSampleRate = 44100.0;
    std::vector<Preset> factoryPresets;
    std::vector<Preset> allPresets; // factory + user
    std::atomic<int> currentPresetIndex { 0 };

    void scanUserPresets();
    void rebuildAllPresets();

    // 2x oversampling for destroy section
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampling;

    // Pre-allocated buffer for dry/wet mix (avoids audio-thread allocation)
    juce::AudioBuffer<float> dryBuffer;
    juce::AudioBuffer<float> masterDryBuffer;

    bool lastTapeOn = false;
    juce::SmoothedValue<float> msWidthSmoothed { 1.0f };
    juce::SmoothedValue<float> inputGainSmoothed { 1.0f };
    juce::SmoothedValue<float> outputGainSmoothed { 1.0f };
    ModulationMatrix modMatrix;
    juce::Random random;

    // Cached AudioParameterChoice* pointers — set once in prepareToPlay, used every block
    juce::AudioParameterChoice* tapeNoiseSpeedParam  = nullptr;
    juce::AudioParameterChoice* tapeFormulationParam = nullptr;

    // Shared pink-noise filter state (used by HAZE slot)
    float noisePinkB[5][2] = {}; // Voss-McCartney pink noise: 5 stages × 2 channels
    float hazeColorLP[2] = {};   // 1-pole LP state for HAZE color filter

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StardustProcessor)
};
