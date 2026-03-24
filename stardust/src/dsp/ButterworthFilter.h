#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <cmath>
#include <array>

class ButterworthFilter
{
public:
    ButterworthFilter() = default;

    void prepare(double sampleRate, int samplesPerBlock);
    void setCutoff(float cutoffValue);    // 0-99 range
    void setResonance(float resonance);   // 0.0-1.0
    void setLFO(float rate, float depth);
    void process(juce::AudioBuffer<float>& buffer);

private:
    struct BiquadSection
    {
        float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f;
        float a1 = 0.0f, a2 = 0.0f;
        float x1 = 0.0f, x2 = 0.0f;
        float y1 = 0.0f, y2 = 0.0f;

        float process(float input)
        {
            float output = b0 * input + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;
            x2 = x1; x1 = input;
            y2 = y1; y1 = output;
            return output;
        }

        void reset()
        {
            x1 = x2 = y1 = y2 = 0.0f;
        }
    };

    void recalculateCoefficients(float cutoffVal, float resonanceVal);

    double sampleRate = 44100.0;

    juce::SmoothedValue<float> cutoffSmoothed { 99.0f };
    juce::SmoothedValue<float> resonanceSmoothed { 0.0f };

    float currentCutoff = 99.0f;
    float currentResonance = 0.0f;

    // 6th-order = 3 cascaded biquad sections, per channel
    static constexpr int kNumSections = 3;
    static constexpr int kMaxChannels = 2;

    std::array<std::array<BiquadSection, kNumSections>, kMaxChannels> sections {};

    // Butterworth Q values for 6th-order (pole angles)
    static constexpr float kButterworthQ[kNumSections] = { 0.5176f, 0.7071f, 1.9319f };

    bool needsRecalc = true;
    float lastCalcCutoff = -1.0f;
    float lastCalcResonance = -1.0f;

    // Triangle LFO for cutoff modulation
    float lfoPhase = 0.0f;
    juce::SmoothedValue<float> lfoRateSmoothed { 1.0f };
    juce::SmoothedValue<float> lfoDepthSmoothed { 0.0f };
    float currentLfoRate = 1.0f;
    float currentLfoDepth = 0.0f;
};
