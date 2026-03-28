#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <cmath>
#include <cstdint>

// Quarter-inch reel-to-reel tape emulation (Ampex 456, 7.5 ips):
// - Wow: subtle pitch drift from capstan speed variation (~0.5Hz)
// - Flutter: tight fast pitch wobble (8Hz primary + 15Hz scrape)
// - Hiss: broadband tape noise (6kHz center, Q=0.8, level-dependent)
// - Tape saturation: gentle asymmetric tanh (Ampex 456 +6dB headroom)
// - Head bump: peaking EQ at 60Hz (+3dB) with notch at 120Hz (-1.5dB)
// - Pre/de-emphasis: NAB 50µs (~3.2kHz treble boost/cut)
// - Bandwidth limit: level-dependent LP (18kHz quiet, 10kHz loud)
class TapeEngine
{
public:
    TapeEngine() = default;

    void prepare(double sampleRate, int samplesPerBlock);
    void setWow(float amount);
    void setFlutter(float amount);
    void setHiss(float amount);
    void process(juce::AudioBuffer<float>& buffer);

private:
    float readHermite(int channel, float delaySamples) const;
    static float fastNoise(uint32_t& state);

    double sampleRate = 44100.0;

    juce::SmoothedValue<float> wowSmoothed { 0.0f };
    juce::SmoothedValue<float> flutterSmoothed { 0.0f };
    juce::SmoothedValue<float> hissSmoothed { 0.0f };

    float currentWow = 0.0f;
    float currentFlutter = 0.0f;
    float currentHiss = 0.0f;

    static constexpr int kMaxChannels = 2;
    static constexpr int kDelayBufferSize = 8192;
    float delayBuffer[kMaxChannels][kDelayBufferSize] = {};
    int writePos = 0;

    // Reel-to-reel: tighter transport, less modulation than cassette
    static constexpr float kBaseDelay = 80.0f;
    static constexpr float kWowDepth = 30.0f;    // ±0.7ms — pro transport
    static constexpr float kFlutterDepth = 12.0f; // ±0.3ms — direct drive

    // LFOs: 2 flutter sources (no tertiary drift on pro transport)
    float wowPhase = 0.0f;
    float flutterPhase1 = 0.0f;
    float flutterPhase2 = 0.0f;
    float wowPhaseInc = 0.0f;
    float flutter1PhaseInc = 0.0f;
    float flutter2PhaseInc = 0.0f;

    float wowNoiseState[kMaxChannels] = {};
    float flutterNoiseState[kMaxChannels] = {};
    uint32_t noiseState[kMaxChannels] = { 12345, 67890 };

    // Head bump: peaking EQ at 60Hz (+3dB, Q=0.7)
    float headBumpX1[kMaxChannels] = {}, headBumpX2[kMaxChannels] = {};
    float headBumpY1[kMaxChannels] = {}, headBumpY2[kMaxChannels] = {};
    float hbB0 = 1.0f, hbB1 = 0.0f, hbB2 = 0.0f, hbA1 = 0.0f, hbA2 = 0.0f;

    // Notch at 120Hz (-1.5dB, Q=1.0)
    float notchX1[kMaxChannels] = {}, notchX2[kMaxChannels] = {};
    float notchY1[kMaxChannels] = {}, notchY2[kMaxChannels] = {};
    float nB0 = 1.0f, nB1 = 0.0f, nB2 = 0.0f, nA1 = 0.0f, nA2 = 0.0f;

    // Hiss: biquad bandpass at 6kHz (Q=0.8)
    float hissBpX1[kMaxChannels] = {}, hissBpX2[kMaxChannels] = {};
    float hissBpY1[kMaxChannels] = {}, hissBpY2[kMaxChannels] = {};
    float hBpB0 = 0.0f, hBpB1 = 0.0f, hBpB2 = 0.0f, hBpA1 = 0.0f, hBpA2 = 0.0f;

    // NAB pre/de-emphasis (50µs, ~3183Hz)
    float preEmpY1[kMaxChannels] = {};
    float peB0 = 1.0f;
    float deEmpY1[kMaxChannels] = {};
    float deB0 = 1.0f;

    // Level-dependent LP (18kHz quiet → 10kHz loud)
    float dynLpState[kMaxChannels] = {};
    float dynLpEnvState[kMaxChannels] = {};
    float envAlpha = 0.0f;

    float wowNoiseAlpha = 0.0f;
    float flutterNoiseAlpha = 0.0f;
};
