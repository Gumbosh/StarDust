#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <cmath>

// SP-950 style input/output level control with frequency-dependent saturation.
// 4× oversampled nonlinearity with asymmetric clipping (D9-D11).
class DestroyDrive
{
public:
    DestroyDrive() = default;

    void prepare(double sampleRate, int samplesPerBlock);

    void setInputDrive(float gainDb);   // 0.0 to +24.0 dB
    void setOutputColor(float gainDb);  // -12.0 to +12.0 dB

    void processInput(juce::AudioBuffer<float>& buffer);
    void processOutput(juce::AudioBuffer<float>& buffer);

private:
    static float dBToLinear(float dB) { return std::pow(10.0f, dB / 20.0f); }

    // Fast tanh rational approximant
    static float fastTanh(float x) noexcept;

    juce::SmoothedValue<float> inputGainSmoothed { 1.0f };
    juce::SmoothedValue<float> outputGainSmoothed { 1.0f };

    float currentInputDb = 0.0f;
    float currentOutputDb = 0.0f;
    double sampleRate = 44100.0;

    // 4× oversampling state (D11)
    static constexpr int kMaxChannels = 2;
    static constexpr int kOversample = 4;
    float prevSample[kMaxChannels] = {};
    float prevPrevSample[kMaxChannels] = {};

    // 2-stage half-band decimation history (4×→2×→1×)
    float hb1History[kMaxChannels][8] = {};  // stage 1: 4×→2×
    float hb2History[kMaxChannels][8] = {};  // stage 1: other pair
    float hb3History[kMaxChannels][8] = {};  // stage 2: 2×→1×

    // C5: Linkwitz-Riley 4th-order crossover at 500Hz (2 cascaded Butterworth stages)
    // LP path
    float xoverLP_B0 = 1.0f, xoverLP_B1 = 0.0f, xoverLP_B2 = 0.0f;
    float xoverLP_A1 = 0.0f, xoverLP_A2 = 0.0f;
    float xoverLPZ1a[kMaxChannels] = {}, xoverLPZ2a[kMaxChannels] = {};  // stage 1
    float xoverLPZ1b[kMaxChannels] = {}, xoverLPZ2b[kMaxChannels] = {};  // stage 2
    // HP path (explicit, no subtraction)
    float xoverHP_B0 = 1.0f, xoverHP_B1 = 0.0f, xoverHP_B2 = 0.0f;
    float xoverHP_A1 = 0.0f, xoverHP_A2 = 0.0f;
    float xoverHPZ1a[kMaxChannels] = {}, xoverHPZ2a[kMaxChannels] = {};  // stage 1
    float xoverHPZ1b[kMaxChannels] = {}, xoverHPZ2b[kMaxChannels] = {};  // stage 2

    // DC blocker: 1-pole HP at ~8Hz (C4: removes DC offset from asymmetric saturation)
    float dcBlockX1[kMaxChannels] = {};
    float dcBlockY1[kMaxChannels] = {};
    float dcBlockCoeff = 0.9989f;

    // DC tracking envelope: slow follower (~2Hz) for even-harmonic compensation (C6)
    float dcTracker[kMaxChannels] = {};
    float dcTrackerAlpha = 0.0003f;

    static float halfBandDecimate(float a, float b, float* hist) noexcept;
};
