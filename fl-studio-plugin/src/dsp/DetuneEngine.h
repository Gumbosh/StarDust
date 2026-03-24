#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <cmath>
#include <array>

// Simulates the SP-1200 "pitch up vinyl → sample → pitch down" workflow.
// Instead of actual pitch shifting (which introduces latency), this applies
// the equivalent aliasing artifacts by resampling at a modified rate.
// Zero latency, same sonic character.
class DetuneEngine
{
public:
    DetuneEngine() = default;

    void prepare(double sampleRate, int samplesPerBlock);
    void setDetune(float rpmDetune);
    void process(juce::AudioBuffer<float>& buffer);

private:
    double hostSampleRate = 44100.0;

    juce::SmoothedValue<float> detuneSmoothed { 0.0f };
    float currentDetune = 0.0f;

    static constexpr int kMaxChannels = 2;

    // Sample-and-hold state for the resample effect
    float holdCounter[kMaxChannels] = {};
    float holdSample[kMaxChannels] = {};

    // Second pass: inverse resample to fold aliasing differently
    float holdCounter2[kMaxChannels] = {};
    float holdSample2[kMaxChannels] = {};
};
