#include "TapeEngine.h"

// Soft saturation thresholds — lowered onset and wider knee for VHS-like gradual bloom
static constexpr float kSatOnset = 0.45f;
static constexpr float kSatTransition = 0.55f;

// Hiss parameters — gain increased for authentic tape noise floor at default 7% setting;
// reduction max raised so loud signals mask noise more convincingly (psychoacoustic masking)
static constexpr float kHissGain = 0.020f;
static constexpr float kMechanicalNoise = 0.000015f;  // motor/bearing noise (speed-independent)
static constexpr float kHissReductionScale = 1.2f;
static constexpr float kHissReductionMax = 0.45f;
static constexpr float kHissPeakFreq = 2250.0f;  // oxide character peak at 7.5 ips (scales linearly with speed)
static constexpr float kHissPeakQ = 0.6f;

// Dynamic LP update interval
static constexpr int kLpUpdateInterval = 16;
static_assert((kLpUpdateInterval & (kLpUpdateInterval - 1)) == 0, "must be power of 2");

// Modulation mixing weights (physically motivated for 1/4" transport mechanics)
static constexpr float kCapstanWeight = 0.6f;         // primary wow (capstan motor, 60%)
static constexpr float kPinchWeight = 0.2f;            // pinch roller eccentricity (20%)
static constexpr float kReelWeight = 0.2f;             // supply reel variation (20%)
static constexpr float kFlutterPrimaryWeight = 0.6f;   // 8Hz main flutter (60%)
static constexpr float kFlutterSecondaryWeight = 0.25f; // 15Hz secondary flutter (25%)
static constexpr float kNoiseModDepth = 0.3f;          // random modulation contribution

// Bias control ranges — widened for more dramatic over/under-bias contrast
static constexpr float kBiasKMin = 0.15f;   // over-biased: easier saturation (was 0.3)
static constexpr float kBiasKMax = 0.7f;    // under-biased: harder saturation (was 0.6)
static constexpr float kBiasAMax = 0.55f;   // over-biased: wider hysteresis loop (was 0.45)
static constexpr float kBiasAMin = 0.18f;   // under-biased: narrower loop (was 0.25)

// Drive control range (maps 0..1 via smoothstep to 0..+18 dB — knob=0 is unity, no attenuation)
static constexpr float kDriveMinDb = 0.0f;
static constexpr float kDriveMaxDb = 18.0f;

// Chain makeup: HF pre/de-emphasis through a nonlinear J-A is not level-neutral; a
// modest lift keeps “all knobs at 0, mix 100%” in the same ballpark as dry.
static constexpr float kBaseChainMakeup = 1.28f;
static constexpr float kDriveMakeupExp = 0.5f; // sqrt(driveGain): 1x at Drive=0, ~2x at +18dB
static constexpr float kMaxDriveLinearGain = 7.9432823f; // 10^(18/20)
static constexpr float kUnityLiftAtMinDrive = 3.9810717f; // +12 dB when cachedDriveGain == 1

// Periodic update intervals
static constexpr int kCachedUpdateInterval = 16;
static constexpr int kOscNormalizeInterval = 512;

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
    if (x > 3.5f) return 1.0f;
    if (x < -3.5f) return -1.0f;
    // 5-coefficient minimax rational — max error < 0.03% across [-3.5, 3.5]
    const float x2 = x * x;
    return x * (135135.0f + x2 * (17325.0f + x2 * 378.0f))
             / (135135.0f + x2 * (62370.0f + x2 * (3150.0f + x2 * 28.0f)));
}

float TapeEngine::fastSinh(float x)
{
    const float ax = std::abs(x);
    if (ax < 2.0f) // reduced from 3.0 to avoid 22% error at boundary
    {
        const float x2 = x * x;
        return x * (1.0f + x2 * (1.0f/6.0f + x2 * (1.0f/120.0f + x2 * (1.0f/5040.0f))));
    }
    const float e = std::exp(ax);
    const float result = (e - 1.0f / e) * 0.5f;
    return x >= 0.0f ? result : -result;
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

float TapeEngine::hystDMDH(float H, float M, float dH) const
{
    const float He = H + hystAlpha * M;
    const float Man = hystMs * langevin(He / hystA);
    const float delta = (dH >= 0.0f) ? 1.0f : -1.0f;
    const float denom = delta * hystK - hystAlpha * (Man - M);
    if (std::abs(denom) < 1e-8f) return 0.0f;
    float dMirr = (Man - M) / denom;
    if (delta > 0.0f && dMirr < 0.0f) dMirr = 0.0f;
    if (delta < 0.0f && dMirr > 0.0f) dMirr = 0.0f;
    const float dMan = hystMs * langevinDeriv(He / hystA) / hystA;
    return (1.0f - hystC) * dMirr + hystC * dMan;
}

float TapeEngine::hystRK4Step(float H, float H_prev, HysteresisState& state) const
{
    const float dH = H - H_prev;
    if (std::abs(dH) < 1e-10f) { state.H_prev = H; return state.M; }
    const float H_mid = H_prev + dH * 0.5f;
    const float k1 = hystDMDH(H_prev, state.M, dH);
    const float k2 = hystDMDH(H_mid, state.M + 0.5f * dH * k1, dH);
    const float k3 = hystDMDH(H_mid, state.M + 0.5f * dH * k2, dH);
    const float k4 = hystDMDH(H, state.M + dH * k3, dH);
    state.M += (dH / 6.0f) * (k1 + 2.0f * k2 + 2.0f * k3 + k4);
    state.M = std::max(-hystMs * 2.0f, std::min(hystMs * 2.0f, state.M));
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

static TapeEngine::BiquadCoeffs makeLowShelf(float f0, float gainDb, float Q, float sr)
{
    const float A = std::pow(10.0f, gainDb / 40.0f);
    const float w0 = TapeEngine::kTwoPi * f0 / sr;
    const float s = std::sin(w0), c = std::cos(w0);
    const float alpha = s / (2.0f * Q);
    const float sq = 2.0f * std::sqrt(A) * alpha;
    const float a0 = (A+1) + (A-1)*c + sq;
    return { (A*((A+1)-(A-1)*c+sq))/a0, (2*A*((A-1)-(A+1)*c))/a0,
             (A*((A+1)-(A-1)*c-sq))/a0, (-2*((A-1)+(A+1)*c))/a0,
             ((A+1)+(A-1)*c-sq)/a0 };
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

static constexpr float kSqrt2 = 1.41421356f;

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
    wow3Osc.setFreq(kBaseReelHz / speedFactor, sr);  // reel wow decreases at higher speed

    // Dynamic LP: max bandwidth widens at higher speeds; range is fixed (speed-independent)
    dynLpMax = std::min(kBaseDynLpMax * speedFactor, sr * 0.45f);
    dynLpRange = kBaseDynLpRange / speedFactor;  // Narrower range at higher speeds (tape compression character)

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

    // NAB LF shelf (318µs time constant → turnover at ~50Hz)
    // Applies at all speeds when NAB standard; gain scales with 1/speedFactor
    // (6dB at 7.5 ips, ~3dB at 15 ips, ~1.5dB at 30 ips). IEC has no LF pre-emphasis.
    if (standard == 0) // NAB at all speeds
    {
        const float empGainDb_lf = kBaseEmpGainDb / speedFactor;
        nabLfPreCoeffs = makeFirstOrderShelf(50.0f,  empGainDb_lf, sr);
        nabLfDeCoeffs  = makeFirstOrderShelf(50.0f, -empGainDb_lf, sr);
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
                      / (static_cast<float>(currentSpeed.load()) * 0.0254f);

    // Hiss character peak: frequency scales with speed (oxide noise shifts up at higher speeds)
    { const float f0 = std::min(kHissPeakFreq * speedFactor, sr * 0.45f), Q = kHissPeakQ;
      const float w0 = kTwoPi * f0 / sr;
      const float alpha = std::sin(w0) / (2.0f * Q);
      const float a0 = 1.0f + alpha;
      hissCoeffs = { alpha/a0, 0, -alpha/a0, (-2*std::cos(w0))/a0, (1-alpha)/a0 }; }

    // Reset dynamic LP to new max
    for (int ch = 0; ch < kMaxChannels; ++ch)
        dynLpCoeffs[ch] = makeButterworthLP(dynLpMax, sr);

    // Reset biquad filter states to prevent transient clicks from abrupt coefficient changes
    for (int ch = 0; ch < kMaxChannels; ++ch)
    {
        headBumpState[ch] = {};
        notchState[ch] = {};
        preEmpState[ch] = {};
        deEmpState[ch] = {};
        nabLfPreState[ch] = {};
        nabLfDeState[ch] = {};
        std::fill(gapBuf[ch], gapBuf[ch] + kGapBufSize, 0.0f);
        dynLpBqState[ch] = {};
        wearToneLpState[ch] = {};
    }
    gapWritePos = 0;
    std::fill(std::begin(azimBuf), std::end(azimBuf), 0.0f);
    azimWritePos = 0;
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
    wearToneLpState[ch] = {};
    glueCohesionEnv[ch] = 0.0f;
    prevH[ch] = 0.0f;
    dcBlockX1[ch] = 0.0f;
    dcBlockY1[ch] = 0.0f;
    dynLpEnvState[ch] = 0.0f;
    hystState[ch] = {};
    hystStateLF[ch] = {};   prevHLF[ch] = 0.0f;
    hystStateHF[ch] = {};   prevHHF[ch] = 0.0f;
    xoverLF500State[ch]  = {};
    xoverHF5kState[ch]   = {};
    xoverLF500bState[ch] = {};
    xoverHF5kbState[ch]  = {};
    pinkB0[ch] = 0.0f;
    pinkB1[ch] = 0.0f;
    pinkB2[ch] = 0.0f;
    pinkB3[ch] = 0.0f;
    pinkB4[ch] = 0.0f;
    compEnvState[ch] = 0.0f;
    hfEnvState[ch] = 0.0f;
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
    wearToneSmoothed.reset(sampleRate, 0.045);

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
    envAlpha = 1.0f - std::exp(-kTwoPi * 100.0f / sr);
    compAttackAlpha = 1.0f - std::exp(-kTwoPi * 200.0f / sr);   // ~0.8ms attack
    compReleaseAlpha = 1.0f - std::exp(-kTwoPi * 3.0f / sr);    // ~53ms release
    glueAttackAlpha = 1.0f - std::exp(-kTwoPi * 2.5f / sr);     // ~1ms
    glueReleaseAlpha = 1.0f - std::exp(-kTwoPi * 2.2f / sr);   // ~72ms slow release
    dcBlockCoeff = std::exp(-kTwoPi * 20.0f / sr);

    // Compute all speed-dependent coefficients (includes hiss filter)
    recomputeSpeedCoeffs();
    recomputeCrossoverCoeffs();
    wearToneLpCoeffs = makeButterworthLP(juce::jmin(22000.0f, static_cast<float>(sampleRate) * 0.45f),
                                         static_cast<float>(sampleRate));
    hissSpeedIpsStored = -1.0f;
    setHissSpeedIps(15.0f);
}

void TapeEngine::resetState()
{
    for (int ch = 0; ch < kMaxChannels; ++ch) resetChannelState(ch);
    writePos = 0;
    gapWritePos = 0;
    std::fill(std::begin(azimBuf), std::end(azimBuf), 0.0f);
    azimWritePos = 0;
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
        const float db = shaped * kDriveMaxDb;  // kDriveMinDb == 0
        driveGainSmoothed.setTargetValue(std::pow(10.0f, db / 20.0f));
    }
}

void TapeEngine::setMix(float a) { if (currentMix != a) { currentMix = a; mixSmoothed.setTargetValue(a); } }

void TapeEngine::setWearTone(float a)
{
    a = juce::jlimit(0.0f, 1.0f, a);
    if (currentWearTone != a) { currentWearTone = a; wearToneSmoothed.setTargetValue(a); }
}

void TapeEngine::setHissSpeedIps(float ips)
{
    ips = juce::jlimit(7.5f, 30.0f, ips);
    if (hissSpeedIpsStored == ips)
        return;
    hissSpeedIpsStored = ips;
    // 0.75-power scaling: 9 dB range across full speed span (4.5 dB per speed octave).
    // More realistic than sqrt (6 dB) — 7.5 ips is clearly noisier than 30 ips.
    cachedHissSpeedScale = std::pow(kRefSpeed / ips, 0.75f);
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
    printThroughLevel = juce::jlimit(0.0f, 1.0f, level);
}

void TapeEngine::setMotorEnabled(bool enabled)
{
    if (motorEnabled == enabled) return;
    motorEnabled = enabled;
    // Ramp over ~2 seconds
    const float rampSamples = static_cast<float>(sampleRate) * 2.0f;
    motorRampInc = (motorEnabled ? 1.0f : -1.0f) / std::max(1.0f, rampSamples);
}

float TapeEngine::readHermite(int channel, float delaySamples) const
{
    const float readPos = static_cast<float>(writePos) - delaySamples;
    const int idx1 = (static_cast<int>(std::floor(readPos)) & (kDelayBufferSize - 1));
    const int idx0 = (idx1 - 1 + kDelayBufferSize) & (kDelayBufferSize - 1);
    const int idx2 = (idx1 + 1) & (kDelayBufferSize - 1);
    const int idx3 = (idx1 + 2) & (kDelayBufferSize - 1);
    const float frac = readPos - std::floor(readPos);
    const float y0 = delayBuffer[channel][idx0], y1 = delayBuffer[channel][idx1];
    const float y2 = delayBuffer[channel][idx2], y3 = delayBuffer[channel][idx3];
    const float c1 = 0.5f * (y2 - y0);
    const float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
    const float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
    return ((c3 * frac + c2) * frac + c1) * frac + y1;
}

// ============================================================================
// 7-tap half-band polyphase decimation helper
// ============================================================================
static float halfBandDecimate(const float in0, const float in1, float history[4])
{
    history[0] = history[1];
    history[1] = history[2];
    history[2] = history[3];
    history[3] = in0;
    const float even = -0.03125f * (history[0] + history[3])
                     +  0.28125f * (history[1] + history[2]);
    const float odd = 0.5f * in1;
    return even + odd;
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

    return std::max(1.0f, kBaseDelay + wowDelay + flutterDelay);
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
    // Hermite (cubic) upsampling before nonlinear — avoids intermodulation aliasing
    const float h0 = hystState[ch].H_prev;  // sample n-1
    const float h1 = midBand;               // sample n (mid band)
    const float tangent0 = (h1 - prevH[ch]) * 0.5f;
    const float tangent1 = (h1 - h0) * 0.5f;
    prevH[ch] = h1;
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
        const float H_interp = h00 * h0 + h10 * tangent0 + h01 * h1 + h11 * tangent1;
        os[j] = hystRK4Step(H_interp, hystState[ch].H_prev, hystState[ch]);
    }
    const float hb1a = halfBandDecimate(os[0], os[1], hb1aHistory[ch]);
    const float hb1b = halfBandDecimate(os[2], os[3], hb1bHistory[ch]);
    const float hb1c = halfBandDecimate(os[4], os[5], hb1cHistory[ch]);
    const float hb1d = halfBandDecimate(os[6], os[7], hb1dHistory[ch]);
    const float hb2a = halfBandDecimate(hb1a, hb1b, hb2aHistory[ch]);
    const float hb2b = halfBandDecimate(hb1c, hb1d, hb2bHistory[ch]);
    const float midOut = halfBandDecimate(hb2a, hb2b, hb3History[ch]);

    // --- LF band (<500Hz): easier saturation (warm, low-end glue) ---
    // 4× oversampled J-A: Hermite upsample → 4× RK4 → 1-stage half-band decimate.
    // Ms ×1.25 and K ×1.15 lower the coercivity threshold — LF saturates earlier.
    const float savedMs = hystMs, savedK = hystK;
    hystMs *= 1.25f;
    hystK  *= 1.15f;
    {
        static constexpr int kOsLF = 4;
        const float lf_h0 = hystStateLF[ch].H_prev;
        const float lf_tangent0 = (lfBand - prevHLF[ch]) * 0.5f;
        const float lf_tangent1 = (lfBand - lf_h0) * 0.5f;
        prevHLF[ch] = lfBand; // A8 fix: store current sample (n), not H_prev (n-2)
        float osLF[kOsLF];
        for (int j = 0; j < kOsLF; ++j)
        {
            const float t  = static_cast<float>(j + 1) / static_cast<float>(kOsLF);
            const float t2 = t * t;
            const float t3 = t2 * t;
            const float h00 = 2.0f * t3 - 3.0f * t2 + 1.0f;
            const float h10 = t3 - 2.0f * t2 + t;
            const float h01 = -2.0f * t3 + 3.0f * t2;
            const float h11 = t3 - t2;
            const float H_interp = h00 * lf_h0 + h10 * lf_tangent0
                                 + h01 * lfBand + h11 * lf_tangent1;
            osLF[j] = hystRK4Step(H_interp, hystStateLF[ch].H_prev, hystStateLF[ch]);
        }
        // 2-stage half-band decimate: 4× → 2× (stage 1) → 1× (stage 2)
        const float hbLF_a = halfBandDecimate(osLF[0], osLF[1], hbLFaHistory[ch]);
        const float hbLF_b = halfBandDecimate(osLF[2], osLF[3], hbLFbHistory[ch]);
        const float lfOut_4x = halfBandDecimate(hbLF_a, hbLF_b, hbLFHistory2[ch]);
        // Restore nominal parameters before HF block
        hystMs = savedMs;
        hystK  = savedK;

        // --- HF band (>5kHz): frequency-dependent saturation (C6) ---
        // Real tape: HF saturates more aggressively at high HF levels (lower coercivity threshold).
        // 8-sample HF envelope follower modulates K downward when HF energy is present.
        hfEnvState[ch] += kHFEnvAlpha * (std::abs(hfBand) - hfEnvState[ch]);
        const float kHFMod = 0.85f - 0.20f * std::min(1.0f, hfEnvState[ch] * 2.0f); // 0.85→0.65
        hystMs *= 0.75f;
        hystK  *= kHFMod;
        const float hf_h0 = hystStateHF[ch].H_prev;
        const float hf_tangent0 = (hfBand - prevHHF[ch]) * 0.5f;
        const float hf_tangent1 = (hfBand - hf_h0) * 0.5f;
        prevHHF[ch] = hfBand; // A8 fix: store current sample (n), not H_prev (n-2)
        float osHF[kOsLF];
        for (int j = 0; j < kOsLF; ++j)
        {
            const float t  = static_cast<float>(j + 1) / static_cast<float>(kOsLF);
            const float t2 = t * t;
            const float t3 = t2 * t;
            const float h00 = 2.0f * t3 - 3.0f * t2 + 1.0f;
            const float h10 = t3 - 2.0f * t2 + t;
            const float h01 = -2.0f * t3 + 3.0f * t2;
            const float h11 = t3 - t2;
            const float H_interp = h00 * hf_h0 + h10 * hf_tangent0
                                 + h01 * hfBand + h11 * hf_tangent1;
            osHF[j] = hystRK4Step(H_interp, hystStateHF[ch].H_prev, hystStateHF[ch]);
        }
        const float hbHF_a = halfBandDecimate(osHF[0], osHF[1], hbHFaHistory[ch]);
        const float hbHF_b = halfBandDecimate(osHF[2], osHF[3], hbHFbHistory[ch]);
        const float hfOut_4x = halfBandDecimate(hbHF_a, hbHF_b, hbHFHistory2[ch]);
        hystMs = savedMs;
        hystK  = savedK;

        return lfOut_4x + midOut + hfOut_4x;
    }
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
    const float hissRed = 1.0f - std::min(1.0f, dynLpEnvState[ch] * kHissReductionScale) * kHissReductionMax;
    const float white = fastNoise(hissPrngState[ch]) * hissAmt * kHissGain * cachedHissSpeedScale * hissRed;
    pinkB0[ch] = 0.99886f * pinkB0[ch] + white * 0.0555179f;
    pinkB1[ch] = 0.99332f * pinkB1[ch] + white * 0.0750759f;
    pinkB2[ch] = 0.96900f * pinkB2[ch] + white * 0.1538520f;
    pinkB3[ch] = 0.86650f * pinkB3[ch] + white * 0.3104856f;
    pinkB4[ch] = 0.55000f * pinkB4[ch] + white * 0.5329522f;
    float pink = (pinkB0[ch] + pinkB1[ch] + pinkB2[ch] + pinkB3[ch] + pinkB4[ch] + white * 0.5362f) * 0.11f;
    pink = processBiquad(pink, hissCoeffs, hissFilterState[ch]) * 0.5f + pink * 0.5f;
    wet += pink;
    // Mechanical noise floor (motor/bearing) — constant amplitude, not speed-scaled
    wet += fastNoise(hissPrngState[ch]) * kMechanicalNoise * hissAmt;
}

// ============================================================================
// Process
// ============================================================================

void TapeEngine::process(juce::AudioBuffer<float>& buffer)
{
    juce::ScopedNoDenormals noDenormals;

    // Apply pending formulation change on audio thread (fix 3.8 — thread-safe)
    {
        const int pf = pendingFormulation.load(std::memory_order_relaxed);
        if (pf != currentFormulation)
        {
            currentFormulation = pf;
            const auto& f = kFormulations[pf];
            hystMs = f.ms; hystAlpha = f.alpha; hystC = f.c;
            hystBaseA = f.baseA; hystBaseK = f.baseK;
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

    // Early exit: skip all processing when mix is effectively zero (saves full DSP chain)
    if (mixSmoothed.getCurrentValue() < 0.001f && mixSmoothed.getTargetValue() < 0.001f)
        return;

    const float sr = static_cast<float>(sampleRate);
    const int maxSafe = juce::jmin(numSamples, kDelayBufferSize / 2);
    jassert(numSamples <= kDelayBufferSize / 2); // warn if host exceeds expected block size

    for (int i = 0; i < maxSafe; ++i)
    {
        const float wowRaw = wowSmoothed.getNextValue();
        const float flutterRaw = flutterSmoothed.getNextValue();
        const float hiss = hissSmoothed.getNextValue();
        const float bias = biasSmoothed.getNextValue();
        const float mix = mixSmoothed.getNextValue();
        const float tapeOutG = tapeOutGainSmoothed.getNextValue();
        const float wearToneAmt = wearToneSmoothed.getNextValue();

        // Drive gain: smoothed per-sample in linear domain (no zipper, no per-sample pow)
        cachedDriveGain = driveGainSmoothed.getNextValue();

        // Update cached expensive computations every N samples
        if ((i & (kCachedUpdateInterval - 1)) == 0)
        {
            cachedWowSqrt = std::sqrt(wowRaw);
            cachedFlutterSqrt = std::sqrt(flutterRaw);
            cachedDriveMakeup = std::sqrt(cachedDriveGain);
            // Speed-adaptive saturation onset: faster tape → earlier onset (HF saturates sooner)
            // At 7.5 ips (ref): 0.45; at 15 ips: ~0.40; at 30 ips: ~0.35
            {
                const float speed = currentSpeed.load();
                const float speedFactor = speed / kRefSpeed;
                const float speedAdaptedOnset = 0.45f / std::sqrt(speedFactor);
                cachedSatOnset = juce::jlimit(0.30f, 0.50f, speedAdaptedOnset);
            }
            // Hysteresis curve shape from bias (fix 3.9 — was per-sample, now every 16)
            // Sqrt response: perceptually linear — mid-range of knob has more nuanced effect
            // than extreme ends. Range: K in [0.3x, 1.2x] base, A in [0.4x, 1.2x] base.
            {
                const float biasNorm = bias;  // 0..1 from smoothed parameter
                const float biasSqrt = std::sqrt(biasNorm);
                hystK = std::max(0.05f, hystBaseK * (0.3f + biasSqrt * 0.9f));
                hystA = std::max(0.05f, hystBaseA * (0.4f + biasSqrt * 0.8f));
            }
            // Bias normalisation: ideal anhysteretic small-signal slope ~ Ms/(3*hystA).
            // Under-bias (low hystK) pushes the real RK4 J-A gain well below that;
            // sqrt(kCentre/k) restores approximate level so Glue stays “character first”.
            {
                const float kCentre = juce::jmax(0.05f, hystBaseK);
                const float kUnderBiasBoost = std::sqrt(kCentre / juce::jmax(0.05f, hystK));
                cachedBiasNorm = 3.0f * hystA * juce::jlimit(1.0f, 1.52f, kUnderBiasBoost);
            }
            {
                // Mixed linear+quadratic curve: immediately audible darkening at low wear values.
                // At 20% knob: t=0.097 (new) vs 0.040 (old) — 2.4x more tone filtering.
                // Max fc: 9.0 kHz (worn but not muffled, vs old 6.3 kHz).
                const float t = 0.35f * wearToneAmt + 0.65f * wearToneAmt * wearToneAmt;
                const float fc = 21500.0f - t * 12500.0f;
                wearToneLpCoeffs = makeButterworthLP(juce::jmax(3600.0f, fc), sr);
            }
        }

        for (int ch = 0; ch < numChannels && ch < kMaxChannels; ++ch)
        {
            const float totalDelay = computeModDelay(ch, cachedWowSqrt, cachedFlutterSqrt);

            auto* data = buffer.getWritePointer(ch);
            // Print-through: inject delayed wet output (~3ms back) into the current input (C1)
            const float printThruRead = printThroughBuf[ch][(printThroughWritePos - printThroughDelaySamples
                + kPrintThroughBufSize) & (kPrintThroughBufSize - 1)];
            const float dry = data[i] + printThruRead * kPrintThroughCoeff * printThroughLevel;
            delayBuffer[ch][writePos] = dry;
            float wet = readHermite(ch, totalDelay);

            // NAB pre-emphasis: LF shelf (318µs) + HF shelf (50µs)
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
                const float driveSpanAsym = juce::jmax(1.0e-6f, kMaxDriveLinearGain - 1.0f);
                const float asymOffset = 0.022f * (cachedDriveGain - 1.0f) / driveSpanAsym;
                wet *= cachedDriveGain;
                wet = processHysteresisBlock(ch, wet);
                // Asymmetric offset applied AFTER hysteresis so it adds even-harmonic
                // distortion without shifting the J-A zero-mean operating point (A6 fix)
                wet += asymOffset;
                wet = processSoftSat(wet, ch);
                wet /= cachedDriveMakeup;
                wet -= asymOffset / cachedDriveMakeup;
            }

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

            // Head bump + notch (speed-scaled)
            wet = processBiquad(wet, headBumpCoeffs, headBumpState[ch]);
            wet = processBiquad(wet, notchCoeffs, notchState[ch]);

            // Gap loss: sinc comb y[n] = 0.5*(x[n] + x[n-D]), nulls at f = (2k-1)*sr/(2D)
            {
                gapBuf[ch][gapWritePos] = wet;
                const int D_int = static_cast<int>(gapDelaySamples);
                const float D_frac = gapDelaySamples - static_cast<float>(D_int);
                const float x0 = gapBuf[ch][(gapWritePos - D_int + kGapBufSize) & kGapMask];
                const float x1 = gapBuf[ch][(gapWritePos - D_int - 1 + kGapBufSize) & kGapMask];
                wet = (wet + x0 * (1.0f - D_frac) + x1 * D_frac) * 0.5f;
            }

            // Per-channel dynamic LP (speed-scaled range)
            {
                const float absWet = std::abs(wet);
                dynLpEnvState[ch] += envAlpha * (absWet - dynLpEnvState[ch]);
                if ((i & (kLpUpdateInterval - 1)) == 0)
                {
                    const float envLevel = std::min(1.0f, dynLpEnvState[ch] * 2.0f);
                    dynLpCoeffs[ch] = makeButterworthLP(dynLpMax - envLevel * dynLpRange, sr);
                }
                wet = processBiquad(wet, dynLpCoeffs[ch], dynLpBqState[ch]);
            }

            // Wear tone: extra HF loss on wet (worn oxide / stretched tape) beyond wobble.
            // Two cascaded 2-pole Butterworths = 4-pole (24 dB/oct) — sharper rolloff above
            // the degradation frequency, more realistic than a single 12 dB/oct filter.
            if (wearToneAmt > 0.004f)
            {
                wet = processBiquad(wet, wearToneLpCoeffs, wearToneLpState[ch]);
                wet = processBiquad(wet, wearToneLpCoeffs, wearToneLp2State[ch]);
            }

            // Makeup: bias normalisation + chain offset + sqrt(drive) + unity lift at low drive.
            {
                const float driveSpan = juce::jmax(1.0e-6f, kMaxDriveLinearGain - 1.0f);
                const float t = juce::jlimit(0.0f, 1.0f, (cachedDriveGain - 1.0f) / driveSpan);
                const float lowDriveUnityLift = 1.0f + (kUnityLiftAtMinDrive - 1.0f) * (1.0f - t);
                wet *= cachedBiasNorm * kBaseChainMakeup * std::max(1.0f, std::pow(cachedDriveGain, kDriveMakeupExp)) * lowDriveUnityLift;
            }

            // Glue cohesion: gentle program-dependent leveling when Glue (bias) knob past ~0.28
            {
                const float glueCohesion = juce::jmax(0.0f, bias - 0.28f) * 1.55f;
                if (glueCohesion > 0.002f)
                {
                    const float absWetGlue = std::abs(wet); // PERF-6: cache abs once
                    const float ax = (absWetGlue > glueCohesionEnv[ch]) ? glueAttackAlpha : glueReleaseAlpha;
                    glueCohesionEnv[ch] += ax * (absWetGlue - glueCohesionEnv[ch]);
                    const float gr = glueCohesion * glueCohesionEnv[ch] * 0.8f;
                    wet *= 1.0f / (1.0f + gr);
                }
            }

            // Pink noise hiss added AFTER makeup (hiss is a floor noise,
            // not subject to chain-loss compensation)
            if (hiss > 0.001f)
                processHissBlock(ch, hiss, wet);

            // Azimuth misalignment: right channel delayed 0–50µs vs left (worn head tilt)
            if (ch == 1)
            {
                const float azimD = wearToneAmt * kAzimMaxUs * 1e-6f * static_cast<float>(sampleRate);
                azimBuf[azimWritePos] = wet;
                const int D_int = static_cast<int>(azimD);
                const float D_frac = azimD - static_cast<float>(D_int);
                const float ax0 = azimBuf[(azimWritePos - D_int + kAzimBufSize) & kAzimMask];
                const float ax1 = azimBuf[(azimWritePos - D_int - 1 + kAzimBufSize) & kAzimMask];
                wet = ax0 * (1.0f - D_frac) + ax1 * D_frac;
                azimWritePos = (azimWritePos + 1) & kAzimMask;
            }

            // Store wet for print-through source (adjacent layer coupling)
            printThroughBuf[ch][printThroughWritePos] = wet;

            data[i] = dry + mix * (wet * tapeOutG - dry);
        }

        // Motor ramp: multiply all channel outputs by motorRamp, then advance it
        if (std::abs(motorRamp - (motorEnabled ? 1.0f : 0.0f)) > 1e-5f || motorRampInc != 0.0f)
        {
            for (int ch = 0; ch < numChannels && ch < kMaxChannels; ++ch)
                buffer.getWritePointer(ch)[i] *= motorRamp;
            motorRamp = juce::jlimit(0.0f, 1.0f, motorRamp + motorRampInc);
            if (motorRamp <= 0.0f || motorRamp >= 1.0f)
                motorRampInc = 0.0f; // reached target, stop ramping
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
}
