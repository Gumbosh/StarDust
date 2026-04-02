#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <cmath>

// 4-voice unison thickener modeled after Acon Digital Multiply.
// Pure FM/AM — no allpass chains, no long delays, no EQ.
// FM depth ~1ms @ 1Hz ≈ ±10 cents detuning; AM 50% shimmer.
class MultiplyEngine
{
public:
    MultiplyEngine() = default;

    void prepare(double sampleRate, int samplesPerBlock);

    void setMix(float mix);
    void setSpeed(float speed);
    void setPans(float outer, float inner); // 1+2 and 3+4 spread (0=centre, 1=full)

    void process(juce::AudioBuffer<float>& buffer);

private:
    static constexpr int kNumVoices       = 4;
    static constexpr int kDelayBufferSize = 4096;
    static constexpr int kMaxChannels     = 2;

    double sampleRate = 44100.0;

    juce::SmoothedValue<float> mixSmoothed   { 0.5f };
    juce::SmoothedValue<float> speedSmoothed { 1.0f };
    float currentMix   = 0.5f;
    float currentSpeed = 1.0f;

    std::array<std::array<float, kDelayBufferSize>, kMaxChannels> delayBuffer {};
    int writePos = 0;

    struct Voice
    {
        float baseDelaySamples = 0.0f;
        float pan              = 0.5f;
        float panTarget        = 0.5f;
        float fmLfoPhase       = 0.0f;
        float fmLfoRate        = 1.0f;
        float fmDepthSamples   = 0.0f;
        float amLfoPhase       = 0.0f;
        float amLfoRate        = 1.0f;
        float amDepth          = 0.5f;
    };
    std::array<Voice, kNumVoices> voices {};

    float readInterpolated(int channel, float delaySamples) const;
};
