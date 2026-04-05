#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <cmath>

// Dual-tap crossfade pitch shifter (Eventide H910 technique).
// Two sawtooth-modulated delay taps 180° apart with Hann crossfade.
class PitchShifter
{
public:
    PitchShifter() = default;

    void prepare(double sampleRate, int samplesPerBlock);

    void setPitch(float semitones);
    void setFeedback(float feedback);
    void setTone(float toneHz);
    void setMix(float mix);

    void process(juce::AudioBuffer<float>& buffer);

private:
    static constexpr int kDelaySize = 65536; // 2^16
    static constexpr int kDelayMask = kDelaySize - 1;
    static constexpr int kMaxChannels = 2;
    static constexpr int kWindowSamples = 2048;

    double sampleRate = 44100.0;

    // Delay line
    std::array<std::array<float, kDelaySize>, kMaxChannels> delayBuffer {};
    int writePos = 0;

    // Dual sawtooth phase (0-1)
    float sawPhase = 0.0f;

    // Smoothed parameters
    juce::SmoothedValue<float> pitchSmoothed { 0.0f };
    juce::SmoothedValue<float> feedbackSmoothed { 0.0f };
    juce::SmoothedValue<float> toneSmoothed { 10000.0f };
    juce::SmoothedValue<float> mixSmoothed { 0.5f };

    // Feedback LP filter state (1-pole per channel)
    float lpState[kMaxChannels] = {};

    // Feedback sample (fed back into delay line)
    float feedbackSample[kMaxChannels] = {};

    // Precomputed Hann window (full period)
    std::array<float, kWindowSamples> hannWindow {};

    float readHermite(int channel, float delaySamples) const;
};
