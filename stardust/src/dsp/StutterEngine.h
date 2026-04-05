#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <cmath>

// Buffer-repeat glitch engine. Captures a slice of audio and loops it
// with per-repeat decay and crossfaded loop points.
class StutterEngine
{
public:
    StutterEngine() = default;

    void prepare(double sampleRate, int samplesPerBlock);

    void setRate(float rateMs);
    void setDecay(float decay);
    void setDepth(float depth);
    void setMix(float mix);

    void process(juce::AudioBuffer<float>& buffer);

private:
    static constexpr int kBufferSize = 131072; // 2^17, ~1.4s at 96kHz
    static constexpr int kBufferMask = kBufferSize - 1;
    static constexpr int kMaxChannels = 2;
    static constexpr int kCrossfadeLen = 64;

    double sampleRate = 44100.0;

    // Capture buffer
    std::array<std::array<float, kBufferSize>, kMaxChannels> captureBuffer {};
    int writePos = 0;

    // Playback state
    int sliceLengthSamples = 4410;
    int latchedSliceLen = 4410;  // latched at loop boundary
    int readPos = 0;
    float currentDecayGain = 1.0f;
    int capturePos = 0;

    // Smoothed parameters
    juce::SmoothedValue<float> rateSmoothed { 125.0f };
    juce::SmoothedValue<float> decaySmoothed { 0.8f };
    juce::SmoothedValue<float> depthSmoothed { 1.0f };
    juce::SmoothedValue<float> mixSmoothed { 1.0f };

    // Precomputed crossfade window (raised cosine half-window)
    std::array<float, kCrossfadeLen> crossfadeWindow {};
};
