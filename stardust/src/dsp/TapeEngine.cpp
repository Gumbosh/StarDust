#include "TapeEngine.h"

// Soft saturation thresholds (real tape onset ~-4dBFS, gradual knee)
static constexpr float kSatOnset = 0.6f;
static constexpr float kSatTransition = 0.4f;

// Hiss parameters
static constexpr float kHissGain = 0.012f;
static constexpr float kHissReductionScale = 1.2f;
static constexpr float kHissReductionMax = 0.3f;
static constexpr float kHissPeakFreq = 4500.0f;  // oxide character peak
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

// Drive control range (maps 0..1 to -12..+12 dB)
static constexpr float kDriveMinDb = -12.0f;
static constexpr float kDriveMaxDb = 12.0f;

// Periodic update intervals
static constexpr int kCachedUpdateInterval = 16;
static constexpr int kOscNormalizeInterval = 512;

// Per-channel phase offsets (radians) — ALL oscillators get stereo decorrelation
static constexpr float kWowOffset[2] = { 0.0f, 0.3f };
static constexpr float kWow2Offset[2] = { 0.0f, 0.45f };   // pinch roller
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
    if (x > 4.0f) return 1.0f;
    if (x < -4.0f) return -1.0f;
    const float x2 = x * x;
    return x * (27.0f + x2) / (27.0f + 9.0f * x2);
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
    // L'(x) = 1/x^2 - 1/sinh^2(x). Taylor threshold matches langevin at 0.2.
    const float ax = std::abs(x);
    if (ax < 0.2f)
    {
        const float x2 = x * x;
        return 1.0f/3.0f - x2 * (1.0f/15.0f - x2 * (2.0f/189.0f - x2 * (1.0f/675.0f)));
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

    // Hiss: better SNR at higher speeds
    hissGainScale = 1.0f / std::sqrt(speedFactor);

    // Dynamic LP: wider bandwidth at higher speeds
    dynLpMax = std::min(kBaseDynLpMax * speedFactor, sr * 0.45f);
    dynLpRange = kBaseDynLpRange * std::min(speedFactor, dynLpMax / kBaseDynLpMax);

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
    // Only applies at 7.5 ips AND NAB standard; IEC has no LF pre-emphasis
    if (standard == 0 && speedFactor <= 1.01f) // NAB at 7.5 ips
    {
        nabLfPreCoeffs = makeFirstOrderShelf(50.0f, 6.0f, sr);
        nabLfDeCoeffs = makeFirstOrderShelf(50.0f, -6.0f, sr);
    }
    else
    {
        // IEC mode or higher speeds: bypass LF shelf
        nabLfPreCoeffs = { 1, 0, 0, 0, 0 };
        nabLfDeCoeffs = { 1, 0, 0, 0, 0 };
    }

    // Gap loss: cutoff scales with speed (at 15/30 ips, pushes above Nyquist = minimal loss)
    const float gapLossFreq = kBaseGapLossFreq * speedFactor;
    gapLossCoeffs = makeButterworthLP(gapLossFreq, sr);

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
        gapLossState[ch] = {};
        dynLpBqState[ch] = {};
    }
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
    gapLossState[ch] = {};
    prevH[ch] = 0.0f;
    dcBlockX1[ch] = 0.0f;
    dcBlockY1[ch] = 0.0f;
    dynLpEnvState[ch] = 0.0f;
    hystState[ch] = {};
    pinkB0[ch] = 0.0f;
    pinkB1[ch] = 0.0f;
    pinkB2[ch] = 0.0f;
    pinkB3[ch] = 0.0f;
    pinkB4[ch] = 0.0f;
    compEnvState[ch] = 0.0f;
    std::fill(std::begin(hb1aHistory[ch]), std::end(hb1aHistory[ch]), 0.0f);
    std::fill(std::begin(hb1bHistory[ch]), std::end(hb1bHistory[ch]), 0.0f);
    std::fill(std::begin(hb2History[ch]), std::end(hb2History[ch]), 0.0f);
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

    for (int ch = 0; ch < kMaxChannels; ++ch) resetChannelState(ch);
    writePos = 0;
    sampleCounter = 0;

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
    dcBlockCoeff = std::exp(-kTwoPi * 20.0f / sr);

    { // Hiss character peak at 4.5kHz (closer to open-reel oxide spectrum)
        const float f0 = kHissPeakFreq, Q = kHissPeakQ;
        const float w0 = kTwoPi * f0 / sr;
        const float alpha = std::sin(w0) / (2.0f * Q);
        const float a0 = 1.0f + alpha;
        hissCoeffs = { alpha/a0, 0, -alpha/a0, (-2*std::cos(w0))/a0, (1-alpha)/a0 };
    }

    // Compute all speed-dependent coefficients
    recomputeSpeedCoeffs();
}

void TapeEngine::resetState()
{
    for (int ch = 0; ch < kMaxChannels; ++ch) resetChannelState(ch);
    writePos = 0;
    sampleCounter = 0;
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
        const float db = kDriveMinDb + a * (kDriveMaxDb - kDriveMinDb);
        driveGainSmoothed.setTargetValue(std::pow(10.0f, db / 20.0f));
    }
}

void TapeEngine::setMix(float a) { if (currentMix != a) { currentMix = a; mixSmoothed.setTargetValue(a); } }

void TapeEngine::setFormulation(int index)
{
    pendingFormulation.store(std::max(0, std::min(index, 2)), std::memory_order_relaxed);
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
    // Hermite (cubic) upsampling before nonlinear — avoids intermodulation aliasing
    // Uses previous two samples + current to construct a cubic through 4 points
    const float h0 = hystState[ch].H_prev;  // sample n-1
    const float h1 = input;  // sample n
    // Cubic Hermite with centered-difference tangent at h0 (fix 2.5)
    const float tangent0 = (h1 - prevH[ch]) * 0.5f;  // centered difference using n-2
    const float tangent1 = (h1 - h0) * 0.5f;           // forward difference, scaled to match tangent0
    prevH[ch] = h0;  // store for next call
    float os[kOversample];
    for (int j = 0; j < kOversample; ++j)
    {
        const float t = static_cast<float>(j + 1) / static_cast<float>(kOversample);
        const float t2 = t * t;
        const float t3 = t2 * t;
        // Hermite basis functions
        const float h00 = 2.0f * t3 - 3.0f * t2 + 1.0f;
        const float h10 = t3 - 2.0f * t2 + t;
        const float h01 = -2.0f * t3 + 3.0f * t2;
        const float h11 = t3 - t2;
        const float H_interp = h00 * h0 + h10 * tangent0 + h01 * h1 + h11 * tangent1;
        os[j] = hystRK4Step(H_interp, hystState[ch].H_prev, hystState[ch]);
    }
    const float hb1a = halfBandDecimate(os[0], os[1], hb1aHistory[ch]);
    const float hb1b = halfBandDecimate(os[2], os[3], hb1bHistory[ch]);
    return halfBandDecimate(hb1a, hb1b, hb2History[ch]);
}

float TapeEngine::processSoftSat(float input, int ch)
{
    // Update compression envelope (asymmetric peak follower)
    const float absLevel = std::abs(input);
    const float compAlpha = (absLevel > compEnvState[ch]) ? compAttackAlpha : compReleaseAlpha;
    compEnvState[ch] += compAlpha * (absLevel - compEnvState[ch]);

    // Dynamic onset: louder signal → earlier saturation → tape compression character
    const float dynamicOnset = std::max(0.3f, kSatOnset - compEnvState[ch] * 0.2f);

    if (absLevel <= dynamicOnset) return input;
    const float over = std::min(1.0f, (absLevel - dynamicOnset) / kSatTransition);
    const float knee = over * over * (3.0f - 2.0f * over);
    return input * (1.0f - knee) + fastTanh(input) * knee;
}

void TapeEngine::processHissBlock(int ch, float hissAmt, float& wet)
{
    const float hissRed = 1.0f - std::min(1.0f, dynLpEnvState[ch] * kHissReductionScale) * kHissReductionMax;
    const float white = fastNoise(hissPrngState[ch]) * hissAmt * kHissGain * hissGainScale * hissRed;
    pinkB0[ch] = 0.99886f * pinkB0[ch] + white * 0.0555179f;
    pinkB1[ch] = 0.99332f * pinkB1[ch] + white * 0.0750759f;
    pinkB2[ch] = 0.96900f * pinkB2[ch] + white * 0.1538520f;
    pinkB3[ch] = 0.86650f * pinkB3[ch] + white * 0.3104856f;
    pinkB4[ch] = 0.55000f * pinkB4[ch] + white * 0.5329522f;
    float pink = (pinkB0[ch] + pinkB1[ch] + pinkB2[ch] + pinkB3[ch] + pinkB4[ch] + white * 0.5362f) * 0.11f;
    pink = processBiquad(pink, hissCoeffs, hissFilterState[ch]) * 0.5f + pink * 0.5f;
    wet += pink;
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

        // Drive gain: smoothed per-sample in linear domain (no zipper, no per-sample pow)
        cachedDriveGain = driveGainSmoothed.getNextValue();

        // Update cached expensive computations every N samples
        if ((i & (kCachedUpdateInterval - 1)) == 0)
        {
            cachedWowSqrt = std::sqrt(wowRaw);
            cachedFlutterSqrt = std::sqrt(flutterRaw);
            cachedDriveMakeup = std::sqrt(cachedDriveGain);
            // Hysteresis curve shape from bias (fix 3.9 — was per-sample, now every 16)
            const float biasOffset = bias - 0.5f;
            hystK = std::max(0.05f, hystBaseK + biasOffset * (kBiasKMax - kBiasKMin));
            hystA = std::max(0.05f, hystBaseA - biasOffset * (kBiasAMax - kBiasAMin));
        }

        for (int ch = 0; ch < numChannels && ch < kMaxChannels; ++ch)
        {
            const float totalDelay = computeModDelay(ch, cachedWowSqrt, cachedFlutterSqrt);

            auto* data = buffer.getWritePointer(ch);
            const float dry = data[i];
            delayBuffer[ch][writePos] = dry;
            float wet = readHermite(ch, totalDelay);

            // NAB pre-emphasis: LF shelf (318µs) + HF shelf (50µs)
            wet = processBiquad(wet, nabLfPreCoeffs, nabLfPreState[ch]);
            wet = processBiquad(wet, preEmpCoeffs, preEmpState[ch]);

            // Drive into tape (scale signal, saturate harder, partial makeup)
            wet *= cachedDriveGain;
            wet = processHysteresisBlock(ch, wet);
            wet = processSoftSat(wet, ch);
            wet /= cachedDriveMakeup;

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

            // Head bump + notch + gap loss (all speed-scaled)
            wet = processBiquad(wet, headBumpCoeffs, headBumpState[ch]);
            wet = processBiquad(wet, notchCoeffs, notchState[ch]);
            wet = processBiquad(wet, gapLossCoeffs, gapLossState[ch]);

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

            // Pink noise hiss (speed-scaled gain)
            if (hiss > 0.001f)
                processHissBlock(ch, hiss, wet);

            data[i] = dry + mix * (wet - dry);
        }

        writePos = (writePos + 1) & (kDelayBufferSize - 1);

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
