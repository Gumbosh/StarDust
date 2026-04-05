#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <atomic>
#include <cmath>
#include "Oscillator.h"
#include "FastMath.h"

// Roland Juno-60 ensemble chorus emulation.
// Hardware: 2× MN3009 256-stage BBD chips + MN3101 clock driver.
//
// Circuit-accurate signal path:
//   1. Input mono-summed → 4-pole 24dB/oct LP anti-alias filter (~3.4kHz cutoff)
//   2. BBD input stage soft saturation (OTA input characteristic)
//   3. Written to dual mono delay buffers (separate BBD paths for I+II mode)
//   4. L and R tapped at different delays (LFO-modulated):
//        Modes I & II standalone: R = inverted LFO (stereo widening)
//        Combined I+II: Mode I = inverted stereo, Mode II = non-inverted (Leslie swirl)
//   5. BBD bucket discretization (512 stages) + even-harmonic distortion
//   6. Output 1-pole LP ~8kHz per BBD chip (MN3009 capacitor rolloff)
//   7. Clock feedthrough noise (faint 50kHz tone)
//
// Mode I:  0.513 Hz, ±1.85 ms depth, 3.5 ms centre (measured: 1.66–5.35 ms swing)
// Mode II: 0.863 Hz, ±2.5  ms depth, 3.5 ms centre
// I+II:    both simultaneously, -3 dB normalised, Mode II R non-inverted
// LFO: triangle wave (MN3101 integrator output)
class ChorusEngine
{
public:
    ChorusEngine() = default;

    void prepare(double sampleRate, int samplesPerBlock);
    void setMix(float mix);   // 0–1, constant-power crossfade
    void setMode(int mode);   // 0=I, 1=II, 2=I+II

    void process(juce::AudioBuffer<float>& buffer);

private:
    double sampleRate = 44100.0;

    juce::SmoothedValue<float> mixSmoothed { 0.5f };
    float currentMix = 0.5f;
    std::atomic<int> junoMode { 0 };
    static_assert(std::atomic<int>::is_always_lock_free, "must be lock-free");

    // Dual MONO delay buffers — separate BBD paths for I+II mode (CH5)
    static constexpr int kDelayBufferSize = 8192;
    std::array<float, kDelayBufferSize> delayBuffer0 {};  // Mode I BBD
    std::array<float, kDelayBufferSize> delayBuffer1 {};  // Mode II BBD
    int writePos = 0;

    // BBD bucket discretization: MN3009 has 512 discrete charge-transfer stages (CH1)
    static constexpr int kBBDBuckets = 512;

    // 4-pole Butterworth LP pre-filter state (24dB/oct — CH2 upgrade)
    // Stage 1: Q = 0.5412 (4th-order Butterworth pole pair 1)
    float hp1z1 = 0.0f, hp1z2 = 0.0f;
    float preB0 = 1.0f, preB1 = 0.0f, preB2 = 0.0f;
    float preA1 = 0.0f, preA2 = 0.0f;
    // Stage 2: Q = 1.3066 (4th-order Butterworth pole pair 2)
    float hp2z1 = 0.0f, hp2z2 = 0.0f;
    float pre2B0 = 1.0f, pre2B1 = 0.0f, pre2B2 = 0.0f;
    float pre2A1 = 0.0f, pre2A2 = 0.0f;

    static constexpr int kNumModes = 2;

    struct ModeVoice
    {
        float lfoPhase     = 0.0f;
        float lfoRate      = 0.0f;   // Hz
        float depthSamples = 0.0f;
        float baseSamples  = 0.0f;
        float bbdLP[2]     = {};     // output LP state: L=0, R=1
        // Clock jitter: smoothed random walk (LP-filtered white noise) — correlated like real BBD clock
        uint32_t jitterPrng = 12345u;
        float jitterL = 0.0f;
        float jitterR = 0.0f;
    };

    std::array<ModeVoice, kNumModes> modes {};
    float bbdLPCoeff = 0.0f;  // MN3009 output reconstruction LP (~8 kHz)

    // NE570 compander emulation (M1): compress before BBD, expand after
    float compEnvState = 0.0f;  // compressor envelope
    float compAttack = 0.0f;    // ~1ms attack
    float compRelease = 0.0f;   // ~20ms release
    float expandEnvL = 0.0f, expandEnvR = 0.0f;  // per-channel expander

    // BBD bucket leakage (M2): delay-dependent LP that increases with delay length
    float leakageLPStateL[kNumModes] = {};
    float leakageLPStateR[kNumModes] = {};

    // BBD clock feedthrough oscillator (CH4)
    IncrementalOscillator clockOsc;
    int clockRenormCount = 0;

    // BBD noise floor PRNG (MN3009 ~-70dB characteristic)
    uint32_t bbdNoisePrng = 98765u;

    float readInterpolated(const float* buf, float delaySamples) const;

    // Triangle LFO: phase [0, 2π) → [-1, +1]
    // MN3101 integrator RC rounding at peaks — soft-saturate via rational approximant
    inline float triLFO(float phase) const noexcept
    {
        const float norm = phase * (1.0f / juce::MathConstants<float>::twoPi);
        const float raw = (norm < 0.5f) ? (4.0f * norm - 1.0f) : (3.0f - 4.0f * norm);
        // tanh(x*1.05)/tanh(1.05) ≈ x*(27+1.1025*x²)/(27+9*1.1025*x²) / 0.7818
        const float x = raw * 1.05f;
        const float x2 = x * x;
        const float result = (x * (27.0f + x2) / (27.0f + 9.0f * x2)) * (1.0f / 0.7818f);
        return juce::jlimit(-1.0f, 1.0f, result);
    }

};
