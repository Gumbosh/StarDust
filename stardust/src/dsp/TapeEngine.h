#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <cmath>

// Cassette tape emulation (1⅞ ips, Type I normal bias):
// - Wow: slow irregular pitch drift from capstan speed variation
// - Flutter: fast random pitch wobble from motor/belt imperfections
// - Hiss: bandpass-filtered tape noise (2-6kHz emphasis)
// - Tape saturation: soft compression on transients (always-on)
// - Bandwidth limit: gentle LP at 11kHz (cassette ceiling)
class TapeEngine
{
public:
    TapeEngine() = default;

    void prepare(double sampleRate, int samplesPerBlock);
    void setWow(float amount);       // 0.0–1.0
    void setFlutter(float amount);   // 0.0–1.0
    void setHiss(float amount);      // 0.0–1.0
    void process(juce::AudioBuffer<float>& buffer);

private:
    float readInterpolated(int channel, float delaySamples) const;

    double sampleRate = 44100.0;

    juce::SmoothedValue<float> wowSmoothed { 0.0f };
    juce::SmoothedValue<float> flutterSmoothed { 0.0f };
    juce::SmoothedValue<float> hissSmoothed { 0.0f };

    float currentWow = 0.0f;
    float currentFlutter = 0.0f;
    float currentHiss = 0.0f;

    // Delay line for pitch modulation
    static constexpr int kMaxChannels = 2;
    static constexpr int kDelayBufferSize = 8192;
    float delayBuffer[kMaxChannels][kDelayBufferSize] = {};
    int writePos = 0;

    // Wow: sine + filtered noise for irregular drift
    float wowPhase = 0.0f;
    float wowNoise[kMaxChannels] = {};       // filtered random component
    float wowNoiseState[kMaxChannels] = {};  // LP filter state for wow noise

    // Flutter: sine + filtered noise for random wobble
    float flutterPhase = 0.0f;
    float flutterNoise[kMaxChannels] = {};
    float flutterNoiseState[kMaxChannels] = {};

    // Hiss: bandpass filtered noise (2-6kHz cassette character)
    float hissHpState[kMaxChannels] = {};
    float hissLpState[kMaxChannels] = {};

    // Cassette bandwidth limit: 1-pole LP at ~11kHz
    float tapeFilterState[kMaxChannels] = {};
    float tapeFilterAlpha = 1.0f;

    // Tape saturation state (head bump ~100Hz boost)
    float headBumpState[kMaxChannels] = {};
    float headBumpAlpha = 0.0f;

    juce::Random random;
};
