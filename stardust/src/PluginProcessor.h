#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "dsp/Saturation.h"
#include "dsp/BitCrusher.h"
#include "dsp/GranularEngine.h"
#include "dsp/ButterworthFilter.h"
#include "dsp/ChorusEngine.h"

struct Preset
{
    juce::String name;
    std::map<juce::String, float> values;
};

class StarDustProcessor : public juce::AudioProcessor
{
public:
    StarDustProcessor();
    ~StarDustProcessor() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return static_cast<int>(factoryPresets.size()); }
    int getCurrentProgram() override { return currentPresetIndex; }
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int /*index*/, const juce::String& /*newName*/) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

    // For level metering (read from GUI thread)
    std::atomic<float> inputLevelLeft { 0.0f };
    std::atomic<float> inputLevelRight { 0.0f };
    std::atomic<float> outputLevelLeft { 0.0f };
    std::atomic<float> outputLevelRight { 0.0f };

    const std::vector<Preset>& getFactoryPresets() const { return factoryPresets; }
    void loadPreset(int index);

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    void initFactoryPresets();

    Saturation saturation;
    BitCrusher bitCrusher;
    GranularEngine granularEngine;
    ButterworthFilter butterworthFilter;
    ChorusEngine chorusEngine;

    double currentSampleRate = 44100.0;
    std::vector<Preset> factoryPresets;
    int currentPresetIndex = 0;

    // Pre-allocated buffer for dry/wet mix (avoids audio-thread allocation)
    juce::AudioBuffer<float> dryBuffer;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StarDustProcessor)
};
