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

    // Pre-S&H anti-alias state (1-pole LP at targetRate/2)
    float preAaState[kMaxChannels] = {};

    // SP-1200 bandwidth ceiling: 1-pole LP at 13kHz
    float nyquistState[kMaxChannels] = {};
    float nyquistAlpha = 1.0f;
};
