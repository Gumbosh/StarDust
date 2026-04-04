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
    void process(juce::AudioBuffer<float>& buffer);

private:
    double hostSampleRate = 44100.0;

    juce::SmoothedValue<float> bitDepthSmoothed { 12.0f };
    juce::SmoothedValue<float> targetRateSmoothed { 26040.0f };  // SP-950 native rate

    float currentBitDepth = 12.0f;
    float currentTargetRate = 26040.0f;  // SP-950 native rate

    // Per-channel sample-and-hold state
    static constexpr int kMaxChannels = 2;
    float holdCounter[kMaxChannels] = {};
    float holdSample[kMaxChannels] = {};

    // Previous bass-reduced sample for linear interpolation at the S&H sampling instant
    float prevBassReduced[kMaxChannels] = {};

    // Pre-S&H anti-alias: 4-pole Butterworth LP at targetRate/2 (2 cascaded biquads)
    float preAaZ1[kMaxChannels] = {};
    float preAaZ2[kMaxChannels] = {};
    float preAaZ1b[kMaxChannels] = {};  // second biquad stage state
    float preAaZ2b[kMaxChannels] = {};
    float preAaB0 = 1.0f, preAaB1 = 0.0f, preAaB2 = 0.0f, preAaA1 = 0.0f, preAaA2 = 0.0f;     // stage 1: Q=0.5412
    float preAaB0b = 1.0f, preAaB1b = 0.0f, preAaB2b = 0.0f, preAaA1b = 0.0f, preAaA2b = 0.0f; // stage 2: Q=1.3066

    // Post-quantization Nyquist ceiling: 4-pole Butterworth LP tracking targetRate * 0.45
    float nyquistZ1[kMaxChannels] = {};
    float nyquistZ2[kMaxChannels] = {};
    float nyquistZ1b[kMaxChannels] = {};
    float nyquistZ2b[kMaxChannels] = {};
    float nyqB0 = 1.0f, nyqB1 = 0.0f, nyqB2 = 0.0f, nyqA1 = 0.0f, nyqA2 = 0.0f;     // stage 1: Q=0.5412
    float nyqB0b = 1.0f, nyqB1b = 0.0f, nyqB2b = 0.0f, nyqA1b = 0.0f, nyqA2b = 0.0f; // stage 2: Q=1.3066

    // ZOH spectral droop compensation: 1-pole high shelf at targetRate/2, ~+3dB
    float zohShelfB0 = 1.0f, zohShelfB1 = 0.0f, zohShelfA1 = 0.0f;
    float zohShelfZ1[kMaxChannels] = {};

    // Clock jitter — hardware sampler ADC clock model (incremental oscillators, no sin() per sample)
    //   Osc A: 7Hz — power-supply ripple coupling into VCO clock
    //   Osc B: 0.3Hz — slow thermal drift of crystal reference
    //   LP-filtered random walk + 1/f pink noise: correlated phase noise
    float jitterAmount = 0.0f;
    float jitterCosA = 1.0f, jitterSinA = 0.0f;
    float jitterRotCosA = 1.0f, jitterRotSinA = 0.0f;
    float jitterCosB = 1.0f, jitterSinB = 0.0f;
    float jitterRotCosB = 1.0f, jitterRotSinB = 0.0f;
    float jitterWalkLP = 0.0f;
    static constexpr float kJitterWalkAlpha = 0.002f;
    uint32_t jitterState = 12345u;
    uint32_t oscRenormCount = 0;
    // Paul Kellet 1/f pink noise for jitter (D5)
    float jitterPinkState = 0.0f;

    // C3: Per-channel jitter state for stereo decorrelation
    float jitterCosA_ch1 = 1.0f, jitterSinA_ch1 = 0.0f;
    float jitterCosB_ch1 = 1.0f, jitterSinB_ch1 = 0.0f;
    float jitterWalkLP_ch1 = 0.0f;
    uint32_t jitterState_ch1 = 54321u;
    float jitterPinkState_ch1 = 0.0f;

    // C2: Bessel reconstruction filter (4th-order at ~16kHz)
    // Preserves transient shape — models SP-1200 output DAC
    float besselZ1a[kMaxChannels] = {}, besselZ2a[kMaxChannels] = {};
    float besselZ1b[kMaxChannels] = {}, besselZ2b[kMaxChannels] = {};
    float bessB0a = 1, bessB1a = 0, bessB2a = 0, bessA1a = 0, bessA2a = 0;
    float bessB0b = 1, bessB1b = 0, bessB2b = 0, bessA1b = 0, bessA2b = 0;

    // DC blocking filter (1-pole HP at ~5Hz)
    float dcBlockX1[kMaxChannels] = {};
    float dcBlockY1[kMaxChannels] = {};
    float dcBlockCoeff = 0.9993f;

    // Frequency-dependent bit-crushing: matched low-shelf pair around quantization.
    float bassPreB0 = 1.0f, bassPreB1 = 0.0f, bassPreB2 = 0.0f, bassPreA1 = 0.0f, bassPreA2 = 0.0f;
    float bassPostB0 = 1.0f, bassPostB1 = 0.0f, bassPostB2 = 0.0f, bassPostA1 = 0.0f, bassPostA2 = 0.0f;
    float bassPreZ1[kMaxChannels] = {}, bassPreZ2[kMaxChannels] = {};
    float bassPostZ1[kMaxChannels] = {}, bassPostZ2[kMaxChannels] = {};

    // 3rd-order noise shaping state (C1): Wannamaker F-weighted error feedback
    // Pushes 12-15dB more quantization noise above 10kHz vs 1st-order
    float noiseShapeE1[kMaxChannels] = {};  // error history tap 1
    float noiseShapeE2[kMaxChannels] = {};  // error history tap 2
    float noiseShapeE3[kMaxChannels] = {};  // error history tap 3

    // TPDF dither state
    uint32_t ditherState[kMaxChannels] = {};
    // Aperture noise state — separate LCG
    uint32_t apertureState[kMaxChannels] = {};

    // Cached last-computed filter cutoff for per-sample recomputation when modulating (D6)
    float lastPreAaCutoff = -1.0f;
    float lastNyqCutoff = -1.0f;
};
