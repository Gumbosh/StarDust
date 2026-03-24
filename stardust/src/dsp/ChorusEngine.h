#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <cmath>

// Chorus/ensemble effect matching Acon Digital Multiply defaults:
// 4 voices, 100% stereo spread, 0ms pre-delay,
// FM: 1Hz rate / 20% depth, AM: 1Hz rate / 50% depth
class ChorusEngine
{
public:
    ChorusEngine() = default;

    void prepare(double sampleRate, int samplesPerBlock);
    void setMix(float mix);
    void process(juce::AudioBuffer<float>& buffer);

private:
    double sampleRate = 44100.0;

    juce::SmoothedValue<float> mixSmoothed { 0.0f };
    float currentMix = 0.0f;

    static constexpr int kNumVoices = 4;
    static constexpr int kMaxChannels = 2;
    static constexpr int kDelayBufferSize = 4096;

    struct Voice
    {
        // Frequency modulation LFO (pitch wobble via delay modulation)
        float fmLfoPhase = 0.0f;
        float fmLfoRate = 0.0f;
        float fmDepthSamples = 0.0f;

        // Amplitude modulation LFO (volume wobble)
        float amLfoPhase = 0.0f;
        float amLfoRate = 0.0f;
        float amDepth = 0.0f;

        float baseDelaySamples = 0.0f;
        float pan = 0.5f;
    };

    std::array<Voice, kNumVoices> voices {};
    std::array<std::array<float, kDelayBufferSize>, kMaxChannels> delayBuffer {};
    int writePos = 0;

    float readInterpolated(int channel, float delaySamples) const;
};
