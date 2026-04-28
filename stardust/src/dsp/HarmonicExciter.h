#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <array>

// Lightweight Harmonic Exciter - high-band harmonic generation, mixed back in.
class HarmonicExciter
{
public:
    HarmonicExciter() = default;

    void prepare(double sampleRate, int samplesPerBlock);
    void reset();
    void process(juce::AudioBuffer<float>& buffer, float drive, float toneHz, float mix);

private:
    struct ChannelState
    {
        float focusLow = 0.0f;
        float dcX1 = 0.0f;
        float dcY1 = 0.0f;
        float airLow = 0.0f;
    };

    static float onePoleCoefficient(float cutoffHz, double sampleRate);
    ChannelState& stateForChannel(int channel) noexcept;

    double sampleRateHz = 44100.0;
    float lastToneHz = 6000.0f;
    std::array<ChannelState, 2> states {};
    juce::SmoothedValue<float> driveSmoothed { 0.0f };
    juce::SmoothedValue<float> focusCoeffSmoothed { 0.0f };
    juce::SmoothedValue<float> airCoeffSmoothed { 0.0f };
    juce::SmoothedValue<float> mixSmoothed { 0.0f };
};
