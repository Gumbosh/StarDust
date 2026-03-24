#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <cmath>

class BitCrusher
{
public:
    BitCrusher() = default;

    void prepare(double sampleRate, int samplesPerBlock);
    void setBitDepth(float bits);
    void setSampleRate(float targetRate);
    void process(juce::AudioBuffer<float>& buffer);

private:
    double hostSampleRate = 44100.0;

    juce::SmoothedValue<float> bitDepthSmoothed { 12.0f };
    juce::SmoothedValue<float> targetRateSmoothed { 26040.0f };

    float currentBitDepth = 12.0f;
    float currentTargetRate = 26040.0f;

    // Per-channel sample-and-hold state
    static constexpr int kMaxChannels = 2;
    float holdCounter[kMaxChannels] = {};
    float holdSample[kMaxChannels] = {};

    // Anti-aliasing: simple 1-pole low-pass before sample-and-hold
    float aaState[kMaxChannels] = {};
    float aaAlpha = 1.0f;
    float lastTargetRate = 0.0f;
};
