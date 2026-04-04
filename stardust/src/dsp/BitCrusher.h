#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <cmath>

// Degrader-style bitcrusher: raw S&H + truncation, no dither, no oversampling.
// Intentionally crude — aliasing and quantization noise ARE the character.
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

    // Clock jitter — simple random walk on S&H timing
    float jitterAmount = 0.0f;
    uint32_t jitterState[kMaxChannels] = {};

    // Post-crush LP filter: 2nd-order Butterworth at targetRate * 0.45
    // Tames the harshest aliasing edges (like Degrader's post-filter)
    float postZ1[kMaxChannels] = {}, postZ2[kMaxChannels] = {};
    float postB0 = 1.0f, postB1 = 0.0f, postB2 = 0.0f, postA1 = 0.0f, postA2 = 0.0f;
};
