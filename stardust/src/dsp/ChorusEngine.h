#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <atomic>
#include <cmath>

// Ensemble effect inspired by Acon Digital Multiply:
// 4 voices with delayed allpass diffusion chains, Hermite-interpolated delay reads,
// constant-power panning, incremental LFOs (zero trig per sample),
// EQ on wet signal (HP ~50Hz, LP ~9kHz), voice-normalized summing.
class ChorusEngine
{
public:
    ChorusEngine() = default;

    void prepare(double sampleRate, int samplesPerBlock);
    void setMix(float mix);
    void setSpeed(float speedMultiplier);
    void setPans(float outer, float inner);
    void process(juce::AudioBuffer<float>& buffer);

private:
    double sampleRate = 44100.0;

    juce::SmoothedValue<float> mixSmoothed { 0.0f };
    juce::SmoothedValue<float> speedSmoothed { 1.0f };
    float currentMix = 0.0f;
    float currentSpeed = 1.0f;

    static constexpr int kNumVoices = 4;
    static constexpr int kMaxChannels = 2;
    static constexpr int kDelayBufferSize = 4096;
    static constexpr int kDelayMask = kDelayBufferSize - 1;
    static constexpr int kAllpassStages = 8;
    static constexpr int kMaxAllpassDelay = 256;
    static constexpr int kApMask = kMaxAllpassDelay - 1;
    static constexpr float kVoiceGain = 1.0f / static_cast<float>(kNumVoices);
    static constexpr float kAmDepth = 0.15f;          // subtle AM shimmer (was 0.3)
    static constexpr float kFmDepthMs = 0.2f;         // FM modulation depth in ms
    static constexpr float kHpFreq = 50.0f;           // wet signal HP cutoff
    static constexpr float kLpFreq = 9000.0f;         // wet signal LP cutoff (was 5kHz)
    static constexpr float kSqrt2 = 1.41421356f;

    // Incremental oscillator (same pattern as TapeEngine)
    struct Osc
    {
        float cosVal = 1.0f, sinVal = 0.0f;
        float cosInc = 1.0f, sinInc = 0.0f;

        void init(float freq, float sr)
        {
            const float w = juce::MathConstants<float>::twoPi * freq / sr;
            cosInc = std::cos(w); sinInc = std::sin(w);
            cosVal = 1.0f; sinVal = 0.0f;
        }

        void initWithPhase(float freq, float sr, float phase)
        {
            const float w = juce::MathConstants<float>::twoPi * freq / sr;
            cosInc = std::cos(w); sinInc = std::sin(w);
            cosVal = std::cos(phase); sinVal = std::sin(phase);
        }

        void advance()
        {
            const float c = cosVal * cosInc - sinVal * sinInc;
            const float s = sinVal * cosInc + cosVal * sinInc;
            cosVal = c; sinVal = s;
        }

        void normalize()
        {
            const float mag = cosVal * cosVal + sinVal * sinVal;
            const float correction = (3.0f - mag) * 0.5f;
            cosVal *= correction; sinVal *= correction;
        }

        void setFreq(float freq, float sr)
        {
            const float w = juce::MathConstants<float>::twoPi * freq / sr;
            cosInc = std::cos(w); sinInc = std::sin(w);
        }
    };

    // Delayed allpass stage (true diffusion with multi-sample delay buffer)
    struct AllpassStage
    {
        float coeff = 0.5f;
        int delayLen = 1;
        float bufferL[kMaxAllpassDelay] = {};
        float bufferR[kMaxAllpassDelay] = {};
        int writeIdx = 0;
    };

    struct Voice
    {
        float baseDelaySamples = 0.0f;
        float pan = 0.5f;
        std::atomic<float> panTarget { 0.5f };
        float panGainL = 0.707f;   // cached cos(pan * halfPi)
        float panGainR = 0.707f;   // cached sin(pan * halfPi)

        Osc fmLfo, amLfo;
        float fmBaseRate = 1.0f;
        float amBaseRate = 1.0f;

        std::array<AllpassStage, kAllpassStages> allpass {};
    };

    float panSmoothAlpha = 0.001f;
    int oscNormCounter = 0;

    std::array<Voice, kNumVoices> voices {};
    std::array<std::array<float, kDelayBufferSize>, kMaxChannels> delayBuffer {};
    int writePos = 0;

    // EQ biquad coefficients and state
    struct BiquadState { float z1 = 0.0f, z2 = 0.0f; };
    float hpB0 = 1, hpB1 = 0, hpB2 = 0, hpA1 = 0, hpA2 = 0;
    float lpB0 = 1, lpB1 = 0, lpB2 = 0, lpA1 = 0, lpA2 = 0;
    std::array<BiquadState, kMaxChannels> hpState {}, lpState {};

    float readHermite(int channel, float delaySamples) const;
    static float processBiquad(float input, float b0, float b1, float b2,
                               float a1, float a2, BiquadState& state);
};
