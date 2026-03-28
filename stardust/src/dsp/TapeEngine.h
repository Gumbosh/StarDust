#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <cmath>
#include <cstdint>
#include <atomic>
#include "Oscillator.h"

// Quarter-inch reel-to-reel tape emulation (Ampex 456, 7.5/15/30 ips):
// - Hysteresis: Jiles-Atherton with RK4 solver (4x oversampled, 7-tap half-band decimation)
// - Wow: multi-rate (capstan 0.5Hz + pinch roller 1.3Hz + supply reel 0.08Hz)
// - Flutter: 2-source (8Hz + 15Hz), independent PRNG per channel
// - Hiss: Paul Kellet pink noise + 4.5kHz tape character peak, level-dependent
// - DC blocker + soft saturation after hysteresis
// - Pre/de-emphasis: matched NAB biquad high-shelf (+/-3dB), speed-dependent gain
// - Head bump: speed-scaled peaking EQ + notch -- Ampex/Studer
// - Head-gap loss: 2-pole Butterworth LP, speed-scaled cutoff
// - Bandwidth: 2-pole (12dB/oct) Butterworth LP, per-channel, level-dependent
// - Bias control: modulates hysteresis curve shape (K/a parameters)
// - Drive: input gain staging into tape saturation (-12dB to +12dB)
// - Speed selection: 7.5/15/30 ips (scales bump, gap, emphasis, wow/flutter, hiss)
class TapeEngine
{
public:
    TapeEngine() = default;
    TapeEngine(const TapeEngine&) = delete;
    TapeEngine& operator=(const TapeEngine&) = delete;

    struct BiquadCoeffs { float b0=1, b1=0, b2=0, a1=0, a2=0; };
    struct BiquadState { float x1=0, x2=0, y1=0, y2=0; };

    static float processBiquad(float x, const BiquadCoeffs& c, BiquadState& s)
    {
        const float y = c.b0*x + c.b1*s.x1 + c.b2*s.x2 - c.a1*s.y1 - c.a2*s.y2;
        s.x2 = s.x1; s.x1 = x; s.y2 = s.y1; s.y1 = y;
        return y;
    }

    static constexpr float kTwoPi = 6.28318530718f;

    void prepare(double sampleRate, int samplesPerBlock);
    void setWow(float amount);
    void setFlutter(float amount);
    void setHiss(float amount);
    void setBias(float amount);
    void setDrive(float amount);
    void setMix(float amount);
    void setSpeed(float speedIps);
    void setFormulation(int index);
    void setStandard(int index);  // 0=NAB, 1=IEC
    void resetState();
    void process(juce::AudioBuffer<float>& buffer);
    static constexpr int getBaseDelaySamples() { return static_cast<int>(kBaseDelay); }

private:
    struct HysteresisState { float M = 0.0f; float H_prev = 0.0f; };

    // Oscillator type alias (shared implementation in Oscillator.h)
    using Oscillator = IncrementalOscillator;

    // Fast polynomial approximations (replace std::tanh/std::sinh)
    static float fastTanh(float x);
    static float fastSinh(float x);
    static float langevin(float x);
    static float langevinDeriv(float x);
    float hystDMDH(float H, float M, float dH) const;
    float hystRK4Step(float H, float H_prev, HysteresisState& state) const;

    // Extracted process helpers
    float computeModDelay(int ch, float wowAmt, float flutterAmt);
    float processHysteresisBlock(int ch, float input);
    float processSoftSat(float input, int ch);  // dynamic onset from compression envelope
    void processHissBlock(int ch, float hissAmt, float& wet);

    // Speed-dependent coefficient recomputation
    void recomputeSpeedCoeffs();

    float readHermite(int channel, float delaySamples) const;
    static float fastNoise(uint32_t& state);
    void resetChannelState(int ch);

    double sampleRate = 44100.0;

    static constexpr int kMaxChannels = 2;

    // Tape formulations (J-A parameters for different oxide types)
    struct TapeFormulation { float ms, baseA, baseK, c, alpha; };
    static constexpr TapeFormulation kFormulations[] = {
        { 1.0f, 0.35f, 0.45f, 0.10f, 1.6e-4f },  // Ampex 456 (warm, saturated)
        { 0.8f, 0.45f, 0.35f, 0.15f, 1.2e-4f },  // Quantegy GP9 (bright, clean)
        { 0.9f, 0.40f, 0.40f, 0.12f, 1.4e-4f },  // SM900 (balanced, neutral)
    };
    int currentFormulation = 0;
    std::atomic<int> pendingFormulation { 0 };

    // Active hysteresis parameters (base from formulation, K/a modulated by bias)
    float hystMs = 1.0f, hystAlpha = 1.6e-4f, hystC = 0.1f;
    float hystBaseA = 0.35f, hystBaseK = 0.45f;  // formulation center values
    float hystA = 0.35f, hystK = 0.45f;          // actual (bias-modulated)

    // Tape compression dynamics (program-dependent saturation threshold)
    float compEnvState[kMaxChannels] = {};
    float compAttackAlpha = 0.0f, compReleaseAlpha = 0.0f;

    juce::SmoothedValue<float> wowSmoothed { 0.0f };
    juce::SmoothedValue<float> flutterSmoothed { 0.0f };
    juce::SmoothedValue<float> hissSmoothed { 0.0f };
    juce::SmoothedValue<float> biasSmoothed { 0.5f };
    juce::SmoothedValue<float> driveGainSmoothed { 1.0f };
    juce::SmoothedValue<float> mixSmoothed { 1.0f };
    float currentWow = 0.0f, currentFlutter = 0.0f, currentHiss = 0.0f;
    float currentBias = 0.5f, currentDrive = 0.5f, currentMix = 1.0f;

    // Tape speed (7.5, 15.0, or 30.0 ips) — atomic for thread safety
    std::atomic<float> currentSpeed { 7.5f };
    std::atomic<bool> speedChanged { false };

    // Tape EQ standard (0=NAB, 1=IEC)
    std::atomic<int> tapeStandard { 0 };
    std::atomic<bool> standardChanged { false };

    // Speed-dependent scaling (recomputed when speed changes)
    // Base values are for 7.5 ips reference speed
    static constexpr float kRefSpeed = 7.5f;
    static constexpr float kBaseWowDepth = 6.0f;    // reduced from 30 (closer to real tape at 7.5 ips)
    static constexpr float kBaseFlutterDepth = 2.5f; // reduced from 12 (closer to real tape)
    static constexpr float kBaseBumpFreq = 90.0f;    // 90Hz for 7.5 ips (was 50Hz — wrong for this speed)
    static constexpr float kBaseNotchFreq = 180.0f;  // 2x bump freq
    static constexpr float kBaseGapLossFreq = 16000.0f;
    static constexpr float kBaseEmpGainDb = 6.0f;  // NAB HF pre-emphasis at 7.5 ips
    // Base oscillator frequencies at 7.5 ips (scale with speed)
    static constexpr float kBaseCapstanHz = 0.5f;
    static constexpr float kBasePinchHz = 1.3f;
    static constexpr float kBaseReelHz = 0.08f;
    static constexpr float kFlutter1Hz = 8.0f;   // bearing resonance (speed-independent)
    static constexpr float kFlutter2Hz = 15.0f;   // guide friction (speed-independent)
    static constexpr float kBaseDynLpMax = 18000.0f;
    static constexpr float kBaseDynLpRange = 4000.0f; // narrowed from 8kHz (more accurate tape HF compression)
    float wowDepth = kBaseWowDepth;
    float flutterDepth = kBaseFlutterDepth;
    float hissGainScale = 1.0f;
    float dynLpMax = kBaseDynLpMax;
    float dynLpRange = kBaseDynLpRange;

    // Cached per-N-samples values (sqrt/pow computed every kCachedUpdateInterval)
    float cachedWowSqrt = 0.0f;
    float cachedFlutterSqrt = 0.0f;
    float cachedDriveGain = 1.0f;
    float cachedDriveMakeup = 1.0f;

    static constexpr int kDelayBufferSize = 8192;
    static_assert((kDelayBufferSize & (kDelayBufferSize - 1)) == 0, "must be power of 2");
    float delayBuffer[kMaxChannels][kDelayBufferSize] = {};
    int writePos = 0;

    static constexpr float kBaseDelay = 80.0f;

    // Incremental oscillators (replace per-sample std::sin calls)
    Oscillator wow1Osc, wow2Osc, wow3Osc;
    Oscillator flutter1Osc, flutter2Osc;

    // Pre-computed per-channel phase offset trig for ALL oscillators
    float wowOffsetCos[kMaxChannels] = {};
    float wowOffsetSin[kMaxChannels] = {};
    float wow2OffsetCos[kMaxChannels] = {};
    float wow2OffsetSin[kMaxChannels] = {};
    float wow3OffsetCos[kMaxChannels] = {};
    float wow3OffsetSin[kMaxChannels] = {};
    float flutterOffsetCos[kMaxChannels] = {};
    float flutterOffsetSin[kMaxChannels] = {};
    float flutter2OffsetCos[kMaxChannels] = {};
    float flutter2OffsetSin[kMaxChannels] = {};

    float wowNoiseState[kMaxChannels] = {};
    float flutterNoiseState[kMaxChannels] = {};
    uint32_t noiseState[kMaxChannels] = { 12345, 67890 };
    uint32_t flutterPrngState[kMaxChannels] = { 54321, 98765 };
    uint32_t hissPrngState[kMaxChannels] = { 11111, 22222 };

    HysteresisState hystState[kMaxChannels] = {};
    float prevH[kMaxChannels] = {};  // previous-previous H for Hermite upsampling tangents

    static constexpr int kOversample = 4;

    // 7-tap half-band polyphase decimation (cascaded: 4x->2x->1x)
    // Separate history arrays for each first-stage polyphase branch (was shared — filter state bug)
    float hb1aHistory[kMaxChannels][4] = {};
    float hb1bHistory[kMaxChannels][4] = {};
    float hb2History[kMaxChannels][4] = {};

    BiquadCoeffs headBumpCoeffs, notchCoeffs, preEmpCoeffs, deEmpCoeffs, hissCoeffs;
    BiquadCoeffs gapLossCoeffs;
    // NAB LF shelf (318µs time constant — +3dB below ~500Hz for LF warmth)
    BiquadCoeffs nabLfPreCoeffs, nabLfDeCoeffs;
    BiquadState nabLfPreState[kMaxChannels], nabLfDeState[kMaxChannels];
    BiquadState headBumpState[kMaxChannels], notchState[kMaxChannels];
    BiquadState preEmpState[kMaxChannels], deEmpState[kMaxChannels];
    BiquadState hissFilterState[kMaxChannels];
    BiquadState gapLossState[kMaxChannels];

    BiquadCoeffs dynLpCoeffs[kMaxChannels];
    BiquadState dynLpBqState[kMaxChannels];
    float dynLpEnvState[kMaxChannels] = {};
    float envAlpha = 0.0f;

    float dcBlockX1[kMaxChannels] = {};
    float dcBlockY1[kMaxChannels] = {};
    float dcBlockCoeff = 0.995f;

    float pinkB0[kMaxChannels] = {}, pinkB1[kMaxChannels] = {}, pinkB2[kMaxChannels] = {};
    float pinkB3[kMaxChannels] = {}, pinkB4[kMaxChannels] = {};

    float wowNoiseAlpha = 0.0f, flutterNoiseAlpha = 0.0f;

    int sampleCounter = 0;
};
