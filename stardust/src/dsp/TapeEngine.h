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
// - Drive: input gain staging into tape saturation (0..+12 dB from normalized drive)
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
    void setTapeOutputDb(float db);
    void setWearTone(float amount);
    void setHissSpeedIps(float ips);
    void setSpeed(float speedIps);
    void setFormulation(int index);
    void setStandard(int index);  // 0=NAB, 1=IEC
    void setPrintThrough(float level); // 0-1, scales print-through coupling
    void setMotorEnabled(bool enabled); // ramps output up/down over ~2s
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
    void recomputeCrossoverCoeffs();

    float readHermite(int channel, float delaySamples) const;
    static float fastNoise(uint32_t& state);
    void resetChannelState(int ch);

    double sampleRate = 44100.0;

    static constexpr int kMaxChannels = 2;

    // Tape formulations (J-A parameters for different oxide types)
    struct TapeFormulation { float ms, baseA, baseK, c, alpha; };
    static constexpr TapeFormulation kFormulations[] = {
        { 1.0f, 0.35f, 0.45f, 0.10f, 1.6e-4f },  // 0: Ampex 456 (warm, saturated)
        { 0.8f, 0.45f, 0.35f, 0.15f, 1.2e-4f },  // 1: Quantegy GP9 (bright, clean)
        { 0.9f, 0.40f, 0.40f, 0.12f, 1.4e-4f },  // 2: SM900 (balanced, neutral)
        { 0.75f, 0.50f, 0.30f, 0.18f, 1.0e-4f }, // 3: BASF LH (bright, LF-limited)
        { 1.05f, 0.38f, 0.42f, 0.11f, 1.5e-4f }, // 4: SM468 (wide, neutral, high output)
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

    // HF energy envelope for frequency-dependent saturation (C6)
    // 8-sample time constant — fast enough to track HF transients
    float hfEnvState[kMaxChannels] = {};
    static constexpr float kHFEnvAlpha = 0.117f; // 1 - exp(-1/8)

    juce::SmoothedValue<float> wowSmoothed { 0.0f };
    juce::SmoothedValue<float> flutterSmoothed { 0.0f };
    juce::SmoothedValue<float> hissSmoothed { 0.0f };
    juce::SmoothedValue<float> biasSmoothed { 0.5f };
    juce::SmoothedValue<float> driveGainSmoothed { 1.0f };
    juce::SmoothedValue<float> mixSmoothed { 1.0f };
    juce::SmoothedValue<float> tapeOutGainSmoothed { 1.0f };
    juce::SmoothedValue<float> wearToneSmoothed { 0.0f };
    float currentWow = 0.0f, currentFlutter = 0.0f, currentHiss = 0.0f;
    float currentBias = 0.5f, currentDrive = 0.5f, currentMix = 1.0f;
    float currentTapeOutDb = 0.0f;
    float currentWearTone = 0.0f;

    // Tape speed (7.5, 15.0, or 30.0 ips) — atomic for thread safety
    std::atomic<float> currentSpeed { 7.5f };
    std::atomic<bool> speedChanged { false };

    // Tape EQ standard (0=NAB, 1=IEC)
    std::atomic<int> tapeStandard { 0 };
    std::atomic<bool> standardChanged { false };

    // Speed-dependent scaling (recomputed when speed changes)
    // Base values are for 7.5 ips reference speed
    static constexpr float kRefSpeed = 7.5f;
    static constexpr float kBaseWowDepth = 70.0f;   // ~5 cents at full wear, 15 ips (audible tape wobble)
    static constexpr float kBaseFlutterDepth = 10.0f; // ~5 cents flutter at full wear (clearly characterful)
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
    float dynLpMax = kBaseDynLpMax;
    float dynLpRange = kBaseDynLpRange;

    // Cached per-N-samples values (sqrt/pow computed every kCachedUpdateInterval)
    float cachedWowSqrt = 0.0f;
    float cachedFlutterSqrt = 0.0f;
    float cachedDriveGain = 1.0f;
    float cachedDriveMakeup = 1.0f;
    float cachedBiasNorm = 1.05f;  // = 3.0f * hystA — normalises J-A small-signal gain to ~1
    float cachedSatOnset = 0.45f;  // speed-adaptive soft saturation onset threshold
    float hissSpeedIpsStored = 15.0f;
    float cachedHissSpeedScale = 1.0f;

    static constexpr int kDelayBufferSize = 8192;
    static_assert((kDelayBufferSize & (kDelayBufferSize - 1)) == 0, "must be power of 2");
    float delayBuffer[kMaxChannels][kDelayBufferSize] = {};
    int writePos = 0;

    // Print-through simulation: adjacent tape layer magnetization (C1)
    // Wet output is delayed 3ms and added to the future input at -46dB (0.005 linear)
    static constexpr int kPrintThroughBufSize = 512; // power-of-2; 512/44100 ≈ 11.6ms
    static constexpr float kPrintThroughCoeff = 0.005f; // -46dB coupling
    float printThroughBuf[kMaxChannels][kPrintThroughBufSize] = {};
    int printThroughWritePos = 0;
    int printThroughDelaySamples = 132; // ~3ms at 44.1kHz; recomputed in prepare()
    float printThroughLevel = 1.0f;    // user-controlled 0-1 scale on print-through

    // Motor start/stop ramp (ramps output over ~2s to simulate motor speed-up/down)
    bool motorEnabled = true;
    float motorRamp = 1.0f;    // current gain [0-1]; 1=running, 0=stopped
    float motorRampInc = 0.0f; // per-sample increment (positive=ramping up, negative=down)

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

    static constexpr int kOversample = 8;

    // 7-tap half-band polyphase decimation (cascaded: 8x->4x->2x->1x — 3 stages)
    // 3-stage cascade eliminates J-A discretization error above 15kHz at 8× vs 4×
    float hb1aHistory[kMaxChannels][4] = {};
    float hb1bHistory[kMaxChannels][4] = {};
    float hb1cHistory[kMaxChannels][4] = {}; // stage 1 branch C (samples 4-5)
    float hb1dHistory[kMaxChannels][4] = {}; // stage 1 branch D (samples 6-7)
    float hb2aHistory[kMaxChannels][4] = {}; // stage 2 branch A (hb1a+hb1b)
    float hb2bHistory[kMaxChannels][4] = {}; // stage 2 branch B (hb1c+hb1d)
    float hb3History[kMaxChannels][4] = {};  // stage 3 (hb2a+hb2b → 1x)

    // Half-band decimation history for LF/HF band oversampling (4× → 2× → 1×, 2 stages each)
    // Each branch (a = samples 0-1, b = samples 2-3) needs independent state (A7 fix)
    float hbLFaHistory[kMaxChannels][4] = {};  // LF stage 1 branch A: osLF[0,1]
    float hbLFbHistory[kMaxChannels][4] = {};  // LF stage 1 branch B: osLF[2,3]
    float hbLFHistory2[kMaxChannels][4] = {};  // LF stage 2: 2× → 1×
    float hbHFaHistory[kMaxChannels][4] = {};  // HF stage 1 branch A: osHF[0,1]
    float hbHFbHistory[kMaxChannels][4] = {};  // HF stage 1 branch B: osHF[2,3]
    float hbHFHistory2[kMaxChannels][4] = {};  // HF stage 2: 2× → 1×

    BiquadCoeffs headBumpCoeffs, notchCoeffs, preEmpCoeffs, deEmpCoeffs, hissCoeffs;
    // Gap comb filter: sinc multi-null response  y[n] = 0.5*(x[n] + x[n-D])
    // D = sr * kHeadGapM / (speedIps * 0.0254) → first null at v/(2*g)
    static constexpr float kHeadGapM = 6.8e-6f; // 6.8 micron effective gap (Ampex-style)
    static constexpr int kGapBufSize = 16;       // covers D up to 16 samples (192kHz + 7.5ips)
    static constexpr int kGapMask = kGapBufSize - 1;
    float gapBuf[kMaxChannels][kGapBufSize] = {};
    int gapWritePos = 0;
    float gapDelaySamples = 1.575f; // recomputed in recomputeSpeedCoeffs()
    // NAB LF shelf (318µs time constant — +3dB below ~500Hz for LF warmth)
    BiquadCoeffs nabLfPreCoeffs, nabLfDeCoeffs;
    BiquadState nabLfPreState[kMaxChannels], nabLfDeState[kMaxChannels];
    BiquadState headBumpState[kMaxChannels], notchState[kMaxChannels];
    BiquadState preEmpState[kMaxChannels], deEmpState[kMaxChannels];
    BiquadState hissFilterState[kMaxChannels];

    // 3-band J-A crossover filters (LP at 500Hz and 5kHz for band splitting)
    BiquadCoeffs xoverLF500Coeffs {};   // LP at 500Hz  (extracts LF band)
    BiquadCoeffs xoverHF5kCoeffs  {};   // LP at 5kHz   (extracts LF+MID band)
    BiquadState  xoverLF500State[kMaxChannels]  {};
    BiquadState  xoverHF5kState[kMaxChannels]   {};
    // Second filter instance for MID band extraction (parallel path, own state)
    BiquadState  xoverLF500bState[kMaxChannels] {};
    BiquadState  xoverHF5kbState[kMaxChannels]  {};

    // Per-band J-A states for LF (<500Hz) and HF (>5kHz) bands
    HysteresisState hystStateLF[kMaxChannels] {};
    HysteresisState hystStateHF[kMaxChannels] {};
    float prevHLF[kMaxChannels] {};   // previous H for LF band oversampling
    float prevHHF[kMaxChannels] {};   // previous H for HF band oversampling

    BiquadCoeffs dynLpCoeffs[kMaxChannels];
    BiquadState dynLpBqState[kMaxChannels];
    float dynLpEnvState[kMaxChannels] = {};
    float envAlpha = 0.0f;

    BiquadCoeffs wearToneLpCoeffs {};
    BiquadState wearToneLpState[kMaxChannels] {};
    // Second pole pair — same coefficients, cascaded to form 4-pole (24 dB/oct) Butterworth
    BiquadState wearToneLp2State[kMaxChannels] {};
    float glueCohesionEnv[kMaxChannels] = {};
    float glueAttackAlpha = 0.0f, glueReleaseAlpha = 0.0f;

    // Azimuth misalignment: right channel (ch=1) delayed 0–50µs vs left (worn head tilt)
    static constexpr float kAzimMaxUs = 50.0f;
    static constexpr int kAzimBufSize = 16;  // covers 50µs at up to 192kHz
    static constexpr int kAzimMask = kAzimBufSize - 1;
    float azimBuf[kAzimBufSize] = {};
    int azimWritePos = 0;

    float dcBlockX1[kMaxChannels] = {};
    float dcBlockY1[kMaxChannels] = {};
    float dcBlockCoeff = 0.995f;

    float pinkB0[kMaxChannels] = {}, pinkB1[kMaxChannels] = {}, pinkB2[kMaxChannels] = {};
    float pinkB3[kMaxChannels] = {}, pinkB4[kMaxChannels] = {};

    float wowNoiseAlpha = 0.0f, flutterNoiseAlpha = 0.0f;

    int sampleCounter = 0;
};
