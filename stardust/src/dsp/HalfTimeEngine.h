#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <atomic>
#include <cmath>

// Tempo-synced half-speed effect (CableGuys HalfTime / Cymatics Dejavu style).
// Double-buffered: one buffer captures while the other plays back at reduced speed.
// Hermite interpolation for sub-sample accuracy at fractional playback rates.
class HalfTimeEngine
{
public:
    HalfTimeEngine() = default;

    void prepare(double sampleRate, int samplesPerBlock);

    void setDivision(int divisionIndex);  // 0..7
    void setSpeed(int speedIndex);        // 0=half (0.5x), 1=quarter (0.25x)
    void setFade(float fade);             // 0-1, crossfade proportion
    void setMix(float mix);               // 0-1
    void setBPM(double bpm);

    void process(juce::AudioBuffer<float>& buffer);

    static constexpr int kNumDivisions = 8;

private:
    static constexpr int kBufferSize = 1048576; // 2^20, handles 4 Bars at 60 BPM @ 192kHz
    static_assert((kBufferSize & (kBufferSize - 1)) == 0, "kBufferSize must be a power of 2");
    static constexpr int kBufferMask = kBufferSize - 1;
    static constexpr int kMaxChannels = 2;

    double sampleRate = 44100.0;
    double currentBPM = 120.0;

    // Division in beats (quarter note fractions)
    static constexpr float kDivisionBeats[kNumDivisions] = {
        1.0f / 4.0f,    // 1/16
        1.0f / 3.0f,    // 1/8T (triplet)
        1.0f / 2.0f,    // 1/8
        1.0f,            // 1/4
        2.0f,            // 1/2
        4.0f,            // 1 Bar
        8.0f,            // 2 Bars
        16.0f            // 4 Bars
    };

    static constexpr float kSpeedMultipliers[2] = { 0.5f, 0.25f };

    std::atomic<int> divisionIndex { 3 };  // default 1/4
    std::atomic<int> speedIndex { 0 };     // default half speed

    // Double buffer: A captures while B plays back at reduced speed, then swap
    std::array<std::array<float, kBufferSize>, kMaxChannels> bufferA {};
    std::array<std::array<float, kBufferSize>, kMaxChannels> bufferB {};
    bool aIsCapturing = true;

    int capturePos = 0;
    double playbackPos = 0.0;             // fractional for sub-sample interpolation
    int chunkSamples = 0;                 // recomputed each process() call
    int latchedChunkSamples = 0;          // only updated at swap boundaries

    juce::SmoothedValue<float> fadeSmoothed { 0.15f };
    juce::SmoothedValue<float> mixSmoothed { 0.5f };

    int computeChunkSamples() const;

    float readHermite(const std::array<float, kBufferSize>& buf, double pos) const;
};
