#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <cmath>

// Degrader-style bitcrusher: raw S&H + truncation, no dither, no filtering.
// Intentionally crude — aliasing and quantization noise ARE the character.
// Uses a short crossfade at S&H boundaries to avoid hard clicks.
class BitCrusher
{
public:
    BitCrusher() = default;

    void prepare(double sampleRate, int samplesPerBlock);
    void setBitDepth(float bits);
    void setSampleRate(float targetRate);
    void setJitter(float amount) { jitterAmount = juce::jlimit(0.0f, 1.0f, amount); }
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
    float prevHoldSample[kMaxChannels] = {};

    // Crossfade state: counts down from crossfade length to 0
    float crossfadeCounter[kMaxChannels] = {};
    static constexpr float kCrossfadeSamples = 8.0f; // short crossfade to avoid clicks

    // Clock jitter — simple random walk on S&H timing
    float jitterAmount = 0.0f;
    uint32_t jitterState[kMaxChannels] = {};
};
