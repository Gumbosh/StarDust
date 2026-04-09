#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <cmath>
#include <cstdint>

// Tempo-synced trance-gate sequencer for slot 10.
// Processes input with a step pattern, swing timing, and global ADSR envelope.
class StutterEngine
{
public:
    static constexpr int kPatternSteps = 64;
    static constexpr int kResolutionCount = 6;

    StutterEngine() = default;

    void prepare(double sampleRate, int samplesPerBlock);

    void setBPM(double bpm);
    void setChunks(int chunkCount);
    void setResolution(int resolutionIx);
    void setSwing(float swingNorm);
    void setAttack(float attackNorm);
    void setDecay(float decayNorm);
    void setSustain(float sustainNorm);
    void setRelease(float releaseNorm);
    void setPattern(uint64_t enabledMask, uint64_t tieMask);
    void setMix(float mix);
    void resetSequence();

    void process(juce::AudioBuffer<float>& buffer);

private:
    static constexpr int kMaxChannels = 2;
    static constexpr int kMaxStepSamples = 131072;

    enum class EnvelopeStage
    {
        Idle,
        Attack,
        Decay,
        Sustain,
        Release
    };

    double sampleRate = 44100.0;
    double currentBpm = 120.0;

    int chunks = 16;
    int resolutionIndex = 2; // 1/16
    int currentStep = 0;
    int currentStepDurationSamples = 1;
    double stepSamplesRemaining = 1.0;
    bool sequencePrimed = false;

    uint64_t patternEnabledMask = 0xAAAAAAAAAAAAAAAAull;
    uint64_t patternTieMask = 0ull;

    EnvelopeStage envelopeStage = EnvelopeStage::Idle;
    bool gateOpen = false;
    float envelopeValue = 0.0f;
    float releaseStep = 0.0f;

    juce::SmoothedValue<float> attackSmoothed { 0.05f };
    juce::SmoothedValue<float> decaySmoothed { 0.20f };
    juce::SmoothedValue<float> sustainSmoothed { 1.0f };
    juce::SmoothedValue<float> releaseSmoothed { 0.10f };
    juce::SmoothedValue<float> swingSmoothed { 0.0f };
    juce::SmoothedValue<float> mixSmoothed { 1.0f };

    static constexpr std::array<float, kResolutionCount> kResolutionBeats {
        1.0f / 2.0f,   // 1/8
        1.0f / 3.0f,   // 1/8T
        1.0f / 4.0f,   // 1/16
        1.0f / 8.0f,   // 1/32
        1.0f / 12.0f,  // 1/32T
        1.0f / 16.0f   // 1/64
    };

    static float clampNorm(float value);
    bool isStepEnabled(int step) const;
    bool isStepTied(int step) const;
    int resolveStepDurationSamples(int step, float swingNorm) const;
    void startGateSegment(float attackNorm, float decayNorm, float sustainNorm);
    void endGateSegment(float releaseNorm, int stepSamples);
    void advanceSequenceStep(float attackNorm, float decayNorm, float sustainNorm,
                             float releaseNorm, float swingNorm);
    float renderEnvelopeSample(float attackNorm, float decayNorm,
                               float sustainNorm, float releaseNorm);
};
