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
    juce::SmoothedValue<float> targetRateSmoothed { 26040.0f };

    float currentBitDepth = 12.0f;
    float currentTargetRate = 26040.0f;

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
    // preAaLastCutoff removed — coefficients recomputed unconditionally each block

    // Post-quantization Nyquist ceiling: 4-pole Butterworth LP tracking targetRate * 0.45
    // (2 cascaded 2-pole stages sharing coefficients, independent state arrays)
    float nyquistZ1[kMaxChannels] = {};
    float nyquistZ2[kMaxChannels] = {};
    float nyquistZ1b[kMaxChannels] = {};
    float nyquistZ2b[kMaxChannels] = {};
    float nyqB0 = 1.0f, nyqB1 = 0.0f, nyqB2 = 0.0f, nyqA1 = 0.0f, nyqA2 = 0.0f;     // stage 1: Q=0.5412
    float nyqB0b = 1.0f, nyqB1b = 0.0f, nyqB2b = 0.0f, nyqA1b = 0.0f, nyqA2b = 0.0f; // stage 2: Q=1.3066
    // nyqLastNormalized removed — coefficients recomputed unconditionally each block

    // Clock jitter — hardware sampler ADC clock model (incremental oscillators, no sin() per sample)
    //   Osc A: 7Hz — power-supply ripple coupling into VCO clock
    //   Osc B: 0.3Hz — slow thermal drift of crystal reference
    //   LP-filtered random walk: correlated phase noise (dominant real component)
    float jitterAmount = 0.0f;
    float jitterCosA = 1.0f, jitterSinA = 0.0f;
    float jitterRotCosA = 1.0f, jitterRotSinA = 0.0f;
    float jitterCosB = 1.0f, jitterSinB = 0.0f;
    float jitterRotCosB = 1.0f, jitterRotSinB = 0.0f;
    float jitterWalkLP = 0.0f;
    static constexpr float kJitterWalkAlpha = 0.002f;
    uint32_t jitterState = 12345u;
    uint32_t oscRenormCount = 0;  // counter for periodic unit-circle renormalization

    // DC blocking filter (1-pole HP at ~5Hz)
    float dcBlockX1[kMaxChannels] = {};
    float dcBlockY1[kMaxChannels] = {};
    float dcBlockCoeff = 0.9993f;

    // Frequency-dependent bit-crushing: matched low-shelf pair around quantization.
    // Pre: -3dB below 200Hz → quantization noise floor in bass elevated relative to signal.
    // Post: +3dB restores signal level; noise stays raised (C10 vintage character).
    float bassPreB0 = 1.0f, bassPreB1 = 0.0f, bassPreB2 = 0.0f, bassPreA1 = 0.0f, bassPreA2 = 0.0f;
    float bassPostB0 = 1.0f, bassPostB1 = 0.0f, bassPostB2 = 0.0f, bassPostA1 = 0.0f, bassPostA2 = 0.0f;
    float bassPreZ1[kMaxChannels] = {}, bassPreZ2[kMaxChannels] = {};
    float bassPostZ1[kMaxChannels] = {}, bassPostZ2[kMaxChannels] = {};

    // TPDF dither state (one random pair per sample, no noise shaping — matches vintage noise character)
    uint32_t ditherState[kMaxChannels] = {};
    // Aperture noise state — separate LCG from ditherState so the S&H timing jitter
    // and the TPDF quantisation dither are drawn from independent sequences.
    uint32_t apertureState[kMaxChannels] = {};
};
