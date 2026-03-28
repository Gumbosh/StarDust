#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <cmath>
#include <cstdint>
#include <memory>
#include "DattorroReverb.h"
#include "InputEnvelopeFollower.h"
#include "GrainEnvelopeLUT.h"

class GranularEngine
{
public:
    GranularEngine() = default;

    enum class GrainShape { Hanning = 0, Gaussian, Triangle, Trapezoid };

    void prepare(double sampleRate, int samplesPerBlock);
    void setParameters(float grainSize, float grainDensity, float grainScatter,
                       float grainMix, float grainStereoWidth);
    void setBasePitch(float semitones);
    void setPosition(float position);
    void setFreeze(bool freeze);
    void setFeedback(float amount);
    void setTexture(float texture);
    void setSpace(float space);
    void setDrift(float drift);
    void setCloud(float cloud);
    void setGrainShape(GrainShape newShape) { shape = newShape; }
    void process(juce::AudioBuffer<float>& buffer);

private:
    struct Grain
    {
        float currentPosition = 0.0f;
        float playbackRate = 1.0f;
        float pitchDriftRate = 0.0f;
        float panL = 0.707f, panR = 0.707f;
        float gainMul = 1.0f;
        bool reverse = false;

        // AA biquad
        float aaS1[2] = {}, aaS2[2] = {};
        float aaB0 = 1.0f, aaB1 = 0.0f, aaB2 = 0.0f, aaA1 = 0.0f, aaA2 = 0.0f;
        bool aaActive = false;

        // SVF with coefficient caching
        float svfIc1[2] = {}, svfIc2[2] = {};
        float svfBaseFreq = 8000.0f, svfRange = 6000.0f, svfQ = 0.707f;
        float svfCachedG0 = 0.0f, svfCachedK = 1.0f;
        int svfUpdateCounter = 0;
        bool svfActive = true;

        // Stereo decorrelation allpass
        float decorrelState = 0.0f, decorrelCoeff = 0.0f;

        int lengthInSamples = 16;
        int currentSample = 0;
        bool active = false;
    };

    // Extracted methods
    void spawnGrains(float readAnchor, float scatter, float basePitchRate,
                     float pitchMod, int grainSizeSamples, float effectiveDensity,
                     float overlapFactor, float stereoWidth, bool isStereo);
    void processGrain(Grain& g, bool isStereo, float sr, float envFollow,
                      float& outL, float& outR);

    float readHermite(int channel, float position) const;
    bool isBufferSilent(float position, int lengthSamples) const;
    int findNearestZeroCrossing(int startIdx, int searchRange) const;
    static float fastNoise(uint32_t& state);

    double sampleRate = 44100.0;

    // Smoothed parameters
    juce::SmoothedValue<float> grainSizeSmoothed { 30.0f };
    juce::SmoothedValue<float> grainDensitySmoothed { 4.0f };
    juce::SmoothedValue<float> grainScatterSmoothed { 0.2f };
    juce::SmoothedValue<float> grainMixSmoothed { 0.5f };
    juce::SmoothedValue<float> grainStereoWidthSmoothed { 0.0f };
    juce::SmoothedValue<float> basePitchRateSmoothed { 1.0f };
    juce::SmoothedValue<float> positionSmoothed { 1.0f };
    juce::SmoothedValue<float> feedbackSmoothed { 0.0f };
    juce::SmoothedValue<float> textureSmoothed { 0.3f };
    juce::SmoothedValue<float> spaceSmoothed { 0.0f };

    float currentGrainSize = 30.0f, currentDensity = 4.0f;
    float currentScatter = 0.2f, currentMix = 0.5f;
    float currentStereoWidth = 0.0f, currentBasePitch = 0.0f;
    float currentPosition = 1.0f, currentFeedback = 0.0f;
    float currentTexture = 0.3f, currentSpace = 0.0f;
    float currentDriftRaw = 0.0f, currentCloud = 0.3f;
    bool frozen = false;

    // Freeze crossfade (10ms smooth transition)
    float freezeFade = 0.0f;
    float freezeFadeInc = 0.0f;
    bool freezeTarget = false;

    // Buffer
    static constexpr int kInputBufferSize = 768000;
    static constexpr int kMaxChannels = 2;
    static constexpr int kMaxGrains = 256;

    std::unique_ptr<std::array<std::array<float, kInputBufferSize>, kMaxChannels>> inputBuffer;
    int writePos = 0, frozenWritePos = 0;
    std::array<Grain, kMaxGrains> grains {};

    int samplesSinceLastGrain = 0, nextGrainInterval = 0, warmupSamples = 0;
    GrainShape shape = GrainShape::Hanning;

    // Tail handling
    float inputRmsState = 0.0f, rmsAlpha = 0.0f;
    static constexpr float kSilenceThreshold = 0.0001f;
    bool inTailMode = false;
    int tailSamplesRemaining = 0, tailDurationSamples = 0;

    // Feedback
    float feedbackBuffer[kMaxChannels] = {};
    float feedbackLpState[kMaxChannels] = {};
    float feedbackLpAlpha = 0.0f;

    float normSmoothed = 1.0f;

    // Internal modulation LFOs
    float posLfoPhase = 0.0f, posLfoInc = 0.0f;
    float sizeLfoPhase = 0.0f, sizeLfoInc = 0.0f;
    float pitchLfoPhase = 0.0f, pitchLfoInc = 0.0f;

    // Freeze micro-variation
    float freezeJitterPhase = 0.0f, freezeJitterInc = 0.0f;
    float freezePitchJitterPhase = 0.0f, freezePitchJitterInc = 0.0f;

    // Dattorro plate reverb (replaces FDN)
    DattorroReverb dattorro;

    // Input envelope follower
    InputEnvelopeFollower inputFollower;

    uint32_t noiseState = 42;
};
