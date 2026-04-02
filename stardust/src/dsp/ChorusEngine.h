#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <cmath>

// Roland Juno-60 ensemble chorus emulation.
// Hardware: 2× MN3009 256-stage BBD chips + MN3101 clock driver.
//
// Circuit-accurate signal path:
//   1. Input mono-summed → 2-pole 12dB/oct LP anti-alias filter (~3.4kHz cutoff)
//   2. BBD input stage soft saturation (OTA input characteristic)
//   3. Written to single mono delay buffer
//   4. L and R tapped at different delays (LFO-modulated):
//        Modes I & II standalone: R = inverted LFO (stereo widening)
//        Combined I+II: Mode I = inverted stereo, Mode II = non-inverted (Leslie swirl)
//   5. Output 1-pole LP ~8kHz per BBD chip (MN3009 capacitor rolloff)
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
    int junoMode = 0;

    // Single MONO delay buffer — both L and R tap from the same BBD path
    static constexpr int kDelayBufferSize = 8192;
    std::array<float, kDelayBufferSize> delayBuffer {};
    int writePos = 0;

    // 2-pole Butterworth LP pre-filter state (12dB/oct — matches Juno-60 CN-2 board)
    float hp1z1 = 0.0f, hp1z2 = 0.0f;  // biquad state
    float preB0 = 1.0f, preB1 = 0.0f, preB2 = 0.0f;
    float preA1 = 0.0f, preA2 = 0.0f;

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

    float readInterpolated(float delaySamples) const;

    // Triangle LFO: phase [0, 2π) → [-1, +1]
    inline float triLFO(float phase) const noexcept
    {
        const float norm = phase * (1.0f / juce::MathConstants<float>::twoPi);
        return (norm < 0.5f) ? (4.0f * norm - 1.0f) : (3.0f - 4.0f * norm);
    }
};
