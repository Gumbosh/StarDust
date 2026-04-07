#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <atomic>
#include <cmath>

// Granular freeze/scatter engine. Captures audio into a circular buffer
// and replays overlapping grains with pitch shift, scatter, reverse, and Hann windowing.
class GranularEngine
{
public:
    GranularEngine() = default;

    void prepare(double sampleRate, int samplesPerBlock);

    void setSize(float sizeMs);
    void setDensity(float density);
    void setPitch(float semitones);
    void setScatter(float scatter);
    void setReverse(float reverseAmount); // 0-1: probability of grain playing backwards
    void setMix(float mix);

    // Tempo sync: 0=off, 1+=musical division index (overrides size knob)
    void setSyncDivision(int division);
    void setHostBpm(double bpm);

    void process(juce::AudioBuffer<float>& buffer);

private:
    static constexpr int kMaxGrains = 48;
    static constexpr int kCaptureSize = 262144; // 2^18, ~2.7s at 96kHz
    static constexpr int kCaptureMask = kCaptureSize - 1;
    static constexpr int kMaxChannels = 2;
    static constexpr int kWindowSize = 4096;

    double sampleRate = 44100.0;

    // Circular capture buffer (always recording)
    std::array<std::array<float, kCaptureSize>, kMaxChannels> captureBuffer {};
    int writePos = 0;

    // Smoothed parameters
    juce::SmoothedValue<float> sizeSmoothed { 50.0f };
    juce::SmoothedValue<float> densitySmoothed { 4.0f };
    juce::SmoothedValue<float> pitchSmoothed { 0.0f };
    juce::SmoothedValue<float> scatterSmoothed { 0.5f };
    juce::SmoothedValue<float> mixSmoothed { 0.5f };

    std::atomic<float> reverseAmount { 0.0f }; // 0-1: probability of reverse grain
    std::atomic<int> syncDivision { 0 };      // 0=off, 1+=division index
    std::atomic<double> hostBpm { 120.0 };
    static_assert(std::atomic<float>::is_always_lock_free, "must be lock-free");
    static_assert(std::atomic<int>::is_always_lock_free, "must be lock-free");

    struct Grain
    {
        bool active = false;
        int startPos = 0;
        float readPos = 0.0f;
        float playbackRate = 1.0f;
        int grainLength = 0; // output duration in samples
        bool reverse = false; // true = play grain backwards
    };
    std::array<Grain, kMaxGrains> grains {};

    // Grain scheduling
    float grainAccumulator = 0.0f;
    int nextGrainIndex = 0;

    // PRNG (LCG, no heap)
    uint32_t rngState = 42u;

    // Precomputed Hann window table
    std::array<float, kWindowSize> hannWindow {};

    float readHermite(int channel, float pos) const;
    float getWindowValue(float phase) const;
    void triggerGrain(float sizeSamples, float scatterAmt, float pitchRatio);
    float nextRandom(); // 0-1 uniform

    // Tempo sync: convert division index to grain size in ms
    float divisionToMs() const;
};
