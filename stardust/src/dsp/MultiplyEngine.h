#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <cmath>
#include "Oscillator.h"

// 4-voice unison thickener modeled after Acon Digital Multiply.
// FM-modulated micro-delays + very short allpass diffusion for transparent thickening.
// No granular pitch shifting, no aggressive AM — detuning comes solely from FM delay modulation.
class MultiplyEngine
{
public:
    MultiplyEngine() = default;

    void prepare(double sampleRate, int samplesPerBlock);

    void setMix(float mix);
    void setSpeed(float speed);
    void setPans(float outer, float inner); // 1+2 and 3+4 spread (0=centre, 1=full)

    void process(juce::AudioBuffer<float>& buffer);

private:
    static constexpr int kNumVoices       = 4;
    static constexpr int kDelayBufferSize = 4096;
    static constexpr int kDelayMask       = kDelayBufferSize - 1;
    static constexpr int kMaxChannels     = 2;

    // Allpass diffusion chain: 3 cascaded micro-allpasses per voice.
    // Keep this subtle so the effect stays immediate and does not feel predelayed.
    static constexpr int kNumAllpasses    = 3;
    static constexpr float kAllpassFeedback = 0.12f;  // subtle — less phase smearing in low end

    // Renormalize incremental oscillators every N samples
    static constexpr int kNormalizeInterval = 512;

    double sampleRate = 44100.0;

    juce::SmoothedValue<float> mixSmoothed   { 0.5f };
    juce::SmoothedValue<float> speedSmoothed { 1.0f };
    float currentMix   = 0.5f;
    float currentSpeed = 1.0f;

    std::array<std::array<float, kDelayBufferSize>, kMaxChannels> delayBuffer {};
    int writePos = 0;
    int sampleCounter = 0; // for oscillator renormalization

    struct AllpassState
    {
        static constexpr int kAllpassBufSize = 2048;
        static constexpr int kAllpassBufMask = kAllpassBufSize - 1;
        float buffer[kAllpassBufSize] = {};
        int writePos = 0;
        int delaySamples = 5;
    };

    struct Voice
    {
        float baseDelaySamples = 0.0f;
        float pan              = 0.5f;
        float panTarget        = 0.5f;
        float fmDepthSamples   = 0.0f;

        // Per-channel FM oscillators for independent modulation
        IncrementalOscillator fmOsc[kMaxChannels];
        float fmBaseRate = 1.0f; // base FM LFO rate (before speed scaling)

        // Allpass diffusion per channel
        AllpassState allpass[kMaxChannels][kNumAllpasses] = {};

        // ITD offset for spatial imaging
        float itdDelaySamples = 0.0f;
    };
    std::array<Voice, kNumVoices> voices {};

    // M4: Bass mono crossover at 320Hz (2-pole Butterworth LP)
    float bassCrossB0 = 1.0f, bassCrossB1 = 0.0f, bassCrossB2 = 0.0f;
    float bassCrossA1 = 0.0f, bassCrossA2 = 0.0f;
    float bassCrossZ1[kMaxChannels] = {};
    float bassCrossZ2[kMaxChannels] = {};

    float readHermite(int channel, float delaySamples) const;
    static float processAllpass(float input, AllpassState& state);
};
