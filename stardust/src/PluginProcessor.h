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
#include "dsp/Oscillator.h"
#include "dsp/GranularEngine.h"
#include "dsp/StutterEngine.h"
#include "dsp/PitchShifter.h"
#include "dsp/ReverserEngine.h"
#include "dsp/HalfTimeEngine.h"

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
    GranularEngine granularEngine;
    StutterEngine stutterEngine;
    PitchShifter pitchShifter;
    ReverserEngine reverserEngine;
    HalfTimeEngine halfTimeEngine;

    double currentSampleRate = 44100.0;
    std::vector<Preset> factoryPresets;
    std::vector<Preset> allPresets; // factory + user
    std::atomic<int> currentPresetIndex { 0 };

    void scanUserPresets();
    void rebuildAllPresets();

    // 2x oversampling for BitCrusher
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampling;

    // Pre-allocated buffer for dry/wet mix (avoids audio-thread allocation)
    juce::AudioBuffer<float> dryBuffer;
    juce::AudioBuffer<float> masterDryBuffer;

    bool lastTapeOn = false;
    bool lastDestroyOn = false;
    juce::SmoothedValue<float> msWidthSmoothed { 1.0f };
    juce::SmoothedValue<float> inputGainSmoothed { 1.0f };
    juce::SmoothedValue<float> outputGainSmoothed { 1.0f };
    ModulationMatrix modMatrix;
    juce::Random random;

    // Cached AudioParameterChoice* pointers — set once in prepareToPlay, used every block
    juce::AudioParameterChoice* tapeNoiseSpeedParam  = nullptr;

    // Cached parameter pointers for audio-thread reads (avoids juce::String
    // heap allocation in processBlock — real-time safety).
    // Set once in prepareToPlay, read every block.
    std::atomic<float>* cachedModLfo1Rate  = nullptr;
    std::atomic<float>* cachedModLfo1Depth = nullptr;
    std::atomic<float>* cachedModLfo1Wave  = nullptr;
    std::atomic<float>* cachedModLfo1Sync  = nullptr;
    std::atomic<float>* cachedModLfo2Rate  = nullptr;
    std::atomic<float>* cachedModLfo2Depth = nullptr;
    std::atomic<float>* cachedModLfo2Wave  = nullptr;
    std::atomic<float>* cachedModLfo2Sync  = nullptr;

    std::atomic<float>* cachedModSlot1Src = nullptr;
    std::atomic<float>* cachedModSlot1Tgt = nullptr;
    std::atomic<float>* cachedModSlot1Amt = nullptr;
    std::atomic<float>* cachedModSlot2Src = nullptr;
    std::atomic<float>* cachedModSlot2Tgt = nullptr;
    std::atomic<float>* cachedModSlot2Amt = nullptr;
    std::atomic<float>* cachedModSlot3Src = nullptr;
    std::atomic<float>* cachedModSlot3Tgt = nullptr;
    std::atomic<float>* cachedModSlot3Amt = nullptr;
    std::atomic<float>* cachedModSlot4Src = nullptr;
    std::atomic<float>* cachedModSlot4Tgt = nullptr;
    std::atomic<float>* cachedModSlot4Amt = nullptr;

    std::atomic<float>* cachedChainSlot[4] = { nullptr, nullptr, nullptr, nullptr };

    // Shared pink-noise filter state (used by HAZE slot)
    float noisePinkB[5][2] = {}; // Voss-McCartney pink noise: 5 stages × 2 channels
    float hazeColorLP[2] = {};   // 1-pole LP state for HAZE color filter

    // H1: Vinyl crackle state
    float crackleDecay[2] = {};
    float crackleImpulse[2] = {};
    // H1: Crackle bandpass state (800Hz-6kHz)
    float crackleBPState[2] = {};

    // H2: Mains hum incremental oscillators (60/120/180/240 Hz)
    IncrementalOscillator humOsc60, humOsc120, humOsc180, humOsc240;

    // H3: Sub-bass rumble state
    float rumbleState[2] = {};
    float rumbleLFOPhase = 0.0f;

    // H4: Signal envelope for noise riding
    float hazeSignalEnv[2] = {};

    // H5: Noise color wow LFO
    float hazeWowPhase = 0.0f;

    // H6: Stereo allpass decorrelation
    float hazeAllpassBuf[512] = {};
    int hazeAllpassWritePos = 0;
    static constexpr int kHazeAllpassDelay = 22;  // ~0.5ms at 44.1kHz

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StardustProcessor)
};
