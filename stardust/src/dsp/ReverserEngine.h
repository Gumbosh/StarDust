#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <cmath>

// Tempo-synced audio reverser (kHs Reverser style).
// Repeat count (1-64) × division (1/64..1/4) sets the total reversed chunk length.
// Double-buffered: one buffer captures while the other plays reversed.
class ReverserEngine
{
public:
    ReverserEngine() = default;

    void prepare(double sampleRate, int samplesPerBlock);

    void setRepeatCount(int count);       // 1-64
    void setDivision(int divisionIndex);  // 0=1/64 .. 7=1/4
    void setCrossfade(float crossfade);   // 0-1
    void setMix(float mix);              // 0-1
    void setBPM(double bpm);

    void process(juce::AudioBuffer<float>& buffer);

    static constexpr int kNumDivisions = 8;

private:
    static constexpr int kBufferSize = 524288; // 2^19, handles slow tempos + high repeat counts
    static constexpr int kBufferMask = kBufferSize - 1;
    static constexpr int kMaxChannels = 2;

    double sampleRate = 44100.0;
    double currentBPM = 120.0;

    // Division in beats (quarter note fractions)
    static constexpr float kDivisionBeats[kNumDivisions] = {
        1.0f / 16.0f,   // 1/64
        1.0f / 8.0f,    // 1/32
        1.0f / 6.0f,    // 1/16T (triplet)
        1.0f / 4.0f,    // 1/16
        1.0f / 3.0f,    // 1/8T (triplet)
        1.0f / 2.0f,    // 1/8
        2.0f / 3.0f,    // 1/4T (triplet)
        1.0f            // 1/4
    };

    int repeatCount = 4;    // 1-64, like kHs "4/16" = 4 repeats of 1/16
    int divisionIndex = 3;  // default 1/16

    // Double buffer: A captures while B plays reversed, then swap
    std::array<std::array<float, kBufferSize>, kMaxChannels> bufferA {};
    std::array<std::array<float, kBufferSize>, kMaxChannels> bufferB {};
    bool aIsCapturing = true;

    int capturePos = 0;
    int playbackPos = 0;
    int chunkSamples = 0; // repeatCount × divisionSamples

    juce::SmoothedValue<float> crossfadeSmoothed { 0.15f };
    juce::SmoothedValue<float> mixSmoothed { 0.5f };

    int computeChunkSamples() const;
};
