#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <memory>
#include "DattorroReverb.h"
#include "InputEnvelopeFollower.h"
#include "GrainEnvelopeLUT.h"
#include "Oscillator.h"

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
    void setReverse(float prob);
    void setVoices(int count);
    void setVoiceInterval(float semitones);
    void setPitchQuant(int mode); // 0=Off, 1=Chromatic, 2=Major, 3=Minor, 4=Pentatonic
    void setSVFCutoff(float hz);     // 1000–20000Hz, default 16000
    void setSVFResonance(float q);   // 0.3–2.0, default 0.5
    void setReverbSize(float s)    { dattorro.setSize(s); }
    void setReverbPreDelay(float ms) { dattorro.setPreDelay(ms); }
    void setPitchSpread(float spread); // [0,1] → per-grain random pitch ±12st
    void setLoopback(float amount);    // [0,1] → grain output fed back into input buffer
    void setTempoSync(int mode, double bpm, double ppq)
    {
        syncMode = mode;
        hostBpm.store(bpm, std::memory_order_relaxed);
        hostPpqPosition.store(ppq, std::memory_order_relaxed);
    }
    void process(juce::AudioBuffer<float>& buffer);

    static constexpr int kMaxGrains = 64;

    // Visual feedback: normalized buffer positions + grain phase for each active grain
    struct GrainDisplayInfo { float normPos = 0.0f; float phase = 0.0f; bool active = false; };
    void getGrainDisplayInfo(std::array<GrainDisplayInfo, kMaxGrains>& out) const;

private:
    struct Grain
    {
        float currentPosition = 0.0f;
        float playbackRate = 1.0f;
        float pitchDriftRate = 0.0f;
        float panL = 0.707f, panR = 0.707f;
        float gainMul = 1.0f;
        bool reverse = false;

        // AA biquad (cascaded: 2 stages for 24dB/oct Butterworth when rate > 1.5x)
        float aaS1[2] = {}, aaS2[2] = {};
        float aaB0 = 1.0f, aaB1 = 0.0f, aaB2 = 0.0f, aaA1 = 0.0f, aaA2 = 0.0f;
        float aa2S1[2] = {}, aa2S2[2] = {};
        float aa2B0 = 1.0f, aa2B1 = 0.0f, aa2B2 = 0.0f, aa2A1 = 0.0f, aa2A2 = 0.0f;
        bool aaActive = false;
        bool aa2Active = false;

        // SVF with coefficient caching
        float svfIc1[2] = {}, svfIc2[2] = {};
        float svfBaseFreq = 16000.0f, svfRange = 2000.0f, svfQ = 0.5f;
        float svfCachedG0 = 0.0f, svfCachedK = 1.0f;
        int svfUpdateCounter = 0;
        bool svfActive = true;

        // Stereo decorrelation allpass
        float decorrelState = 0.0f, decorrelCoeff = 0.0f;

        // Per-grain freeze jitter phase offset (decorrelates grains in freeze mode)
        float freezeJitterOffset = 0.0f;

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
    float readSinc(int channel, float position) const;
    float readSincReversed(int channel, float position) const; // B12: time-reversed kernel for reverse grains
    bool isBufferSilent(float position, int lengthSamples) const;
    float findNearestZeroCrossing(int startIdx, int searchRange) const;
    static float fastNoise(uint32_t& state);
    static float fastTanh(float x);

    // Windowed sinc LUT (32-tap Kaiser, pre-computed at prepare)
    static constexpr int kSincTableSize = 512;
    static constexpr int kSincTaps = 32;
    static constexpr int kSincHalfTaps = kSincTaps / 2;
    std::unique_ptr<std::array<float, kSincTableSize * kSincTaps>> sincLUT;

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
    float lastDattorroTexture = -1.0f;
    bool frozen = false;
    int currentPitchQuant = 0; // 0=Off, 1=Chr, 2=Maj, 3=Min, 4=Pent

    // Freeze crossfade (10ms smooth transition)
    float freezeFade = 0.0f;
    float freezeFadeInc = 0.0f;
    std::atomic<bool> freezeTarget { false };

    // Buffer (power-of-2 for bitmask indexing — ~6s at 44.1kHz, covers longer freeze/scatter ranges)
    static constexpr int kInputBufferSize = 262144; // 2^18
    static constexpr int kInputBufferMask = kInputBufferSize - 1;
    static constexpr int kMaxChannels = 2;

    std::unique_ptr<std::array<std::array<float, kInputBufferSize>, kMaxChannels>> inputBuffer;
    int writePos = 0, frozenWritePos = 0;
    std::array<Grain, kMaxGrains> grains {};

    // Active grain index array — avoids scanning all 256 slots per sample
    std::array<int, kMaxGrains> activeIndices {};
    int activeCount = 0;
    bool activeListDirty = false;
    void rebuildActiveList();

    float samplesSinceLastGrain = 0.0f, nextGrainInterval = 0.0f;
    int warmupSamples = 0;
    GrainShape shape = GrainShape::Hanning;

    // Tempo sync (0=off, 1=1/4, 2=1/8, 3=1/16, 4=1/32)
    int syncMode = 0;
    std::atomic<double> hostBpm { 120.0 };
    std::atomic<double> hostPpqPosition { 0.0 };
    double lastGrainPpq = 0.0;

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

    // Internal modulation LFOs (shared IncrementalOscillator — zero trig per sample)
    IncrementalOscillator posLfo, sizeLfo, pitchLfo;
    int lfoNormCounter = 0;

    // Freeze micro-variation (noise-shaped drift, not sinusoidal)
    float freezePosNoise = 0.0f;    // LP-filtered position drift
    float freezePitchNoise = 0.0f;  // LP-filtered pitch drift
    float freezeNoiseAlpha = 0.0f;  // LP coefficient (~0.5Hz bandwidth)
    uint32_t freezeNoiseState = 31337;

    // Dattorro plate reverb (replaces FDN)
    DattorroReverb dattorro;

    // Input envelope follower
    InputEnvelopeFollower inputFollower;

    uint32_t noiseState = 42;
    uint32_t schedulerNoiseState = 7919; // separate state for grain scheduling (decorrelated from grain params)
    uint32_t spatialNoiseState = 54773;  // separate PRNG for pan/stereo/decorrelation

    float reverseProb = 0.0f;
    int numVoices = 1;
    float voiceIntervalSemitones = 7.0f;

    float pitchSpread = 0.0f;   // [0,1]: per-grain pitch randomization ±12 semitones
    float loopbackAmount = 0.0f; // [0,1]: fraction of grain output fed back into input buffer

    // SVF cutoff/resonance (user-controllable, applied at grain spawn)
    float svfCutoffHz = 16000.0f;
    float svfResonance = 0.5f;
};
