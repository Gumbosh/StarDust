#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <cmath>

// Ensemble effect matching Acon Digital Multiply:
// 4 voices with phase-randomizing allpass chains.
// EQ on wet signal (low cut ~50Hz, high shelf rolloff ~5kHz).
// Subtle FM for life, AM for shimmer.
class ChorusEngine
{
public:
    ChorusEngine() = default;

    void prepare(double sampleRate, int samplesPerBlock);
    void setMix(float mix);
    void setSpeed(float speedMultiplier);
    void setPans(float outer, float inner);
    void process(juce::AudioBuffer<float>& buffer);

private:
    double sampleRate = 44100.0;

    juce::SmoothedValue<float> mixSmoothed { 0.0f };
    juce::SmoothedValue<float> speedSmoothed { 1.0f };
    float currentMix = 0.0f;
    float currentSpeed = 1.0f;

    static constexpr int kNumVoices = 4;
    static constexpr int kMaxChannels = 2;
    static constexpr int kDelayBufferSize = 4096;
    static constexpr int kAllpassStages = 8;

    struct AllpassStage
    {
        float coeff = 0.5f;
        float bufferL = 0.0f;
        float bufferR = 0.0f;
    };

    struct Voice
    {
        float baseDelaySamples = 0.0f;
        float pan = 0.5f;
        float panTarget = 0.5f;

        // FM LFO (subtle — just prevents static comb filtering)
        float fmLfoPhase = 0.0f;
        float fmLfoRate = 0.0f;
        float fmDepthSamples = 0.0f;

        // AM LFO
        float amLfoPhase = 0.0f;
        float amLfoRate = 0.0f;
        float amDepth = 0.0f;

        // Phase-randomizing allpass chain
        std::array<AllpassStage, kAllpassStages> allpass {};
    };

    std::array<Voice, kNumVoices> voices {};
    std::array<std::array<float, kDelayBufferSize>, kMaxChannels> delayBuffer {};
    int writePos = 0;

    // EQ on wet signal: 1-pole high-pass (low cut) + 1-pole low-pass (high shelf)
    struct BiquadState
    {
        float z1 = 0.0f;
        float z2 = 0.0f;
    };

    // High-pass filter coefficients (low cut ~50Hz)
    float hpB0 = 1.0f, hpB1 = 0.0f, hpB2 = 0.0f, hpA1 = 0.0f, hpA2 = 0.0f;
    std::array<BiquadState, kMaxChannels> hpState {};

    // Low-pass filter coefficients (high shelf rolloff ~5kHz)
    float lpB0 = 1.0f, lpB1 = 0.0f, lpB2 = 0.0f, lpA1 = 0.0f, lpA2 = 0.0f;
    std::array<BiquadState, kMaxChannels> lpState {};

    float readInterpolated(int channel, float delaySamples) const;
    static float processBiquad(float input, float b0, float b1, float b2,
                               float a1, float a2, BiquadState& state);
};
