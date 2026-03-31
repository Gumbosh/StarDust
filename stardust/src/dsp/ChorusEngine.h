#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <cmath>

// Ensemble effect matching Acon Digital Multiply:
// 4 voices with phase-randomizing allpass chains.
// EQ on wet signal (low cut ~50Hz, high shelf rolloff ~5kHz).
// Subtle FM for life, AM for shimmer.
class ChorusEngine
{
public:
    ChorusEngine() = default;

    void prepare(double sampleRate, int samplesPerBlock);
    void setMix(float mix);
    void setSpeed(float speedMultiplier);
    void setPans(float outer, float inner);
    void setDepth(float d);    // [0,1] → 0.01ms–1.5ms FM depth per voice
    void setTone(float t);     // [0,1] → LP cutoff 3kHz–20kHz
    void setFeedback(float fb); // [-0.7, 0.7] resonant feedback path
    void setLfoShape(int shape); // 0=Sine, 1=Tri, 2=Random S&H
    void setTempoSync(bool enabled, double bpm); // tempo-lock LFO rate
    void setShimmer(float s); // [0,1] → amDepth per voice
    void setVintageMode(int mode); // 0=Custom, 1=Juno-60, 2=Dim-D, 3=Tri-Chorus, 4=Flanger
    void setDrift(float amount); // [0,1] → per-voice LFO rate drift range (0=0%, 1=10%)
    void process(juce::AudioBuffer<float>& buffer);

private:
    double sampleRate = 44100.0;

    juce::SmoothedValue<float> mixSmoothed { 0.0f };
    juce::SmoothedValue<float> speedSmoothed { 1.0f };
    juce::SmoothedValue<float> feedbackSmoothed { 0.0f };
    float currentMix = 0.0f;
    float currentSpeed = 1.0f;
    float currentFeedback = 0.0f;
    int currentLfoShape = 0; // 0=Sine, 1=Tri, 2=Random S&H
    bool tempoSyncEnabled = false;
    float tempoSyncedRate = 1.0f;

    static constexpr int kNumVoices = 6;
    static constexpr int kMaxChannels = 2;

    float feedbackState[kMaxChannels] = {};
    static constexpr int kDelayBufferSize = 8192;
    static constexpr int kAllpassStages = 8;
    int currentAllpassStages = kAllpassStages; // C3: vintage mode overrides (Juno=6, DimD=4, TriCh=6)

    struct AllpassStage
    {
        float coeff = 0.5f;
        float bufferL = 0.0f;
        float bufferR = 0.0f;
    };

    struct Voice
    {
        float baseDelaySamples = 0.0f;
        float pan = 0.5f;
        float panTarget = 0.5f;

        // FM LFO (subtle — just prevents static comb filtering)
        float fmLfoPhase = 0.0f;
        float fmLfoRate = 0.0f;
        float fmLfoRateBase = 0.0f;   // nominal rate (set at prepare/vintage mode)
        float fmLfoRateTarget = 0.0f; // drifting target (±3% random walk)
        float fmDepthSamples = 0.0f;
        int lfoRateDriftCounter = 0;  // counter for 4096-sample drift updates

        // AM LFO
        float amLfoPhase = 0.0f;
        float amLfoRate = 0.0f;
        float amDepth = 0.0f;

        // Random S&H state for LFO shape 2
        float shHoldValue = 0.0f;
        float shPrevPhase = 0.0f;

        // Phase-randomizing allpass chain
        std::array<AllpassStage, kAllpassStages> allpass {};

        // Slowly-drifting allpass targets (time-variant phase diffusion)
        std::array<float, kAllpassStages> allpassTarget {};
        float driftPhase = 0.0f;          // per-voice drift oscillator phase
        float driftRate  = 0.0f;          // Hz, set at prepare(), varies 0.03–0.09 Hz per voice

        // BBD bucket-capacitor 1-pole LP state (per channel L/R)
        float bbdLP[2] = {};

        // Per-voice LCG PRNG state for BBD clock jitter
        uint32_t jitterPrng = 12345u;

        // BBD clock-alias sideband phasor (phase of alias tone in audio band)
        float bbdClockPhase = 0.0f;

        // 4× oversampling state for tanh saturation stage (per channel)
        float osHistory[2] = {};       // 1-sample history for linear upsampling (4 sub-points)
    };

    std::array<Voice, kNumVoices> voices {};
    std::array<std::array<float, kDelayBufferSize>, kMaxChannels> delayBuffer {};
    int writePos = 0;

    // EQ on wet signal: 1-pole high-pass (low cut) + 1-pole low-pass (high shelf)
    struct BiquadState
    {
        float z1 = 0.0f;
        float z2 = 0.0f;
    };

    // High-pass filter coefficients (low cut ~50Hz)
    float hpB0 = 1.0f, hpB1 = 0.0f, hpB2 = 0.0f, hpA1 = 0.0f, hpA2 = 0.0f;
    std::array<BiquadState, kMaxChannels> hpState {};

    // Low-pass filter coefficients (high shelf rolloff ~5kHz)
    float lpB0 = 1.0f, lpB1 = 0.0f, lpB2 = 0.0f, lpA1 = 0.0f, lpA2 = 0.0f;
    std::array<BiquadState, kMaxChannels> lpState {};

    float bbdLPCoeff = 0.0f; // 1-pole LP coefficient for BBD capacitor rolloff (~8kHz)
    float driftAmount = 0.03f; // per-voice LFO rate drift range (fraction, e.g. 0.03 = ±3%)

    // Input envelope follower for dynamic LFO depth scaling
    float inputEnvelope  = 0.0f;
    float envAttackAlpha  = 0.0f;  // 5ms attack
    float envReleaseAlpha = 0.0f;  // 100ms release

    float readInterpolated(int channel, float delaySamples) const;
    static float processBiquad(float input, float b0, float b1, float b2,
                               float a1, float a2, BiquadState& state);

    // 256-entry sin LUT + linear interpolation (PERF-3)
    static constexpr int kSinLUTSize = 256;
    float lfoSinLUT[kSinLUTSize] = {};
    // Returns sin(phase) using LUT + lerp. Phase must be in [0, 2π).
    inline float lfoSin(float phase) const noexcept
    {
        const float norm = phase * (static_cast<float>(kSinLUTSize) / juce::MathConstants<float>::twoPi);
        const int i0 = static_cast<int>(norm) & (kSinLUTSize - 1);
        const int i1 = (i0 + 1) & (kSinLUTSize - 1);
        const float frac = norm - std::floor(norm);
        return lfoSinLUT[i0] + frac * (lfoSinLUT[i1] - lfoSinLUT[i0]);
    }
};
