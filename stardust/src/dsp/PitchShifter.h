#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <cmath>

// Granular dual-tap pitch shifter.
// Two sawtooth-modulated delay taps 180° apart with Hann crossfade.
class PitchShifter
{
public:
    PitchShifter() = default;

    void prepare(double sampleRate, int samplesPerBlock);

    void setShift(float semitones);
    void setJitter(float amount);
    void setGrainSize(float grainSizeMs);
    void setMix(float mix);

    void process(juce::AudioBuffer<float>& buffer);

private:
    static constexpr int kDelaySize = 65536; // 2^16
    static constexpr int kDelayMask = kDelaySize - 1;
    static constexpr int kMaxChannels = 2;
    static constexpr int kWindowTableSize = 2048;

    double sampleRate = 44100.0;

    // Delay line
    std::array<std::array<float, kDelaySize>, kMaxChannels> delayBuffer {};
    int writePos = 0;

    // Dual sawtooth phase (0-1)
    float sawPhase = 0.0f;

    // Smoothed parameters
    juce::SmoothedValue<float> shiftSmoothed { 0.0f };
    juce::SmoothedValue<float> jitterSmoothed { 0.0f };
    juce::SmoothedValue<float> grainSizeSmoothed { 80.0f };
    juce::SmoothedValue<float> mixSmoothed { 0.5f };

    // Smoothed random modulation for pitch jitter.
    juce::Random random;
    float jitterValue = 0.0f;
    float jitterTarget = 0.0f;
    int jitterCountdown = 0;

    // Precomputed Hann window (full period)
    std::array<float, kWindowTableSize> hannWindow {};

    float readHermite(int channel, float delaySamples) const;
    float nextJitterValue(int grainSamples);
};
