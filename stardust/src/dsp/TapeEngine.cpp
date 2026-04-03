#include "TapeEngine.h"

// Soft saturation knee width (speed-adaptive onset is in cachedSatOnset)
static constexpr float kSatTransition = 0.55f;

// Hiss parameters — gain increased for authentic tape noise floor at default 7% setting;
// reduction max raised so loud signals mask noise more convincingly (psychoacoustic masking)
static constexpr float kHissGain = 0.12f;
static constexpr float kHissPeakFreq = 2250.0f;  // oxide character peak at 7.5 ips (scales linearly with speed)
static constexpr float kHissPeakQ = 0.6f;


// Modulation mixing weights (physically motivated for 1/4" transport mechanics)
static constexpr float kCapstanWeight = 0.6f;         // primary wow (capstan motor, 60%)
static constexpr float kPinchWeight = 0.2f;            // pinch roller eccentricity (20%)
static constexpr float kReelWeight = 0.2f;             // supply reel variation (20%)
static constexpr float kFlutterPrimaryWeight = 0.6f;   // 8Hz main flutter (60%)
static constexpr float kFlutterSecondaryWeight = 0.25f; // 15Hz secondary flutter (25%)
static constexpr float kNoiseModDepth = 0.3f;          // random modulation contribution

// Drive control range (maps 0..1 via smoothstep to 0..+18 dB — knob=0 is unity, no attenuation)
static constexpr float kDriveMaxDb = 18.0f;

// Chain makeup: post-chain level trim after exact analytical bias normalisation.
// cachedBiasNorm is now the exact analytical inverse of J-A small-signal gain, so
// kBaseChainMakeup × kUnityLiftAtMinDrive ≈ 1.0 yields output unity at Drive=0
// across the full Glue range. Slight raise above 1/kUnityLiftAtMinDrive (0.251)
// compensates for residual psychoacoustic loudness loss from tape's darker HF character.
static constexpr float kBaseChainMakeup = 0.25f;
static constexpr float kMaxDriveLinearGain = 7.9432823f; // 10^(18/20)
static constexpr float kDriveSpanAsym = kMaxDriveLinearGain - 1.0f; // denominator for asymOffset normalisation
static constexpr float kUnityLiftAtMinDrive = 3.9810717f; // +12 dB when cachedDriveGain == 1

// Periodic update intervals
static constexpr int kCachedUpdateInterval = 16;
static constexpr int kOscNormalizeInterval = 512;

// LF/HF band oversampling factor (MID band uses kOversample=8 from the header)
static constexpr int kOs4x = 4;

// Per-channel phase offsets (radians) — ALL oscillators get stereo decorrelation
static constexpr float kWowOffset[2] = { 0.0f, 1.2f };
static constexpr float kWow2Offset[2] = { 0.0f, 1.5f };    // pinch roller
static constexpr float kWow3Offset[2] = { 0.0f, 0.15f };   // supply reel
static constexpr float kFlutterOffset[2] = { 0.0f, 0.5f };
static constexpr float kFlutter2Offset[2] = { 0.0f, 0.7f }; // secondary flutter

// Head bump gain/Q (constant across speeds, frequency scales)
static constexpr float kBumpGainDb = 2.5f;
static constexpr float kBumpQ = 0.8f;
static constexpr float kNotchGainDb = -1.5f;
static constexpr float kNotchQ = 0.8f;

// 7-tap half-band kernel: [-0.03125, 0, 0.28125, 0.5, 0.28125, 0, -0.03125]
// ~42dB stopband rejection. All 5 non-zero taps applied via polyphase decomposition.

// ============================================================================
// Fast polynomial approximations (replace std::tanh/std::sinh in hot path)
// ============================================================================

float TapeEngine::fastTanh(float x)
{
    // Hard clamp beyond ±3.5: tanh(3.5) ≈ 0.9982, error to ±1.0 is negligible.
    if (x > 3.5f) return 1.0f;
    if (x < -3.5f) return -1.0f;
    // [5/5] Padé rational approximant — max error < 0.03% across [-3.5, 3.5]
    const float x2 = x * x;
    const float pade = x * (135135.0f + x2 * (17325.0f + x2 * 378.0f))
                         / (135135.0f + x2 * (62370.0f + x2 * (3150.0f + x2 * 28.0f)));
    // Blend to ±1 over [3.0, 3.5] with cubic smoothstep — eliminates C¹ discontinuity
    // at the hard-clamp boundary. The common case (|x| < 3.0) returns immediately above,
    // so this branch only costs ~1% of hot-path calls.
    const float ax = std::abs(x);
    if (ax > 3.0f)
    {
        const float t = (ax - 3.0f) * 2.0f;                      // [0,1] over [3.0, 3.5]
        const float blend = t * t * (3.0f - 2.0f * t);           // cubic smoothstep
        const float sign = x >= 0.0f ? 1.0f : -1.0f;
        return pade + blend * (sign - pade);
    }
    return pade;
}

float TapeEngine::fastSinh(float x)
{
    const float ax = std::abs(x);
    if (ax < 3.0f)
    {
        // 7-term Maclaurin series — max relative error < 0.003% at |x|=3.
        // Covers the full operating range of langevinDeriv without a discontinuous join.
        const float x2 = x * x;
        return x * (1.0f + x2 * (1.0f/6.0f
               + x2 * (1.0f/120.0f
               + x2 * (1.0f/5040.0f
               + x2 * (1.0f/362880.0f
               + x2 * (1.0f/39916800.0f
               + x2 *  1.0f/6227020800.0f))))));
    }
    const float e = std::exp(ax);
    return (x >= 0.0f ? 1.0f : -1.0f) * (e - 1.0f / e) * 0.5f;
}

float TapeEngine::langevin(float x)
{
    // L(x) = coth(x) - 1/x. Taylor for |x| < 0.2 (4-term, accurate to ~1e-10).
    // General branch uses std::tanh (not fastTanh) because 1/tanh amplifies
    // approximation error — fastTanh's Padé causes ~0.7% jump at boundary.
    const float ax = std::abs(x);
    if (ax < 0.2f)
    {
        const float x2 = x * x;
        return x * (1.0f/3.0f - x2 * (1.0f/45.0f - x2 * (2.0f/945.0f - x2 * (1.0f/4725.0f))));
    }
    return 1.0f / std::tanh(x) - 1.0f / x;
}

float TapeEngine::langevinDeriv(float x)
{
    // L'(x) = 1/x^2 - 1/sinh^2(x). Extended Taylor to 0.25 (5-term, ~1e-8 error).
    const float ax = std::abs(x);
    if (ax < 0.25f)
    {
        const float x2 = x * x;
        return 1.0f/3.0f - x2 * (1.0f/15.0f - x2 * (2.0f/189.0f - x2 * (1.0f/675.0f - x2 * (2.0f/10395.0f))));
    }
    const float sinhX = fastSinh(x);
    return 1.0f / (x * x) - 1.0f / (sinhX * sinhX);
}

float TapeEngine::hystDMDH(float H, float M, float dH, float ms, float k) const
{
    const float He = H + hystAlpha * M;
    const float Man = ms * langevin(He / hystA);
    const float delta = (dH >= 0.0f) ? 1.0f : -1.0f;
    const float denom = delta * k - hystAlpha * (Man - M);
    if (std::abs(denom) < 1e-8f) return 0.0f;
    float dMirr = (Man - M) / denom;
    if (delta > 0.0f && dMirr < 0.0f) dMirr = 0.0f;
    if (delta < 0.0f && dMirr > 0.0f) dMirr = 0.0f;
    const float dMan = ms * langevinDeriv(He / hystA) / hystA;
    return (1.0f - hystC) * dMirr + hystC * dMan;
}

float TapeEngine::hystRK4Step(float H, HysteresisState& state, float ms, float k) const
{
    const float dH = H - state.H_prev;
    if (std::abs(dH) < 1e-10f) { state.H_prev = H; return state.M; }
    const float H_mid = state.H_prev + dH * 0.5f;
    const float k1 = hystDMDH(state.H_prev, state.M, dH, ms, k);
    const float k2 = hystDMDH(H_mid, state.M + 0.5f * dH * k1, dH, ms, k);
    const float k3 = hystDMDH(H_mid, state.M + 0.5f * dH * k2, dH, ms, k);
    const float k4 = hystDMDH(H, state.M + dH * k3, dH, ms, k);
    state.M += (dH / 6.0f) * (k1 + 2.0f * k2 + 2.0f * k3 + k4);
    // Clamp to ±1.5×Ms: allows modest transient overshoot (physically plausible)
    // while preventing unrealistic excursions that the ±2×Ms bound permitted.
    state.M = std::max(-ms * 1.5f, std::min(ms * 1.5f, state.M));
    state.H_prev = H;
    return state.M;
}

// ============================================================================
// Helpers
// ============================================================================

float TapeEngine::fastNoise(uint32_t& state)
{
    if (state == 0) state = 1;
    state ^= state << 13; state ^= state >> 17; state ^= state << 5;
    return static_cast<float>(state) / static_cast<float>(UINT32_MAX) * 2.0f - 1.0f;
}

static TapeEngine::BiquadCoeffs makePeakingEQ(float f0, float gainDb, float Q, float sr)
{
    const float A = std::pow(10.0f, gainDb / 40.0f);
    const float w0 = TapeEngine::kTwoPi * f0 / sr;
    const float alpha = std::sin(w0) / (2.0f * Q);
    const float a0 = 1.0f + alpha / A;
    return { (1+alpha*A)/a0, (-2*std::cos(w0))/a0, (1-alpha*A)/a0,
             (-2*std::cos(w0))/a0, (1-alpha/A)/a0 };
}

static TapeEngine::BiquadCoeffs makeHighShelf(float f0, float gainDb, float Q, float sr)
{
    const float A = std::pow(10.0f, gainDb / 40.0f);
    const float w0 = TapeEngine::kTwoPi * f0 / sr;
    const float s = std::sin(w0), c = std::cos(w0);
    const float alpha = s / (2.0f * Q);
    const float sq = 2.0f * std::sqrt(A) * alpha;
    const float a0 = (A+1) - (A-1)*c + sq;
    return { (A*((A+1)+(A-1)*c+sq))/a0, (-2*A*((A-1)+(A+1)*c))/a0,
             (A*((A+1)+(A-1)*c-sq))/a0, (2*((A-1)-(A+1)*c))/a0,
             ((A+1)-(A-1)*c-sq)/a0 };
}


// 1st-order shelf via bilinear transform of analog RC integrator
// H(s) = (s + w_z) / (s + w_p) where w_z = 2*pi*f0, w_p = w_z / A
// Gives -20dB/decade slope (correct for NAB 3180µs time constant)
// Fits in BiquadCoeffs with b2=0, a2=0
static TapeEngine::BiquadCoeffs makeFirstOrderShelf(float f0, float gainDb, float sr)
{
    const float A = std::pow(10.0f, gainDb / 20.0f); // linear gain
    const float wz = TapeEngine::kTwoPi * f0;        // zero frequency (turnover)
    const float wp = wz / A;                          // pole frequency (DC gain = A)
    const float T2 = 2.0f * sr;                       // bilinear scaling
    const float norm = 1.0f / (T2 + wp);
    return { (T2 + wz) * norm,   // b0
             (wz - T2) * norm,   // b1
             0.0f,               // b2 (1st order — unused)
             (wp - T2) * norm,   // a1
             0.0f };             // a2 (1st order — unused)
}

static constexpr float kSqrt2 = juce::MathConstants<float>::sqrt2;

static TapeEngine::BiquadCoeffs makeButterworthLP(float fc, float sr)
{
    const float clamped = std::min(fc, sr * 0.45f);
    const float w0 = TapeEngine::kTwoPi * clamped / sr;
    const float s = std::sin(w0), c = std::cos(w0);
    const float alpha = s / kSqrt2;
    const float a0 = 1.0f + alpha;
    return { ((1-c)*0.5f)/a0, (1-c)/a0, ((1-c)*0.5f)/a0, (-2*c)/a0, (1-alpha)/a0 };
}

// ============================================================================
// Crossover filter coefficient recomputation (3-band J-A band splitting)
// ============================================================================

void TapeEngine::recomputeCrossoverCoeffs()
{
    xoverLF500Coeffs = makeButterworthLP(500.0f,  static_cast<float>(sampleRate));
    xoverHF5kCoeffs  = makeButterworthLP(5000.0f, static_cast<float>(sampleRate));
}

// ============================================================================
// Speed-dependent coefficient recomputation
// ============================================================================

void TapeEngine::recomputeSpeedCoeffs()
{
    const float sr = static_cast<float>(sampleRate);
    const float speed = currentSpeed.load();
    const float speedFactor = speed / kRefSpeed;  // 1.0 at 7.5, 2.0 at 15, 4.0 at 30

    // Wow/flutter: less modulation at higher speeds (tape moves faster)
    wowDepth = kBaseWowDepth / speedFactor;
    flutterDepth = kBaseFlutterDepth / std::sqrt(speedFactor);

    // Wow oscillator frequencies scale with speed (capstan RPM increases)
    // setFreq preserves phase to avoid click on speed change
    wow1Osc.setFreq(kBaseCapstanHz * speedFactor, sr);
    wow2Osc.setFreq(kBasePinchHz * speedFactor, sr);
    wow3Osc.setFreq(kBaseReelHz * speedFactor, sr);  // reel spins faster at higher tape speed

    // Dynamic LP: max bandwidth widens at higher speeds; range is fixed (speed-independent)
    dynLpMax = std::min(kBaseDynLpMax * speedFactor, sr * 0.45f);

    // Speed-adaptive saturation onset (faster tape → later onset — wider dynamic range, cleaner signal).
    // At 7.5 ips (ref): 0.45; at 15 ips: ~0.40; at 30 ips: ~0.35.
    // Lower onset value = saturation curve begins at a lower signal level (hits sooner).
    // Higher speed tape has better HF headroom, so onset should be higher (hits later).
    // Fix: multiply rather than divide so onset rises with speed.
    // At 7.5 ips (ref): 0.45; at 15 ips: ~0.51 (clamped 0.50); at 30 ips: ~0.57 (clamped 0.50).
    {
        const float speedAdaptedOnset = 0.45f * std::sqrt(speedFactor);
        cachedSatOnset = juce::jlimit(0.30f, 0.50f, speedAdaptedOnset);
    }

    // Head bump: frequency scales with speed
    const float bumpFreq = kBaseBumpFreq * speedFactor;
    const float notchFreq = kBaseNotchFreq * speedFactor;
    headBumpCoeffs = makePeakingEQ(bumpFreq, kBumpGainDb, kBumpQ, sr);
    notchCoeffs = makePeakingEQ(notchFreq, kNotchGainDb, kNotchQ, sr);

    // Read tape standard early — needed by both HF and LF emphasis
    const int standard = tapeStandard.load(std::memory_order_relaxed);

    // HF emphasis: NAB uses 50µs (3183Hz), IEC uses speed-dependent time constants
    const float empGainDb = kBaseEmpGainDb / speedFactor;
    float empFreq = 3183.0f;  // NAB default (50µs)
    if (standard == 1)  // IEC
    {
        // IEC time constants: 7.5ips=90µs(1768Hz), 15ips=70µs(2274Hz), 30ips=35µs(4547Hz)
        if (speedFactor <= 1.01f) empFreq = 1768.0f;
        else if (speedFactor <= 2.01f) empFreq = 2274.0f;
        else empFreq = 4547.0f;
    }
    preEmpCoeffs = makeHighShelf(empFreq, std::max(1.0f, empGainDb), 0.707f, sr);
    deEmpCoeffs = makeHighShelf(empFreq, -std::max(1.0f, empGainDb), 0.707f, sr);

    // NAB LF shelf (3180µs time constant → turnover at ~50Hz)
    // Applies at all speeds when NAB standard; gain scales with 1/speedFactor
    // (6dB at 7.5 ips, ~3dB at 15 ips, ~1.5dB at 30 ips). IEC has no LF pre-emphasis.
    if (standard == 0) // NAB at all speeds
    {
        nabLfPreCoeffs = makeFirstOrderShelf(50.0f,  empGainDb, sr);
        nabLfDeCoeffs  = makeFirstOrderShelf(50.0f, -empGainDb, sr);
    }
    else
    {
        // IEC mode: bypass LF shelf
        nabLfPreCoeffs = { 1, 0, 0, 0, 0 };
        nabLfDeCoeffs = { 1, 0, 0, 0, 0 };
    }

    // Gap loss: sinc comb with first null at f = v / (2*g), higher at faster speeds
    // D = sr * g / v (fractional gap delay in samples)
    gapDelaySamples = static_cast<float>(sampleRate) * kHeadGapM
                      / (speed * 0.0254f);

    // Hiss character peak: frequency scales with speed (oxide noise shifts up at higher speeds)
    {
        const float f0 = std::min(kHissPeakFreq * speedFactor, sr * 0.45f);
        const float Q  = kHissPeakQ;
        const float w0    = kTwoPi * f0 / sr;
        const float alpha = std::sin(w0) / (2.0f * Q);
        const float a0    = 1.0f + alpha;
        hissCoeffs = { alpha / a0, 0.0f, -alpha / a0,
                       (-2.0f * std::cos(w0)) / a0, (1.0f - alpha) / a0 };
    }

    // Reset dynamic LP to new max (coefficients are channel-independent)
    dynLpCoeffs = makeButterworthLP(dynLpMax, sr);
    // NOTE: biquad filter states are intentionally NOT reset here.
    // Resetting states on a coefficient-only change causes two problems:
    //   1. Click: zeroing a state mid-signal creates a hard discontinuity in filter output.
    //   2. Volume dip: the pre-emphasis filter (nabLfPre, preEmp) needs ~200-400ms to
    //      re-settle from zero, during which de-emphasis is fully applied to an under-
    //      pre-emphasized signal — producing net HF/LF attenuation until states refill.
    // IIR biquad states adapt smoothly to new coefficients within 1-2 samples; no reset needed.
}

// ============================================================================
// Prepare / Reset
// ============================================================================

void TapeEngine::resetChannelState(int ch)
{
    std::fill(std::begin(delayBuffer[ch]), std::end(delayBuffer[ch]), 0.0f);
    wowNoiseState[ch] = 0.0f;
    flutterNoiseState[ch] = 0.0f;
    headBumpState[ch] = {};
    notchState[ch] = {};
    preEmpState[ch] = {};
    deEmpState[ch] = {};
    nabLfPreState[ch] = {};
    nabLfDeState[ch] = {};
    hissFilterState[ch] = {};
    dynLpBqState[ch] = {};
    std::fill(gapBuf[ch], gapBuf[ch] + kGapBufSize, 0.0f);
    glueCohesionEnv[ch] = 0.0f;
    dcBlockX1[ch] = 0.0f;
    dcBlockY1[ch] = 0.0f;
    hystState[ch] = {};
    hystStateLF[ch] = {};
    hystStateHF[ch] = {};
    xoverLF500State[ch]  = {};
    xoverHF5kState[ch]   = {};
    midBandPrev[ch] = 0.0f;
    lfBandPrev[ch] = 0.0f;
    hfBandPrev[ch] = 0.0f;
    compEnvState[ch] = 0.0f;
    hfEnvState[ch] = 0.0f;
    hissPinkState[ch] = 0.0f;
    std::fill(std::begin(hb1aHistory[ch]), std::end(hb1aHistory[ch]), 0.0f);
    std::fill(std::begin(hb1bHistory[ch]), std::end(hb1bHistory[ch]), 0.0f);
    std::fill(std::begin(hb1cHistory[ch]), std::end(hb1cHistory[ch]), 0.0f);
    std::fill(std::begin(hb1dHistory[ch]), std::end(hb1dHistory[ch]), 0.0f);
    std::fill(std::begin(hb2aHistory[ch]), std::end(hb2aHistory[ch]), 0.0f);
    std::fill(std::begin(hb2bHistory[ch]), std::end(hb2bHistory[ch]), 0.0f);
    std::fill(std::begin(hb3History[ch]), std::end(hb3History[ch]), 0.0f);
    std::fill(std::begin(hbLFaHistory[ch]),  std::end(hbLFaHistory[ch]),  0.0f);
    std::fill(std::begin(hbLFbHistory[ch]),  std::end(hbLFbHistory[ch]),  0.0f);
    std::fill(std::begin(hbLFHistory2[ch]),  std::end(hbLFHistory2[ch]),  0.0f);
    std::fill(std::begin(hbHFaHistory[ch]),  std::end(hbHFaHistory[ch]),  0.0f);
    std::fill(std::begin(hbHFbHistory[ch]),  std::end(hbHFbHistory[ch]),  0.0f);
    std::fill(std::begin(hbHFHistory2[ch]),  std::end(hbHFHistory2[ch]),  0.0f);
    std::fill(std::begin(printThroughBuf[ch]), std::end(printThroughBuf[ch]), 0.0f);
}

void TapeEngine::prepare(double newSampleRate, int /*samplesPerBlock*/)
{
    sampleRate = newSampleRate;
    const float sr = static_cast<float>(sampleRate);

    wowSmoothed.reset(sampleRate, 0.02);
    flutterSmoothed.reset(sampleRate, 0.02);
    hissSmoothed.reset(sampleRate, 0.02);
    biasSmoothed.reset(sampleRate, 0.05);
    driveGainSmoothed.reset(sampleRate, 0.02);
    mixSmoothed.reset(sampleRate, 0.02);
    tapeOutGainSmoothed.reset(sampleRate, 0.02);
    inputGainSmoothed.reset(sampleRate, 0.02);

    for (int ch = 0; ch < kMaxChannels; ++ch) resetChannelState(ch);
    writePos = 0;
    sampleCounter = 0;
    printThroughWritePos = 0;
    printThroughDelaySamples = std::max(1, std::min(kPrintThroughBufSize - 1,
        static_cast<int>(0.003f * sr))); // 3ms

    // Initialize oscillators (wow frequencies set by recomputeSpeedCoeffs below)
    wow1Osc.init(kBaseCapstanHz, sr);
    wow2Osc.init(kBasePinchHz, sr);
    wow3Osc.init(kBaseReelHz, sr);
    flutter1Osc.init(kFlutter1Hz, sr);
    flutter2Osc.init(kFlutter2Hz, sr);

    // Pre-compute per-channel phase offset trig for ALL oscillators
    for (int ch = 0; ch < kMaxChannels; ++ch)
    {
        wowOffsetCos[ch] = std::cos(kWowOffset[ch]);
        wowOffsetSin[ch] = std::sin(kWowOffset[ch]);
        wow2OffsetCos[ch] = std::cos(kWow2Offset[ch]);
        wow2OffsetSin[ch] = std::sin(kWow2Offset[ch]);
        wow3OffsetCos[ch] = std::cos(kWow3Offset[ch]);
        wow3OffsetSin[ch] = std::sin(kWow3Offset[ch]);
        flutterOffsetCos[ch] = std::cos(kFlutterOffset[ch]);
        flutterOffsetSin[ch] = std::sin(kFlutterOffset[ch]);
        flutter2OffsetCos[ch] = std::cos(kFlutter2Offset[ch]);
        flutter2OffsetSin[ch] = std::sin(kFlutter2Offset[ch]);
    }

    cachedWowSqrt = 0.0f;
    cachedFlutterSqrt = 0.0f;
    cachedDriveGain = 1.0f;
    cachedDriveMakeup = 1.0f;

    wowNoiseAlpha = 1.0f - std::exp(-kTwoPi * 3.0f / sr);
    flutterNoiseAlpha = 1.0f - std::exp(-kTwoPi * 25.0f / sr);
    compAttackAlpha = 1.0f - std::exp(-kTwoPi * 200.0f / sr);   // ~0.8ms attack
    compReleaseAlpha = 1.0f - std::exp(-kTwoPi * 3.0f / sr);    // ~53ms release
    glueAttackAlpha = 1.0f - std::exp(-kTwoPi * 2.5f / sr);     // ~64ms attack (τ = 1/(2π×2.5))
    glueReleaseAlpha = 1.0f - std::exp(-kTwoPi * 2.2f / sr);   // ~72ms release (τ = 1/(2π×2.2))
    dcBlockCoeff = std::exp(-kTwoPi * 8.0f / sr);  // ~8Hz HP — preserves sub-bass while blocking DC

    // Compute all speed-dependent coefficients (includes hiss filter)
    recomputeSpeedCoeffs();
    recomputeCrossoverCoeffs();
    hissSpeedIpsStored = -1.0f;
    setHissSpeedIps(currentSpeed.load());
}

void TapeEngine::resetState()
{
    for (int ch = 0; ch < kMaxChannels; ++ch) resetChannelState(ch);
    writePos = 0;
    gapWritePos = 0;
    sampleCounter = 0;
    printThroughWritePos = 0;
}

void TapeEngine::setWow(float a) { if (currentWow != a) { currentWow = a; wowSmoothed.setTargetValue(a); } }
void TapeEngine::setFlutter(float a) { if (currentFlutter != a) { currentFlutter = a; flutterSmoothed.setTargetValue(a); } }
void TapeEngine::setHiss(float a) { if (currentHiss != a) { currentHiss = a; hissSmoothed.setTargetValue(a); } }
void TapeEngine::setBias(float a) { if (currentBias != a) { currentBias = a; biasSmoothed.setTargetValue(a); } }
void TapeEngine::setDrive(float a)
{
    if (currentDrive != a)
    {
        currentDrive = a;
        // Smoothstep curve: sweet spot at 30-50% knob, aggressive bloom above 70%
        const float shaped = a * a * (3.0f - 2.0f * a);
        const float db = shaped * kDriveMaxDb;
        driveGainSmoothed.setTargetValue(std::pow(10.0f, db / 20.0f));
    }
}

void TapeEngine::setMix(float a) { if (currentMix != a) { currentMix = a; mixSmoothed.setTargetValue(a); } }

void TapeEngine::setInputGain(float db)
{
    if (currentInputDb != db)
    {
        currentInputDb = db;
        inputGainSmoothed.setTargetValue(std::pow(10.0f, db / 20.0f));
    }
}

void TapeEngine::setHissSpeedIps(float ips)
{
    ips = juce::jlimit(7.5f, 30.0f, ips);
    if (hissSpeedIpsStored == ips)
        return;
    hissSpeedIpsStored = ips;
    // 0.75-power scaling: 9 dB range across full speed span (4.5 dB per speed octave).
    // More realistic than sqrt (6 dB) — 7.5 ips is clearly noisier than 30 ips.
    cachedHissSpeedScale.store(std::pow(kRefSpeed / ips, 0.75f), std::memory_order_relaxed);
}

void TapeEngine::setTapeOutputDb(float db)
{
    if (currentTapeOutDb != db)
    {
        currentTapeOutDb = db;
        tapeOutGainSmoothed.setTargetValue(std::pow(10.0f, db / 20.0f));
    }
}

void TapeEngine::setFormulation(int index)
{
    pendingFormulation.store(std::max(0, std::min(index, 4)), std::memory_order_relaxed);
}

void TapeEngine::setSpeed(float speedIps)
{
    speedIps = juce::jlimit(7.5f, 30.0f, speedIps);
    if (currentSpeed.load() != speedIps)
    {
        currentSpeed.store(speedIps);
        speedChanged.store(true);
    }
}

void TapeEngine::setStandard(int index)
{
    if (tapeStandard.load() != index)
    {
        tapeStandard.store(index);
        standardChanged.store(true);
    }
}

void TapeEngine::setPrintThrough(float level)
{
    printThroughLevel.store(juce::jlimit(0.0f, 1.0f, level), std::memory_order_relaxed);
}

void TapeEngine::setMotorEnabled(bool enabled)
{
    if (motorEnabled.load(std::memory_order_relaxed) == enabled) return;
    motorEnabled.store(enabled, std::memory_order_relaxed);
    // Ramp over ~2 seconds
    const float rampSamples = static_cast<float>(sampleRate) * 2.0f;
    motorRampInc.store((enabled ? 1.0f : -1.0f) / std::max(1.0f, rampSamples),
                       std::memory_order_relaxed);
}

float TapeEngine::readHermite(int channel, float delaySamples) const
{
    const float readPos   = static_cast<float>(writePos) - delaySamples;
    const float readFloor = std::floor(readPos);
    const int idx1 = static_cast<int>(readFloor) & (kDelayBufferSize - 1);
    const int idx0 = (idx1 - 1 + kDelayBufferSize) & (kDelayBufferSize - 1);
    const int idx2 = (idx1 + 1) & (kDelayBufferSize - 1);
    const int idx3 = (idx1 + 2) & (kDelayBufferSize - 1);
    const float frac = readPos - readFloor;
    const float y0 = delayBuffer[channel][idx0], y1 = delayBuffer[channel][idx1];
    const float y2 = delayBuffer[channel][idx2], y3 = delayBuffer[channel][idx3];
    const float c1 = 0.5f * (y2 - y0);
    const float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
    const float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
    return ((c3 * frac + c2) * frac + c1) * frac + y1;
}

// ============================================================================
// 15-tap Kaiser half-band polyphase decimation helper (~90dB stopband rejection)
// ============================================================================
static float halfBandDecimate(const float in0, const float in1, float history[8])
{
    history[0] = history[1];
    history[1] = history[2];
    history[2] = history[3];
    history[3] = history[4];
    history[4] = history[5];
    history[5] = history[6];
    history[6] = history[7];
    history[7] = in0;
    const float even = -0.001586f * (history[0] + history[7])
                     +  0.011750f * (history[1] + history[6])
                     + -0.052800f * (history[2] + history[5])
                     +  0.295636f * (history[3] + history[4]);
    return even + 0.5f * in1;
}

// ============================================================================
// Extracted process helpers
// ============================================================================

float TapeEngine::computeModDelay(int ch, float wowAmt, float flutterAmt)
{
    // Per-channel phase offsets on ALL wow/flutter oscillators (full stereo decorrelation)
    const float chWow1Sin = wow1Osc.sinVal * wowOffsetCos[ch]
                          + wow1Osc.cosVal * wowOffsetSin[ch];
    const float chWow2Sin = wow2Osc.sinVal * wow2OffsetCos[ch]
                          + wow2Osc.cosVal * wow2OffsetSin[ch];
    const float chWow3Sin = wow3Osc.sinVal * wow3OffsetCos[ch]
                          + wow3Osc.cosVal * wow3OffsetSin[ch];
    const float chWow = chWow1Sin * kCapstanWeight
                      + chWow2Sin * kPinchWeight
                      + chWow3Sin * kReelWeight;
    const float wowRand = fastNoise(noiseState[ch]);
    wowNoiseState[ch] += wowNoiseAlpha * (wowRand - wowNoiseState[ch]);
    // Normalize sum to prevent over-additive depth
    const float wowModSum = (chWow + wowNoiseState[ch] * kNoiseModDepth)
                          / (kCapstanWeight + kPinchWeight + kReelWeight + kNoiseModDepth);
    const float wowDelay = wowAmt * wowDepth * wowModSum;

    const float chFlutter1Sin = flutter1Osc.sinVal * flutterOffsetCos[ch]
                              + flutter1Osc.cosVal * flutterOffsetSin[ch];
    const float chFlutter2Sin = flutter2Osc.sinVal * flutter2OffsetCos[ch]
                              + flutter2Osc.cosVal * flutter2OffsetSin[ch];
    const float flutterRand = fastNoise(flutterPrngState[ch]);
    flutterNoiseState[ch] += flutterNoiseAlpha * (flutterRand - flutterNoiseState[ch]);
    const float flutterModSum = (chFlutter1Sin * kFlutterPrimaryWeight
                               + chFlutter2Sin * kFlutterSecondaryWeight
                               + flutterNoiseState[ch] * kNoiseModDepth)
                              / (kFlutterPrimaryWeight + kFlutterSecondaryWeight + kNoiseModDepth);
    const float flutterDelay = flutterAmt * flutterDepth * flutterModSum;

    // Minimum 3 samples: readHermite accesses idx3 = floor(readPos)+2, which must
    // stay behind writePos. delay<3 would expose an unwritten (stale) buffer slot.
    return std::max(3.0f, kBaseDelay + wowDelay + flutterDelay);
}

float TapeEngine::processHysteresisBlock(int ch, float input)
{
    // --- Frequency-dependent J-A: split into 3 bands with distinct saturation curves ---
    // LP + HP complementary pairs guarantee perfect reconstruction (LP + HP == input).
    // LF (<500Hz): easier saturation — more magnetic material resonates at low frequencies.
    // MID (500Hz–5kHz): nominal parameters — the primary character band.
    // HF (>5kHz): less saturation — high frequencies are harder to drive into the tape.

    // Band splitting via complementary LP/HP filters
    const float lfBand  = processBiquad(input, xoverLF500Coeffs, xoverLF500State[ch]);
    const float lfmBand = processBiquad(input, xoverHF5kCoeffs,  xoverHF5kState[ch]);
    const float midBand = lfmBand - lfBand;   // 500Hz–5kHz (complementary subtract)
    const float hfBand  = input   - lfmBand;  // >5kHz       (complementary subtract)

    // --- MID band: full 8× oversampled J-A (nominal parameters, main character band) ---
    // Catmull-Rom Hermite upsampling: central-difference tangent at h0, forward-diff at h1.
    //   m0 = (h1 - h_{-1}) * 0.5  — central diff at h0 (using stored prev-prev value)
    //   m1 = (h1 - h0)    * 0.5  — forward diff at h1 (causal; best available approximation)
    // This gives proper Catmull-Rom character vs the old single-tangent symmetric cubic.
    const float h0 = hystState[ch].H_prev;  // sample n-1
    const float h1 = midBand;               // sample n (mid band)
    const float m0 = (h1 - midBandPrev[ch]) * 0.5f; // central diff at h0
    const float m1 = (h1 - h0) * 0.5f;              // forward diff at h1
    midBandPrev[ch] = h0;
    float os[kOversample];
    for (int j = 0; j < kOversample; ++j)
    {
        const float t = static_cast<float>(j + 1) / static_cast<float>(kOversample);
        const float t2 = t * t;
        const float t3 = t2 * t;
        const float h00 = 2.0f * t3 - 3.0f * t2 + 1.0f;
        const float h10 = t3 - 2.0f * t2 + t;
        const float h01 = -2.0f * t3 + 3.0f * t2;
        const float h11 = t3 - t2;
        const float H_interp = h00 * h0 + h10 * m0 + h01 * h1 + h11 * m1;
        os[j] = hystRK4Step(H_interp, hystState[ch], hystMs, hystK);
    }
    const float hb1a = halfBandDecimate(os[0], os[1], hb1aHistory[ch]);
    const float hb1b = halfBandDecimate(os[2], os[3], hb1bHistory[ch]);
    const float hb1c = halfBandDecimate(os[4], os[5], hb1cHistory[ch]);
    const float hb1d = halfBandDecimate(os[6], os[7], hb1dHistory[ch]);
    const float hb2a = halfBandDecimate(hb1a, hb1b, hb2aHistory[ch]);
    const float hb2b = halfBandDecimate(hb1c, hb1d, hb2bHistory[ch]);
    const float midOut = halfBandDecimate(hb2a, hb2b, hb3History[ch]);

    // --- LF band (<500Hz): easier saturation (warm, low-end glue) ---
    // K ×0.70 lowers coercivity (smaller denominator in dMirr → more sensitive, earlier onset).
    // Ms ×1.25 raises the saturation ceiling so LF compresses with character rather than clipping.
    // Per-band ms/k are passed directly to hystRK4Step — no member mutation needed.
    const float lfMs = hystMs * 1.25f, lfK = hystK * 0.70f;
    const float lf_h0 = hystStateLF[ch].H_prev;
    const float lf_h1 = lfBand;
    const float lf_m0 = (lf_h1 - lfBandPrev[ch]) * 0.5f; // central diff at h0
    const float lf_m1 = (lf_h1 - lf_h0) * 0.5f;          // forward diff at h1
    lfBandPrev[ch] = lf_h0;
    float osLF[kOs4x];
    for (int j = 0; j < kOs4x; ++j)
    {
        const float t  = static_cast<float>(j + 1) / static_cast<float>(kOs4x);
        const float t2 = t * t, t3 = t2 * t;
        const float H_interp = (2*t3-3*t2+1)*lf_h0 + (t3-2*t2+t)*lf_m0
                             + (-2*t3+3*t2)*lf_h1  + (t3-t2)*lf_m1;
        osLF[j] = hystRK4Step(H_interp, hystStateLF[ch], lfMs, lfK);
    }
    const float hbLF_a = halfBandDecimate(osLF[0], osLF[1], hbLFaHistory[ch]);
    const float hbLF_b = halfBandDecimate(osLF[2], osLF[3], hbLFbHistory[ch]);
    const float lfOut_4x = halfBandDecimate(hbLF_a, hbLF_b, hbLFHistory2[ch]);

    // --- HF band (>5kHz): frequency-dependent saturation (C6) ---
    // Higher K = harder to magnetize (larger denominator in dMirr). Baseline 1.25 so HF
    // starts harder to drive than MID. When HF energy is high, K drops toward 0.80 —
    // modelling how driven HF compresses (still above 0 to keep the ODE stable).
    hfEnvState[ch] += kHFEnvAlpha * (std::abs(hfBand) - hfEnvState[ch]);
    const float kHFMod = 1.25f - 0.45f * std::min(1.0f, hfEnvState[ch] * 2.0f); // 1.25→0.80
    const float hfMs = hystMs * 0.75f, hfK = hystK * kHFMod;
    const float hf_h0 = hystStateHF[ch].H_prev;
    const float hf_h1 = hfBand;
    const float hf_m0 = (hf_h1 - hfBandPrev[ch]) * 0.5f; // central diff at h0
    const float hf_m1 = (hf_h1 - hf_h0) * 0.5f;          // forward diff at h1
    hfBandPrev[ch] = hf_h0;
    float osHF[kOs4x];
    for (int j = 0; j < kOs4x; ++j)
    {
        const float t  = static_cast<float>(j + 1) / static_cast<float>(kOs4x);
        const float t2 = t * t, t3 = t2 * t;
        const float H_interp = (2*t3-3*t2+1)*hf_h0 + (t3-2*t2+t)*hf_m0
                             + (-2*t3+3*t2)*hf_h1  + (t3-t2)*hf_m1;
        osHF[j] = hystRK4Step(H_interp, hystStateHF[ch], hfMs, hfK);
    }
    const float hbHF_a = halfBandDecimate(osHF[0], osHF[1], hbHFaHistory[ch]);
    const float hbHF_b = halfBandDecimate(osHF[2], osHF[3], hbHFbHistory[ch]);
    const float hfOut_4x = halfBandDecimate(hbHF_a, hbHF_b, hbHFHistory2[ch]);

    return lfOut_4x + midOut + hfOut_4x;
}

float TapeEngine::processSoftSat(float input, int ch)
{
    // Update compression envelope (asymmetric peak follower)
    const float absLevel = std::abs(input);
    const float compAlpha = (absLevel > compEnvState[ch]) ? compAttackAlpha : compReleaseAlpha;
    compEnvState[ch] += compAlpha * (absLevel - compEnvState[ch]);

    // Dynamic onset: louder signal → earlier saturation → tape compression character
    // cachedSatOnset is speed-adaptive (lower at 30 ips, higher at 7.5 ips)
    const float dynamicOnset = std::max(0.3f, cachedSatOnset - compEnvState[ch] * 0.2f);

    if (absLevel <= dynamicOnset) return input;
    const float over = std::min(1.0f, (absLevel - dynamicOnset) / kSatTransition);
    const float knee = over * over * (3.0f - 2.0f * over);
    return input * (1.0f - knee) + fastTanh(input) * knee;
}

void TapeEngine::processHissBlock(int ch, float hissAmt, float& wet)
{
    const float white = fastNoise(hissPrngState[ch]);
    // 1-pole pink shaper (Kellett 2002 stage 1): slopes noise spectrum -3dB/oct below ~500Hz.
    // b0 is a leaky integrator; combined (b0 + direct) gives 1/f tilt up to ~500Hz, then flat.
    // Normalised at character peak 2250 Hz: |H_pink(2250Hz)| = 0.3248, factor = 1/0.3248 = 1.78.
    // (Using broadband-RMS normalisation 0.685 would attenuate the bandpass output by ~13 dB.)
    hissPinkState[ch] = 0.99765f * hissPinkState[ch] + white * 0.09905f;
    const float pink = (hissPinkState[ch] + white * 0.0555f) * 1.78f;
    // Speed-scaled character bandpass applied to the pink-shaped noise
    wet += processBiquad(pink * hissAmt * kHissGain * cachedHissSpeedScale.load(std::memory_order_relaxed),
                         hissCoeffs, hissFilterState[ch]);
}

// ============================================================================
// Process
// ============================================================================

void TapeEngine::process(juce::AudioBuffer<float>& buffer)
{
    juce::ScopedNoDenormals noDenormals;

    // Apply pending formulation change on audio thread (thread-safe via atomic)
    {
        const int pf = pendingFormulation.load(std::memory_order_relaxed);
        if (pf != currentFormulation)
        {
            currentFormulation = pf;
            const auto& f = kFormulations[pf];
            hystMs = f.ms; hystAlpha = f.alpha; hystC = f.c;
            hystBaseA = f.baseA; hystBaseK = f.baseK;
            // Immediately derive hystA/hystK and cachedBiasNorm from the new base values
            // so the first sample after the change uses correct J-A parameters.
            // (Without this, the 16-sample cache batch would leave them stale for up to 15 samples.)
            {
                const float biasSqrt = std::sqrt(biasSmoothed.getCurrentValue());
                hystK = std::max(0.05f, hystBaseK * (0.3f + biasSqrt * 0.9f));
                hystA = std::max(0.05f, hystBaseA * (0.65f + biasSqrt * 0.55f));
                // Exact small-signal inverse: G_JA = hystC*Ms/(3A) → compensation = 3A/(c*Ms)
                cachedBiasNorm = 3.0f * hystA / (hystC * hystMs);
            }
            // Reset J-A state and half-band histories: old magnetization and interpolation
            // history are invalid under the new formulation's parameters and would produce
            // a click if carried over.
            for (int ch = 0; ch < kMaxChannels; ++ch)
            {
                hystState[ch] = {};
                hystStateLF[ch] = {};
                hystStateHF[ch] = {};
                std::fill(std::begin(hb1aHistory[ch]),  std::end(hb1aHistory[ch]),  0.0f);
                std::fill(std::begin(hb1bHistory[ch]),  std::end(hb1bHistory[ch]),  0.0f);
                std::fill(std::begin(hb1cHistory[ch]),  std::end(hb1cHistory[ch]),  0.0f);
                std::fill(std::begin(hb1dHistory[ch]),  std::end(hb1dHistory[ch]),  0.0f);
                std::fill(std::begin(hb2aHistory[ch]),  std::end(hb2aHistory[ch]),  0.0f);
                std::fill(std::begin(hb2bHistory[ch]),  std::end(hb2bHistory[ch]),  0.0f);
                std::fill(std::begin(hb3History[ch]),   std::end(hb3History[ch]),   0.0f);
                std::fill(std::begin(hbLFaHistory[ch]), std::end(hbLFaHistory[ch]), 0.0f);
                std::fill(std::begin(hbLFbHistory[ch]), std::end(hbLFbHistory[ch]), 0.0f);
                std::fill(std::begin(hbLFHistory2[ch]), std::end(hbLFHistory2[ch]), 0.0f);
                std::fill(std::begin(hbHFaHistory[ch]), std::end(hbHFaHistory[ch]), 0.0f);
                std::fill(std::begin(hbHFbHistory[ch]), std::end(hbHFbHistory[ch]), 0.0f);
                std::fill(std::begin(hbHFHistory2[ch]), std::end(hbHFHistory2[ch]), 0.0f);
            }
        }
    }

    // Recompute speed-dependent coefficients if speed or standard changed
    if (speedChanged.load() || standardChanged.load())
    {
        recomputeSpeedCoeffs();
        speedChanged.store(false);
        standardChanged.store(false);
    }

    const auto numChannels = buffer.getNumChannels();
    const auto numSamples = buffer.getNumSamples();

    // Early exit: skip the full DSP chain when mix is effectively zero.
    // Oscillators must still advance to prevent a phase discontinuity (click/pitch artifact)
    // when mix is re-enabled after a period of bypass.
    // SmoothedValues must also advance so their ramps stay in sync with wall-clock time —
    // failing to do so causes zipper noise when parameters are changed during bypass and
    // mix later returns above the threshold.
    if (mixSmoothed.getCurrentValue() < 0.001f && mixSmoothed.getTargetValue() < 0.001f)
    {
        for (int i = 0; i < numSamples; ++i)
        {
            wowSmoothed.getNextValue();
            flutterSmoothed.getNextValue();
            hissSmoothed.getNextValue();
            biasSmoothed.getNextValue();
            driveGainSmoothed.getNextValue();
            tapeOutGainSmoothed.getNextValue();
            inputGainSmoothed.getNextValue();
            wow1Osc.advance();
            wow2Osc.advance();
            wow3Osc.advance();
            flutter1Osc.advance();
            flutter2Osc.advance();
            if (++sampleCounter >= kOscNormalizeInterval)
            {
                sampleCounter = 0;
                wow1Osc.normalize();
                wow2Osc.normalize();
                wow3Osc.normalize();
                flutter1Osc.normalize();
                flutter2Osc.normalize();
            }
        }
        return;
    }

    const int maxSafe = juce::jmin(numSamples, kDelayBufferSize / 2);
    jassert(numSamples <= kDelayBufferSize / 2); // warn if host exceeds expected block size


    // Hoist per-block atomics — all set from UI thread, never change mid-block
    const bool  motEnabled      = motorEnabled.load(std::memory_order_relaxed);
    float       motInc          = motorRampInc.load(std::memory_order_relaxed);
    const float printThroughLvl = printThroughLevel.load(std::memory_order_relaxed);

    for (int i = 0; i < maxSafe; ++i)
    {
        const float wowRaw = wowSmoothed.getNextValue();
        const float flutterRaw = flutterSmoothed.getNextValue();
        const float hiss = hissSmoothed.getNextValue();
        const float bias = biasSmoothed.getNextValue();
        const float mix = mixSmoothed.getNextValue();
        const float tapeOutG = tapeOutGainSmoothed.getNextValue();
        cachedInputGain = inputGainSmoothed.getNextValue();

        // Drive gain: smoothed per-sample in linear domain (no zipper, no per-sample pow)
        cachedDriveGain = driveGainSmoothed.getNextValue();

        // Update cached expensive computations every N samples
        if ((i & (kCachedUpdateInterval - 1)) == 0)
        {
            cachedWowSqrt = std::sqrt(wowRaw);
            cachedFlutterSqrt = std::sqrt(flutterRaw);
            cachedDriveMakeup = std::sqrt(cachedDriveGain);
            // Hysteresis curve shape from bias: sqrt response is perceptually linear —
            // mid-range of knob has more nuanced effect than extreme ends.
            // A range: [0.65x, 1.2x] base — minimum at 65% keeps the J-A in its
            // quasi-linear regime for typical programme levels even at Glue=0,
            // preventing extreme saturation loss (UAD Oxide reference behaviour).
            // K range: [0.3x, 1.2x] base (unchanged — coercivity shaping).
            {
                const float biasSqrt = std::sqrt(bias);
                hystK = std::max(0.05f, hystBaseK * (0.3f + biasSqrt * 0.9f));
                hystA = std::max(0.05f, hystBaseA * (0.65f + biasSqrt * 0.55f));
            }
            // Bias normalisation: exact analytical inverse of J-A small-signal gain
            // G = hystC * Ms / (3 * A) → compensation = 3A / (c * Ms).
            // Level stays consistent across the full Glue range; residual saturation
            // at low bias is intentional (physical underbias character) and bounded.
            {
                cachedBiasNorm = 3.0f * hystA / (hystC * hystMs);
            }
        }

        for (int ch = 0; ch < numChannels && ch < kMaxChannels; ++ch)
        {
            const float totalDelay = computeModDelay(ch, cachedWowSqrt, cachedFlutterSqrt);

            auto* data = buffer.getWritePointer(ch);
            // Print-through: inject delayed wet output (~3ms back) into tape input only.
            // trueDry is the unmodified input used in the final dry/wet blend so print-through
            // cannot bleed into the bypass path when mix < 1.
            const float trueDry = data[i];
            const float printThruRead = printThroughBuf[ch][(printThroughWritePos - printThroughDelaySamples
                + kPrintThroughBufSize) & (kPrintThroughBufSize - 1)];
            const float inputForTape = trueDry * cachedInputGain + printThruRead * kPrintThroughCoeff * printThroughLvl;
            delayBuffer[ch][writePos] = inputForTape;
            float wet = readHermite(ch, totalDelay);

            // NAB pre-emphasis: LF shelf (3180µs) + HF shelf (50µs)
            wet = processBiquad(wet, nabLfPreCoeffs, nabLfPreState[ch]);
            wet = processBiquad(wet, preEmpCoeffs, preEmpState[ch]);

            // Drive into tape: scale input, run through J-A hysteresis + soft saturation.
            // Partial makeup (sqrt(driveGain)) is applied here to restore the drive
            // amplitude inside the chain; the remaining compensation is applied after
            // the full chain (cachedBiasNorm * kBaseChainMakeup * driveGain^0.5).
            {
                // Asymmetric DC bias: biases the J-A operating point to produce 2nd-harmonic
                // (even harmonic) character — the source of VHS-like warmth vs harsh odd harmonics.
                // Scales from 0 at Drive=0 (cachedDriveGain==1) to ~0.022 at Drive=max.
                // Residual DC after compensation is removed by the DC blocker below.
                const float asymOffset = 0.022f * (cachedDriveGain - 1.0f) / kDriveSpanAsym;
                wet *= cachedDriveGain;
                wet = processHysteresisBlock(ch, wet);
                // Asymmetric offset applied AFTER hysteresis so it adds even-harmonic
                // distortion without shifting the J-A zero-mean operating point (A6 fix).
                // processSoftSat is a nonlinear knee, so the subtraction at the end is an
                // approximate (not exact) cancellation — any residual DC (~0.022 max) is
                // removed by the DC blocker further down the chain.
                wet += asymOffset;
                wet = processSoftSat(wet, ch);
                wet /= cachedDriveMakeup;
                wet -= asymOffset / cachedDriveMakeup;
            }

            // Gap loss: sinc comb y[n] = 0.5*(x[n] + x[n-D]), nulls at f = (2k-1)*sr/(2D)
            // Playback-side physical effect — placed before de-emphasis so the reproduce EQ
            // sees the head's full response (matching real reproduce circuit design).
            {
                gapBuf[ch][gapWritePos] = wet;
                const int D_int = static_cast<int>(gapDelaySamples);
                const float D_frac = gapDelaySamples - static_cast<float>(D_int);
                const float x0 = gapBuf[ch][(gapWritePos - D_int + kGapBufSize) & kGapMask];
                const float x1 = gapBuf[ch][(gapWritePos - D_int - 1 + kGapBufSize) & kGapMask];
                wet = (wet + x0 * (1.0f - D_frac) + x1 * D_frac) * 0.5f;
            }

            // Head bump + notch (playback head resonance, speed-scaled)
            // Placed before de-emphasis so the reproduce EQ partially compensates the bump.
            wet = processBiquad(wet, headBumpCoeffs, headBumpState[ch]);
            wet = processBiquad(wet, notchCoeffs, notchState[ch]);

            // NAB de-emphasis: HF inverse + LF inverse
            wet = processBiquad(wet, deEmpCoeffs, deEmpState[ch]);
            wet = processBiquad(wet, nabLfDeCoeffs, nabLfDeState[ch]);

            // DC blocker (preserves LF NAB cancellation)
            {
                const float x = wet;
                const float y = x - dcBlockX1[ch] + dcBlockCoeff * dcBlockY1[ch];
                dcBlockX1[ch] = x;
                dcBlockY1[ch] = y;
                wet = y;
            }

            // Static HF softener: fixed LP at speed-scaled max (no dynamic update — avoids coefficient-change artifacts)
            wet = processBiquad(wet, dynLpCoeffs, dynLpBqState[ch]);

            // Makeup: bias normalisation + chain offset + sqrt(drive) + unity lift at low drive.
            // driveT=0 at min Drive (gain==1), driveT=1 at max Drive — drives lowDriveUnityLift
            // to 1.0 so the +12dB boost only applies when Drive is near zero.
            {
                const float driveT = juce::jlimit(0.0f, 1.0f,
                    (cachedDriveGain - 1.0f) / kDriveSpanAsym);
                const float lowDriveUnityLift = 1.0f + (kUnityLiftAtMinDrive - 1.0f) * (1.0f - driveT);
                wet *= cachedBiasNorm * kBaseChainMakeup * cachedDriveMakeup * lowDriveUnityLift;
            }

            // Glue cohesion: gentle program-dependent leveling when Glue (bias) knob past ~0.28
            {
                const float glueCohesion = juce::jmax(0.0f, bias - 0.28f) * 1.55f;
                if (glueCohesion > 0.002f)
                {
                    const float absWetGlue = std::abs(wet);
                    const float ax = (absWetGlue > glueCohesionEnv[ch]) ? glueAttackAlpha : glueReleaseAlpha;
                    glueCohesionEnv[ch] += ax * (absWetGlue - glueCohesionEnv[ch]);
                    const float gr = glueCohesion * glueCohesionEnv[ch] * 0.8f;
                    wet *= 1.0f / (1.0f + gr);
                }
            }

            // Store wet for print-through source (adjacent layer coupling)
            printThroughBuf[ch][printThroughWritePos] = wet;

            data[i] = trueDry + mix * (wet * tapeOutG - trueDry);
            // Hiss added at final output, scaled by mix and tapeOutG — noise floor tracks
            // the tape signal level (both originate from the same playback head/amp path).
            if (hiss > 0.001f)
                processHissBlock(ch, hiss * mix * tapeOutG, data[i]);
        }

        // Motor ramp: multiply all channel outputs by motorRamp, then advance it
        if (std::abs(motorRamp - (motEnabled ? 1.0f : 0.0f)) > 1e-5f || motInc != 0.0f)
        {
            for (int ch = 0; ch < numChannels && ch < kMaxChannels; ++ch)
                buffer.getWritePointer(ch)[i] *= motorRamp;
            motorRamp = juce::jlimit(0.0f, 1.0f, motorRamp + motInc);
            if (motorRamp <= 0.0f || motorRamp >= 1.0f)
            {
                motorRampInc.store(0.0f, std::memory_order_relaxed);
                motInc = 0.0f; // sync local copy so condition exits cleanly
            }
        }

        printThroughWritePos = (printThroughWritePos + 1) & (kPrintThroughBufSize - 1);
        writePos = (writePos + 1) & (kDelayBufferSize - 1);
        gapWritePos = (gapWritePos + 1) & kGapMask;

        // Advance oscillators (once per sample, shared across channels)
        wow1Osc.advance();
        wow2Osc.advance();
        wow3Osc.advance();
        flutter1Osc.advance();
        flutter2Osc.advance();

        // Periodic normalization to prevent magnitude drift
        if (++sampleCounter >= kOscNormalizeInterval)
        {
            sampleCounter = 0;
            wow1Osc.normalize();
            wow2Osc.normalize();
            wow3Osc.normalize();
            flutter1Osc.normalize();
            flutter2Osc.normalize();
        }
    }

    // FIX-5: if the host sent more samples than kDelayBufferSize/2 allows us to process,
    // silence the overflow region rather than leaving stale host data in the buffer.
    if (maxSafe < numSamples)
    {
        for (int ch = 0; ch < numChannels && ch < kMaxChannels; ++ch)
            juce::FloatVectorOperations::clear(buffer.getWritePointer(ch) + maxSafe,
                                               numSamples - maxSafe);
    }
}
