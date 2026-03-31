#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <cmath>

class BitCrusher
{
public:
    BitCrusher() = default;

    void prepare(double sampleRate, int samplesPerBlock);
    void setBitDepth(float bits);
    void setSampleRate(float targetRate);
    void setJitter(float amount) { jitterAmount = juce::jlimit(0.0f, 1.0f, amount); }
    void setHardwareProfile(int profile); // 0=Custom, 1=SP-1200, 2=MPC-60, 3=S950, 4=SP-12
    void process(juce::AudioBuffer<float>& buffer);

private:
    double hostSampleRate = 44100.0;

    juce::SmoothedValue<float> bitDepthSmoothed { 12.0f };
    juce::SmoothedValue<float> targetRateSmoothed { 26040.0f };

    float currentBitDepth = 12.0f;
    float currentTargetRate = 26040.0f;

    // Per-channel sample-and-hold state
    static constexpr int kMaxChannels = 2;
    float holdCounter[kMaxChannels] = {};
    float holdSample[kMaxChannels] = {};

    // Catmull-Rom history for Hermite-smoothed S&H intake (3 previous filtered samples)
    // Ordered oldest→newest: [0]=p0, [1]=p1, [2]=p2; current filtered sample is p3
    float upsampleHistory[kMaxChannels][3] = {};

    // Pre-S&H anti-alias: 2-pole Butterworth LP at targetRate/2
    // Biquad state per channel + dynamic coefficients (recomputed when rate changes)
    float preAaZ1[kMaxChannels] = {};
    float preAaZ2[kMaxChannels] = {};
    float preAaB0 = 1.0f, preAaB1 = 0.0f, preAaB2 = 0.0f, preAaA1 = 0.0f, preAaA2 = 0.0f;
    float preAaLastCutoff = -1.0f;  // cached to skip recompute when rate unchanged

    // Post-quantization Nyquist ceiling: 4-pole Butterworth LP tracking targetRate * 0.45
    // (2 cascaded 2-pole stages sharing coefficients, independent state arrays)
    float nyquistZ1[kMaxChannels] = {};
    float nyquistZ2[kMaxChannels] = {};
    float nyquistZ1b[kMaxChannels] = {};
    float nyquistZ2b[kMaxChannels] = {};
    float nyqB0 = 1.0f, nyqB1 = 0.0f, nyqB2 = 0.0f, nyqA1 = 0.0f, nyqA2 = 0.0f;
    float nyqLastTargetRate = -1.0f;  // cache to gate coefficient recomputation

    // Clock jitter — 3-frequency stack (90Hz motor, 200Hz capstan, 0.3Hz reel wow) + noise
    float jitterAmount = 0.0f;
    float jitterPhase = 0.0f;          // 90Hz motor fundamental
    float jitterPhaseCapstan = 0.0f;  // 200Hz capstan harmonic
    float jitterPhaseReel    = 0.0f;  // 0.3Hz reel wow
    uint32_t jitterState = 12345u;
    static constexpr float jitterFreq = 90.0f;

    // Frequency-dependent bit-crushing: matched low-shelf pair around quantization
    // Pre-quantize: -3dB below 200Hz (ADC bass accuracy loss simulation)
    // Post-quantize: +3dB below 200Hz (restore level — quantization noise stays)
    float bassPreB0 = 1.0f, bassPreB1 = 0.0f, bassPreB2 = 0.0f, bassPreA1 = 0.0f, bassPreA2 = 0.0f;
    float bassPostB0 = 1.0f, bassPostB1 = 0.0f, bassPostB2 = 0.0f, bassPostA1 = 0.0f, bassPostA2 = 0.0f;
    float bassPreZ1[kMaxChannels] = {}, bassPreZ2[kMaxChannels] = {};
    float bassPostZ1[kMaxChannels] = {}, bassPostZ2[kMaxChannels] = {};

    // 2nd-order noise-shaped dither with 8× averaging oversampling
    // 8 independent TPDF-dithered quantizations averaged per host sample → ~9dB noise floor reduction
    static constexpr int kQuantOS = 8;
    static constexpr float kQuantOSInv = 1.0f / static_cast<float>(kQuantOS);
    uint32_t ditherState[kMaxChannels] = {};
    float ditherError[kMaxChannels] = {};     // current quantization error
    float ditherErrorPrev[kMaxChannels] = {}; // previous error for 2nd-order shaping
};
