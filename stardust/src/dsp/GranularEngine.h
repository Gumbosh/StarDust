#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <cmath>

class GranularEngine
{
public:
    GranularEngine() = default;

    void prepare(double sampleRate, int samplesPerBlock);
    void setParameters(float grainSize, float grainDensity, float grainScatter,
                       float grainMix, float grainStereoWidth);
    void setBasePitch(float semitones);
    void process(juce::AudioBuffer<float>& buffer);

private:
    struct Grain
    {
        float startPosition = 0.0f;
        float currentPosition = 0.0f;
        float playbackRate = 1.0f;
        float pan = 0.5f;
        int lengthInSamples = 0;
        int currentSample = 0;
        bool active = false;
    };

    float readFromBuffer(int channel, float position) const;

    double sampleRate = 44100.0;

    // Parameters (smoothed)
    juce::SmoothedValue<float> grainSizeSmoothed { 30.0f };
    juce::SmoothedValue<float> grainDensitySmoothed { 4.0f };
    juce::SmoothedValue<float> grainScatterSmoothed { 0.2f };
    juce::SmoothedValue<float> grainMixSmoothed { 0.5f };
    juce::SmoothedValue<float> grainStereoWidthSmoothed { 0.0f };
    juce::SmoothedValue<float> basePitchRateSmoothed { 1.0f };

    float currentGrainSize = 30.0f;
    float currentDensity = 4.0f;
    float currentScatter = 0.2f;
    float currentMix = 0.5f;
    float currentStereoWidth = 0.0f;
    float currentBasePitch = 0.0f;

    // Circular input buffer (2 seconds at 96kHz)
    static constexpr int kInputBufferSize = 192000;
    static constexpr int kMaxChannels = 2;
    static constexpr int kMaxGrains = 64;

    std::array<std::array<float, kInputBufferSize>, kMaxChannels> inputBuffer {};
    int writePos = 0;

    // Grain pool
    std::array<Grain, kMaxGrains> grains {};

    // Scheduling
    int samplesSinceLastGrain = 0;
    int nextGrainInterval = 0;
    int warmupSamples = 0;

    // Random number generation
    juce::Random random;
};
