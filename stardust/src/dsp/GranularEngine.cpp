#include "GranularEngine.h"

static constexpr float kMaxOverlap = 8.0f;
static constexpr float kTailDurationMs = 150.0f;  // shorter = tighter stop, less residual
static constexpr float kNormSmoothing = 0.005f; // ~10ms time constant (smoother density transitions)
static constexpr float kMaxNormGain = 1.2f;       // ~1.6dB max boost at sparse density
static constexpr float kGrainGainVariation = 0.25f;


static float quantizePitch(float semitones, int mode)
{
    if (mode == 0) return semitones;
    static const int kScales[4][7] = {
        {0,1,2,3,4,5,6},      // Chromatic (all 12 — use steps=12)
        {0,2,4,5,7,9,11},     // Major
        {0,2,3,5,7,8,10},     // Minor
        {0,2,4,7,9,-1,-1}     // Pentatonic (5 notes)
    };
    const int modeIdx = juce::jlimit(0, 3, mode - 1);
    const int* scale = kScales[modeIdx];
    const int steps = (mode == 1) ? 12 : (mode == 4 ? 5 : 7);
    const int oct = static_cast<int>(std::floor(semitones / 12.0f));
    const int rem = static_cast<int>(std::round(semitones)) % 12;
    const int remWrapped = ((rem % 12) + 12) % 12;
    int best = scale[0], bestDist = 99;
    for (int i = 0; i < steps && scale[i] >= 0; ++i)
    {
        const int d = std::abs(remWrapped - scale[i]);
        if (d < bestDist) { bestDist = d; best = scale[i]; }
    }
    return static_cast<float>(oct * 12 + best);
}

float GranularEngine::fastNoise(uint32_t& state)
{
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    return static_cast<float>(state) / static_cast<float>(UINT32_MAX) * 2.0f - 1.0f;
}

float GranularEngine::fastTanh(float x)
{
    if (x > 4.0f) return 1.0f;
    if (x < -4.0f) return -1.0f;
    const float x2 = x * x;
    return x * (27.0f + x2) / (27.0f + 9.0f * x2);
}

void GranularEngine::prepare(double newSampleRate, int /*samplesPerBlock*/)
{
    sampleRate = newSampleRate;
    const float sr = static_cast<float>(sampleRate);
    const float twoPi = 6.28318530718f;

    GrainEnvelopeLUT::initialize();

    // Pre-compute windowed sinc LUT (Kaiser window, beta=6, 8-tap)
    if (!sincLUT)
        sincLUT = std::make_unique<std::array<float, kSincTableSize * kSincTaps>>();
    {
        constexpr float beta = 8.6f;
        constexpr float pi = 3.14159265358979f;
        // Approximate Kaiser window: I0(beta * sqrt(1 - x^2)) / I0(beta)
        // Using I0 approximation: I0(x) ≈ 1 + (x/2)^2 + (x/2)^4/4 + (x/2)^6/36 + (x/2)^8/576
        auto besselI0 = [](float x) -> float {
            const float xh = x * 0.5f;
            const float xh2 = xh * xh;
            return 1.0f + xh2 * (1.0f + xh2 * (0.25f + xh2 * (1.0f/36.0f + xh2 * (1.0f/576.0f + xh2 * (1.0f/14400.0f)))));
        };
        const float i0Beta = besselI0(beta);
        for (int fi = 0; fi < kSincTableSize; ++fi)
        {
            const float frac = static_cast<float>(fi) / static_cast<float>(kSincTableSize);
            float tapSum = 0.0f;
            for (int t = 0; t < kSincTaps; ++t)
            {
                const float x = static_cast<float>(t - kSincHalfTaps) + (1.0f - frac);
                // Sinc function
                float sincVal = (std::abs(x) < 1e-6f) ? 1.0f : std::sin(pi * x) / (pi * x);
                // Kaiser window
                const float nPos = 2.0f * static_cast<float>(t) / static_cast<float>(kSincTaps - 1) - 1.0f;
                const float kaiserArg = beta * std::sqrt(std::max(0.0f, 1.0f - nPos * nPos));
                const float window = besselI0(kaiserArg) / i0Beta;
                (*sincLUT)[static_cast<size_t>(fi * kSincTaps + t)] = sincVal * window;
                tapSum += sincVal * window;
            }
            // Normalize so taps sum to 1.0
            if (std::abs(tapSum) > 1e-6f)
                for (int t = 0; t < kSincTaps; ++t)
                    (*sincLUT)[static_cast<size_t>(fi * kSincTaps + t)] /= tapSum;
        }
    }

    grainSizeSmoothed.reset(sampleRate, 0.02);
    grainDensitySmoothed.reset(sampleRate, 0.02);
    grainScatterSmoothed.reset(sampleRate, 0.02);
    grainMixSmoothed.reset(sampleRate, 0.02);
    grainStereoWidthSmoothed.reset(sampleRate, 0.02);
    basePitchRateSmoothed.reset(sampleRate, 0.02);
    positionSmoothed.reset(sampleRate, 0.05);
    feedbackSmoothed.reset(sampleRate, 0.02);
    textureSmoothed.reset(sampleRate, 0.05);
    spaceSmoothed.reset(sampleRate, 0.02);

    if (!inputBuffer)
        inputBuffer = std::make_unique<std::array<std::array<float, kInputBufferSize>, kMaxChannels>>();
    for (int ch = 0; ch < kMaxChannels; ++ch)
    {
        (*inputBuffer)[ch].fill(0.0f);
        feedbackBuffer[ch] = 0.0f;
        feedbackLpState[ch] = 0.0f;
    }
    feedbackLpAlpha = 1.0f - std::exp(-twoPi * 8000.0f / sr);

    writePos = 0; frozenWritePos = 0;
    samplesSinceLastGrain = 0.0f; nextGrainInterval = 0.0f;
    warmupSamples = 512; frozen = false;
    freezeFade = 0.0f; freezeTarget = false;
    freezeFadeInc = 1.0f / (0.01f * sr); // 10ms crossfade

    for (auto& g : grains) g.active = false;
    activeCount = 0;

    inputRmsState = 0.0f;
    rmsAlpha = 1.0f - std::exp(-twoPi * 30.0f / sr);
    inTailMode = false; tailSamplesRemaining = 0;
    tailDurationSamples = static_cast<int>(kTailDurationMs * 0.001f * sr);
    normSmoothed = 1.0f;

    posLfo.init(0.1f, sr);
    sizeLfo.init(0.07f, sr);
    pitchLfo.init(0.13f, sr);
    lfoNormCounter = 0;

    freezePosNoise = 0.0f;
    freezePitchNoise = 0.0f;
    freezeNoiseAlpha = 1.0f - std::exp(-twoPi * 0.5f / sr); // ~0.5Hz bandwidth for organic drift
    freezeNoiseState = 31337;

    dattorro.prepare(sampleRate);
    inputFollower.prepare(sampleRate);
}

void GranularEngine::setParameters(float grainSize, float grainDensity,
                                    float grainScatter, float grainMix, float grainStereoWidth)
{
    if (currentGrainSize != grainSize) { currentGrainSize = grainSize; grainSizeSmoothed.setTargetValue(grainSize); }
    if (currentDensity != grainDensity) { currentDensity = grainDensity; grainDensitySmoothed.setTargetValue(grainDensity); }
    if (currentScatter != grainScatter) { currentScatter = grainScatter; grainScatterSmoothed.setTargetValue(grainScatter); }
    if (currentMix != grainMix) { currentMix = grainMix; grainMixSmoothed.setTargetValue(grainMix); }
    if (currentStereoWidth != grainStereoWidth) { currentStereoWidth = grainStereoWidth; grainStereoWidthSmoothed.setTargetValue(grainStereoWidth); }
}

void GranularEngine::setBasePitch(float semitones)
{
    if (currentBasePitch != semitones) { currentBasePitch = semitones; basePitchRateSmoothed.setTargetValue(std::pow(2.0f, semitones / 12.0f)); }
}

void GranularEngine::setPosition(float position)
{
    if (currentPosition != position) { currentPosition = position; positionSmoothed.setTargetValue(position); }
}

void GranularEngine::setFreeze(bool freeze)
{
    freezeTarget.store(freeze, std::memory_order_relaxed);
}

void GranularEngine::setFeedback(float amount)
{
    const float c = std::max(0.0f, std::min(1.0f, amount));
    if (currentFeedback != c) { currentFeedback = c; feedbackSmoothed.setTargetValue(c * 0.7f); }
}

void GranularEngine::setTexture(float texture)
{
    if (currentTexture != texture) { currentTexture = texture; textureSmoothed.setTargetValue(texture); }
}

void GranularEngine::setSpace(float space)
{
    if (currentSpace != space) { currentSpace = space; spaceSmoothed.setTargetValue(space); }
}

void GranularEngine::setDrift(float drift)
{
    currentDriftRaw = std::max(0.0f, std::min(1.0f, drift));
}

void GranularEngine::setCloud(float cloud)
{
    currentCloud = std::max(0.0f, std::min(1.0f, cloud));
}

void GranularEngine::setReverse(float prob)
{
    reverseProb = juce::jlimit(0.0f, 1.0f, prob);
}

void GranularEngine::setVoices(int count)
{
    numVoices = juce::jlimit(1, 4, count);
}

void GranularEngine::setVoiceInterval(float semitones)
{
    voiceIntervalSemitones = juce::jlimit(0.0f, 24.0f, semitones);
}

void GranularEngine::setPitchQuant(int mode)
{
    currentPitchQuant = juce::jlimit(0, 4, mode);
}

void GranularEngine::setPitchSpread(float spread)
{
    pitchSpread = juce::jlimit(0.0f, 1.0f, spread);
}

void GranularEngine::setLoopback(float amount)
{
    loopbackAmount = juce::jlimit(0.0f, 1.0f, amount);
}

void GranularEngine::setSVFCutoff(float hz) { svfCutoffHz = hz; }

void GranularEngine::setSVFResonance(float q) { svfResonance = juce::jlimit(0.3f, 4.0f, q); }

float GranularEngine::readHermite(int channel, float position) const
{
    int idx1 = static_cast<int>(std::floor(position)) & kInputBufferMask;
    const int idx0 = (idx1 - 1) & kInputBufferMask;
    const int idx2 = (idx1 + 1) & kInputBufferMask;
    const int idx3 = (idx1 + 2) & kInputBufferMask;
    const float frac = position - std::floor(position);
    const auto& buf = (*inputBuffer)[static_cast<size_t>(channel)];
    const float y0 = buf[static_cast<size_t>(idx0)], y1 = buf[static_cast<size_t>(idx1)];
    const float y2 = buf[static_cast<size_t>(idx2)], y3 = buf[static_cast<size_t>(idx3)];
    const float c1 = 0.5f * (y2 - y0);
    const float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
    const float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
    return ((c3 * frac + c2) * frac + c1) * frac + y1;
}

float GranularEngine::readSinc(int channel, float position) const
{
    const int idx = static_cast<int>(std::floor(position));
    const float frac = position - std::floor(position);
    const int tableIdx = std::min(kSincTableSize - 1, static_cast<int>(frac * static_cast<float>(kSincTableSize)));
    const auto& buf = (*inputBuffer)[static_cast<size_t>(channel)];
    const float* taps = &(*sincLUT)[static_cast<size_t>(tableIdx * kSincTaps)];
    float result = 0.0f;
    for (int t = 0; t < kSincTaps; ++t)
    {
        const int sampleIdx = (idx - kSincHalfTaps + t) & kInputBufferMask;
        result += buf[static_cast<size_t>(sampleIdx)] * taps[t];
    }
    return result;
}

// B12: Time-reversed sinc kernel for reversed grains.
// Uses 1-frac for the table index so the kernel is mirrored around the read point,
// giving the correct spectral envelope when grains play backwards.
float GranularEngine::readSincReversed(int channel, float position) const
{
    const int idx = static_cast<int>(std::floor(position));
    const float frac = position - std::floor(position);
    const float revFrac = 1.0f - frac; // mirror fractional offset
    const int tableIdx = std::min(kSincTableSize - 1, static_cast<int>(revFrac * static_cast<float>(kSincTableSize)));
    const auto& buf = (*inputBuffer)[static_cast<size_t>(channel)];
    const float* taps = &(*sincLUT)[static_cast<size_t>(tableIdx * kSincTaps)];
    float result = 0.0f;
    for (int t = 0; t < kSincTaps; ++t)
    {
        const int sampleIdx = (idx - kSincHalfTaps + t) & kInputBufferMask;
        result += buf[static_cast<size_t>(sampleIdx)] * taps[t];
    }
    return result;
}

bool GranularEngine::isBufferSilent(float position, int lengthSamples) const
{
    constexpr int kCheckPoints = 8;
    const float step = static_cast<float>(lengthSamples) / static_cast<float>(kCheckPoints);
    float peak = 0.0f;
    for (int p = 0; p < kCheckPoints; ++p)
    {
        int idx = static_cast<int>(position + step * static_cast<float>(p)) & kInputBufferMask;
        peak = std::max(peak, std::abs((*inputBuffer)[0][static_cast<size_t>(idx)]));
    }
    return peak < kSilenceThreshold;
}

float GranularEngine::findNearestZeroCrossing(int startIdx, int searchRange) const
{
    // Prefer actual sign-change crossings with linear interpolation (C8)
    // Falls back to minimum-amplitude sample if no sign change found in range.
    float bestCrossingFrac = static_cast<float>(startIdx & kInputBufferMask);
    float bestCrossingDist = static_cast<float>(searchRange + 1);

    int fallbackIdx = startIdx & kInputBufferMask;
    float fallbackAbs = std::abs((*inputBuffer)[0][static_cast<size_t>(startIdx & kInputBufferMask)]);

    for (int off = -searchRange + 1; off <= searchRange; ++off)
    {
        const int i0 = (startIdx + off - 1) & kInputBufferMask;
        const int i1 = (startIdx + off)     & kInputBufferMask;
        const float s0 = (*inputBuffer)[0][static_cast<size_t>(i0)];
        const float s1 = (*inputBuffer)[0][static_cast<size_t>(i1)];

        // Track minimum amplitude fallback
        if (std::abs(s1) < fallbackAbs) { fallbackAbs = std::abs(s1); fallbackIdx = i1; }

        // Sign change: find interpolated crossing point
        if (s0 * s1 < 0.0f)
        {
            // Linear interpolation: s0 + t*(s1-s0) = 0  →  t = -s0/(s1-s0) = s0/(s0-s1)
            const float t = s0 / (s0 - s1); // in [0, 1]
            const float crossingFrac = static_cast<float>(i0) + t;
            const float dist = std::abs(crossingFrac - static_cast<float>(startIdx & kInputBufferMask));
            if (dist < bestCrossingDist) { bestCrossingDist = dist; bestCrossingFrac = crossingFrac; }
        }
    }

    // Return sign-change crossing if found; otherwise return minimum-amplitude sample
    if (bestCrossingDist <= static_cast<float>(searchRange))
        return bestCrossingFrac;
    return static_cast<float>(fallbackIdx);
}

void GranularEngine::rebuildActiveList()
{
    activeCount = 0;
    for (int i = 0; i < kMaxGrains; ++i)
        if (grains[static_cast<size_t>(i)].active)
            activeIndices[static_cast<size_t>(activeCount++)] = i;
}

// ============================================================================
// Grain spawning (extracted from process loop)
// ============================================================================
void GranularEngine::spawnGrains(float readAnchor, float scatter, float basePitchRate,
                                  float pitchMod, int grainSizeSamples, float effectiveDensity,
                                  float overlapFactor, float stereoWidth, bool isStereo)
{
    const float sr = static_cast<float>(sampleRate);
    const float twoPi = 6.28318530718f;
    const float bufDepth = static_cast<float>(kInputBufferSize) * 0.9f;

    // Fixed-interval scheduling (GRN Lite style — metronomic, density = grains/sec)
    // Mean interval = grainLength / density (in samples)
    const float meanIntervalSamples = static_cast<float>(grainSizeSamples) / std::max(0.1f, effectiveDensity);
    nextGrainInterval = std::max(1.0f, meanIntervalSamples);

    // Single grain per trigger — steal oldest grain if pool is full
    Grain* target = nullptr;
    for (auto& g : grains)
    {
        if (!g.active) { target = &g; break; }
    }
    // Voice stealing: if no free slot, steal the grain closest to finishing
    if (!target)
    {
        // Pool exhaustion guard: if all grains are near end-of-life (>80%), skip this
        // spawn to prevent spawn→immediate-steal stutter (they'll complete in ~0.2×grainLen)
        bool anyHasRoom = false;
        for (const auto& g : grains)
            if (g.currentSample < static_cast<int>(g.lengthInSamples * 0.8f))
                { anyHasRoom = true; break; }
        if (!anyHasRoom) return;

        int maxProgress = -1;
        for (auto& g : grains)
        {
            if (g.currentSample > maxProgress) { maxProgress = g.currentSample; target = &g; }
        }
    }
    if (target)
    {
        auto& g = *target;
        g.active = false; // reset before re-spawning

        // Cap to 200ms max temporal scatter regardless of buffer depth
        const float maxScatter = std::min(scatter * bufDepth * 0.3f, static_cast<float>(sampleRate) * 0.2f);
        // Transient-aware spawn: when Texture > 0.5 and an envelope peak is detected,
        // bias grains toward the current write position (the transient) by reducing scatter.
        const float envLevel = inputFollower.getEnvelope();
        const float transientBias = juce::jlimit(0.0f, 1.0f,
            (currentTexture - 0.5f) * 2.0f) * envLevel;
        const float effectiveScatter = maxScatter * (1.0f - transientBias * 0.8f);
        float startPos = readAnchor + fastNoise(noiseState) * effectiveScatter;

        // Freeze micro-variation: noise-shaped drift + per-grain random offset
        g.freezeJitterOffset = fastNoise(spatialNoiseState); // random ±1 per grain (spatial PRNG)
        if (frozen)
        {
            const float jitterDepth = std::min(sr * 0.020f, static_cast<float>(grainSizeSamples) * 0.5f);
            // Combine shared noise drift with per-grain random offset for organic texture
            startPos += (freezePosNoise + g.freezeJitterOffset * 0.5f) * jitterDepth;
        }

        startPos = static_cast<float>(static_cast<int>(startPos + static_cast<float>(kInputBufferSize)) & kInputBufferMask);

        // Zero-crossing alignment for small grains
        if (grainSizeSamples < static_cast<int>(sr * 0.01f))
        {
            const int searchRange = std::min(32, grainSizeSamples / 2);
            startPos = findNearestZeroCrossing(static_cast<int>(startPos), searchRange);
        }

        // Only skip if the buffer region is truly silent AND we're reading near the write head
        // (avoids blocking grains when position knob explores older buffer content)
        const int distFromWrite = (writePos - static_cast<int>(startPos) + kInputBufferSize) & kInputBufferMask;
        if (!frozen && distFromWrite < grainSizeSamples * 4 && isBufferSilent(startPos, grainSizeSamples))
            return;

        g.currentPosition = startPos;

        // Pitch with quantized scatter
        float pitchRate = basePitchRate * (1.0f + pitchMod);
        if (frozen)
            pitchRate *= 1.0f + (freezePitchNoise + g.freezeJitterOffset * 0.3f) * 0.015f; // ±25 cents noise-shaped

        // Continuous pitch jitter ±12st (GRN Lite style — with optional scale quantization)
        if (scatter > 0.0f)
        {
            float pitchSemitones = fastNoise(noiseState) * scatter * 12.0f;
            pitchSemitones = quantizePitch(pitchSemitones, currentPitchQuant);
            pitchRate *= std::pow(2.0f, pitchSemitones / 12.0f);
        }
        g.playbackRate = pitchRate * std::pow(2.0f, fastNoise(noiseState) * 5.0f / 1200.0f);

        // Pitch spread: per-grain random pitch offset ±12 semitones (user-controlled)
        if (pitchSpread > 0.0f)
        {
            const float spreadSt = fastNoise(noiseState) * pitchSpread * 12.0f;
            g.playbackRate *= std::pow(2.0f, spreadSt / 12.0f);
        }

        // Reverse: probability set by explicit reverseProb member (user knob)
        g.reverse = (fastNoise(noiseState) * 0.5f + 0.5f) < reverseProb;
        if (g.reverse) g.playbackRate = -std::abs(g.playbackRate);

        // Pan + gain
        const float pan = 0.5f + fastNoise(spatialNoiseState) * 0.5f * stereoWidth;
        g.panL = std::cos(pan * 1.5707963f);
        g.panR = std::sin(pan * 1.5707963f);
        g.gainMul = 1.0f + fastNoise(spatialNoiseState) * kGrainGainVariation;

        // Length
        g.lengthInSamples = std::max(16, static_cast<int>(
            static_cast<float>(grainSizeSamples) * (1.0f + fastNoise(noiseState) * 0.2f)));
        g.currentSample = 0;

        // Pitch drift
        const float driftCents = fastNoise(noiseState) * 10.0f * scatter;
        const float driftTarget = g.playbackRate * std::pow(2.0f, driftCents / 1200.0f);
        g.pitchDriftRate = (driftTarget - g.playbackRate) / static_cast<float>(g.lengthInSamples);

        // AA biquad (cascaded 4-pole Butterworth for rate > 1.5x)
        const float absRate = std::abs(g.playbackRate);
        if (absRate > 1.05f)
        {
            const float fc = std::min(sr * 0.45f, (sr * 0.45f) / absRate);
            const float w0 = twoPi * fc / sr;
            const float cosw0 = std::cos(w0);
            // Stage 1: Q = 0.5412 for 4-pole Butterworth (or 0.707 if single stage)
            const float q1 = (absRate > 1.5f) ? 0.5412f : 0.7071f;
            const float alpha1 = std::sin(w0) / (2.0f * q1);
            const float a0_1 = 1.0f + alpha1;
            g.aaB0 = ((1.0f - cosw0) * 0.5f) / a0_1;
            g.aaB1 = (1.0f - cosw0) / a0_1; g.aaB2 = g.aaB0;
            g.aaA1 = (-2.0f * cosw0) / a0_1; g.aaA2 = (1.0f - alpha1) / a0_1;
            g.aaActive = true;
            g.aa2Active = absRate > 1.5f;
            if (g.aa2Active)
            {
                // Stage 2: Q = 1.3066 for proper 4-pole Butterworth maximally-flat passband
                const float alpha2 = std::sin(w0) / (2.0f * 1.3066f);
                const float a0_2 = 1.0f + alpha2;
                g.aa2B0 = ((1.0f - cosw0) * 0.5f) / a0_2;
                g.aa2B1 = (1.0f - cosw0) / a0_2; g.aa2B2 = g.aa2B0;
                g.aa2A1 = (-2.0f * cosw0) / a0_2; g.aa2A2 = (1.0f - alpha2) / a0_2;
            }
        }
        else { g.aaActive = false; g.aa2Active = false; }
        g.aaS1[0] = g.aaS1[1] = g.aaS2[0] = g.aaS2[1] = 0.0f;
        g.aa2S1[0] = g.aa2S1[1] = g.aa2S2[0] = g.aa2S2[1] = 0.0f;

        // SVF enabled with near-transparent high-frequency LP (adds subtle air/warmth)
        // Cutoff is envelope-modulated: louder input → brighter grains, silence → darker
        g.svfActive = true;
        g.svfBaseFreq = svfCutoffHz;
        g.svfRange = svfCutoffHz * 0.3f * juce::jlimit(0.0f, 1.0f, inputFollower.getEnvelope() * 2.0f);
        g.svfQ = svfResonance;
        g.svfIc1[0] = g.svfIc1[1] = g.svfIc2[0] = g.svfIc2[1] = 0.0f;

        // Decorrelation allpass (0.2–0.6 range safer for mono compatibility)
        g.decorrelCoeff = 0.2f + (fastNoise(spatialNoiseState) * 0.5f + 0.5f) * 0.4f;
        g.decorrelState = 0.0f;

        g.active = true;
    } // single grain per Poisson trigger via target selection above
}

// ============================================================================
// Per-grain processing (extracted from process loop)
// ============================================================================
void GranularEngine::processGrain(Grain& g, bool isStereo, float sr, float envFollow,
                                   float& outL, float& outR)
{
    if (g.lengthInSamples < 1) { g.active = false; activeListDirty = true; return; }

    const float phase = static_cast<float>(g.currentSample) / static_cast<float>(g.lengthInSamples);

    // Hann-only envelope (GRN Lite style — consistent, click-free)
    // Reversed grains invert envelope phase so attack/release are also reversed
    const float envPhase = g.reverse ? (1.0f - phase) : phase;
    float envelope = GrainEnvelopeLUT::lookup(GrainEnvelopeLUT::Hanning, envPhase);

    // Minimum 16-sample fade-in/fade-out guard (prevents clicks regardless of envelope shape)
    constexpr int kMinFadeSamples = 16;
    if (g.currentSample < kMinFadeSamples)
        envelope *= static_cast<float>(g.currentSample) / static_cast<float>(kMinFadeSamples);
    else if (g.currentSample >= g.lengthInSamples - kMinFadeSamples)
        envelope *= std::max(0.0f, static_cast<float>(g.lengthInSamples - 1 - g.currentSample) / static_cast<float>(kMinFadeSamples));

    // Read from buffer — sinc HQ mode always when LUT is available (B9).
    // Reversed grains use time-reversed sinc kernel for correct spectral envelope (B12).
    const bool useSinc = (sincLUT != nullptr);
    float sL, sR;
    if (useSinc)
    {
        sL = g.reverse ? readSincReversed(0, g.currentPosition) : readSinc(0, g.currentPosition);
        sR = isStereo ? (g.reverse ? readSincReversed(1, g.currentPosition) : readSinc(1, g.currentPosition)) : sL;
    }
    else
    {
        sL = readHermite(0, g.currentPosition);
        sR = isStereo ? readHermite(1, g.currentPosition) : sL;
    }

    // AA biquad (first stage — 12dB/oct)
    if (g.aaActive)
    {
        for (int ch = 0; ch < (isStereo ? 2 : 1); ++ch)
        {
            float& s = (ch == 0) ? sL : sR;
            const float out = g.aaB0 * s + g.aaS1[ch];
            g.aaS1[ch] = g.aaB1 * s - g.aaA1 * out + g.aaS2[ch];
            g.aaS2[ch] = g.aaB2 * s - g.aaA2 * out;
            s = out;
        }
        // Second cascade stage (Q=1.3066 for 4-pole Butterworth flat passband)
        if (g.aa2Active)
        {
            for (int ch = 0; ch < (isStereo ? 2 : 1); ++ch)
            {
                float& s = (ch == 0) ? sL : sR;
                const float out = g.aa2B0 * s + g.aa2S1[ch];
                g.aa2S1[ch] = g.aa2B1 * s - g.aa2A1 * out + g.aa2S2[ch];
                g.aa2S2[ch] = g.aa2B2 * s - g.aa2A2 * out;
                s = out;
            }
        }
    }

    // Stereo decorrelation allpass on R channel
    if (isStereo)
    {
        const float in = sR;
        const float out = g.decorrelCoeff * in + g.decorrelState;
        g.decorrelState = in - g.decorrelCoeff * out;
        sR = out;
    }

    // SVF with cached coefficients (recompute every 16 samples)
    if (g.svfActive)
    {
        if (g.svfUpdateCounter <= 0)
        {
            const float cutoff = std::min(g.svfBaseFreq + envelope * g.svfRange, sr * 0.45f);
            // std::tan for accurate SVF coefficient up to 0.499×Nyquist
            g.svfCachedG0 = std::tan(juce::MathConstants<float>::pi * std::min(cutoff, sr * 0.499f) / sr);
            g.svfCachedK = 1.0f / g.svfQ;
            g.svfUpdateCounter = 16;
        }
        --g.svfUpdateCounter;

        const float g0 = g.svfCachedG0;
        const float k = g.svfCachedK;
        const float a1 = 1.0f / (1.0f + g0 * (g0 + k));
        const float a2 = g0 * a1;
        const float a3 = g0 * a2;

        for (int ch = 0; ch < (isStereo ? 2 : 1); ++ch)
        {
            float& s = (ch == 0) ? sL : sR;
            const float v3 = s - g.svfIc2[ch];
            const float v1 = a1 * g.svfIc1[ch] + a2 * v3;
            const float v2 = g.svfIc2[ch] + a2 * g.svfIc1[ch] + a3 * v3;
            g.svfIc1[ch] = 2.0f * v1 - g.svfIc1[ch];
            g.svfIc2[ch] = 2.0f * v2 - g.svfIc2[ch];
            s = v2;
        }
    }

    // Input envelope following: grains breathe with input dynamics
    const float gainEnv = envelope * g.gainMul * envFollow;
    outL += sL * gainEnv * g.panL;
    if (isStereo) outR += sR * gainEnv * g.panR;

    // Advance grain (bitmask wrap — power-of-2 buffer)
    g.currentPosition += g.playbackRate;
    g.playbackRate += g.pitchDriftRate;
    g.currentPosition = static_cast<float>(static_cast<int>(g.currentPosition + static_cast<float>(kInputBufferSize)) & kInputBufferMask);
    g.currentSample++;
    if (g.currentSample >= g.lengthInSamples) { g.active = false; activeListDirty = true; }
}

// ============================================================================
// Main process loop
// ============================================================================
void GranularEngine::process(juce::AudioBuffer<float>& buffer)
{
    juce::ScopedNoDenormals noDenormals;
    const auto numChannels = std::min(buffer.getNumChannels(), kMaxChannels);
    const auto numSamples = buffer.getNumSamples();
    const bool isStereo = numChannels > 1;
    const float sr = static_cast<float>(sampleRate);
    const float twoPi = 6.28318530718f;

    // Block-local PPQ snapshot — avoids cross-block drift from per-sample accumulation (fix 2.10)
    double blockPpq = hostPpqPosition.load(std::memory_order_relaxed);

    for (int i = 0; i < numSamples; ++i)
    {
        const float grainSizeMs = grainSizeSmoothed.getNextValue();
        const float density = grainDensitySmoothed.getNextValue();
        const float scatter = grainScatterSmoothed.getNextValue();
        const float mix = grainMixSmoothed.getNextValue();
        const float stereoWidth = grainStereoWidthSmoothed.getNextValue();
        const float basePitchRate = basePitchRateSmoothed.getNextValue();
        const float position = positionSmoothed.getNextValue();
        const float feedback = feedbackSmoothed.getNextValue();
        const float texture = textureSmoothed.getNextValue();
        const float space = spaceSmoothed.getNextValue();

        // Freeze crossfade (10ms smooth transition)
        const float freezeGoal = freezeTarget.load(std::memory_order_relaxed) ? 1.0f : 0.0f;
        if (freezeFade < freezeGoal)
            freezeFade = std::min(freezeGoal, freezeFade + freezeFadeInc);
        else if (freezeFade > freezeGoal)
            freezeFade = std::max(freezeGoal, freezeFade - freezeFadeInc);
        // Capture write position at the exact moment freeze activates (fix 2.9)
        const bool shouldFreeze = freezeFade > 0.5f;
        if (shouldFreeze && !frozen) frozenWritePos = writePos;
        frozen = shouldFreeze;

        // Update Dattorro reverb only when texture changes (fix 3.15)
        if (std::abs(texture - lastDattorroTexture) > 0.001f)
        {
            lastDattorroTexture = texture;
            dattorro.setDecay(0.2f + texture * 0.78f);
            dattorro.setDamping(1.0f - texture * 0.7f);
            dattorro.setDiffusion(0.4f + texture * 0.5f);
        }

        const float densityRateScale = 1.5f - std::min(density, 12.0f) / 12.0f;
        const float sizeModDepth = scatter * 0.15f;
        const float sizeMod = 1.0f + sizeLfo.sinVal * sizeModDepth;
        const float pitchMod = pitchLfo.sinVal * scatter * 0.003f;

        const int grainSizeSamples = std::max(16, static_cast<int>(grainSizeMs * sizeMod * 0.001f * sr));
        const float overlapFactor = 2.0f + (std::min(density, 16.0f) / 16.0f) * (kMaxOverlap - 2.0f);

        // Write input (crossfaded during freeze transition)
        // Feedback continues during freeze — only external input is gated by writeMix
        const float writeMix = 1.0f - freezeFade;
        {
            for (int ch = 0; ch < numChannels; ++ch)
            {
                const float extIn = buffer.getReadPointer(ch)[i] * writeMix;
                float in = extIn + feedbackBuffer[ch] * feedback;
                in = fastTanh(in);
                auto& bufSample = (*inputBuffer)[static_cast<size_t>(ch)][static_cast<size_t>(writePos)];
                if (writeMix > 0.001f)
                    bufSample = bufSample * (1.0f - writeMix) + in * writeMix;
                else if (feedback > 0.001f)
                    bufSample = in; // during full freeze, write only feedback (self-oscillation continues)
            }
            if (writeMix > 0.001f)
                inputFollower.processSample(buffer.getReadPointer(0)[i],
                                            isStereo ? buffer.getReadPointer(1)[i] : buffer.getReadPointer(0)[i]);
        }

        // RMS tail detection
        if (!frozen)
            inputRmsState += rmsAlpha * (std::abs(buffer.getReadPointer(0)[i]) - inputRmsState);

        if (frozen || inputRmsState > kSilenceThreshold * 10.0f)
        { inTailMode = false; tailSamplesRemaining = tailDurationSamples; }
        else if (!inTailMode && tailSamplesRemaining > 0) inTailMode = true;
        if (inTailMode) tailSamplesRemaining = std::max(0, tailSamplesRemaining - 1);

        float effectiveDensity = density;
        if (inTailMode && tailDurationSamples > 0)
            effectiveDensity *= static_cast<float>(tailSamplesRemaining) / static_cast<float>(tailDurationSamples);

        // Warmup
        if (warmupSamples > 0)
        {
            --warmupSamples;
            if (!frozen || feedback > 0.001f) writePos = (writePos + 1) & kInputBufferMask;
            for (int ch = 0; ch < numChannels; ++ch)
                buffer.getWritePointer(ch)[i] = 0.0f;
            continue;
        }

        // Read anchor
        const int ewp = frozen ? frozenWritePos : writePos;
        const float bufDepth = static_cast<float>(kInputBufferSize) * 0.9f;
        const float lfoOff = posLfo.sinVal * 0.02f;
        // Clamp position so grains always start ≥ 1 grain-length behind the write head.
        // Without this, position=1.0 puts the read anchor AT the write head and grains
        // read stale data forward (old buffer wrap), causing silence-detection false positives
        // that suppress all grains until the buffer cycles (~3s). Position knob: 1.0 = minimum
        // latency (1 grain-length), 0.0 = maximum history (full bufDepth lookback).
        const float minLookback = static_cast<float>(grainSizeSamples) / bufDepth;
        const float effPos = std::max(0.0f, std::min(1.0f - minLookback, position + lfoOff));
        float readAnchor = static_cast<float>(ewp) - (1.0f - effPos) * bufDepth;
        if (readAnchor < 0.0f) readAnchor += static_cast<float>(kInputBufferSize);

        // Grain scheduling (Poisson free-running or tempo-synced)
        ++samplesSinceLastGrain;
        const bool canSpawn = mix > 0.001f && effectiveDensity > 0.01f
                              && !(inTailMode && tailSamplesRemaining <= 0);

        bool shouldSpawn = false;
        const double currentBpm = hostBpm.load(std::memory_order_relaxed);
        if (syncMode > 0 && currentBpm > 0.0)
        {
            // Tempo-synced: fire grains at beat subdivisions (fix 2.10)
            static constexpr double kSyncDivisions[] = { 0.0, 1.0, 0.5, 0.25, 0.125 };
            const double division = kSyncDivisions[std::min(syncMode, 4)];
            // Reset on transport jump (loop, locate) to prevent burst/silence (fix 3.5)
            if (std::abs(blockPpq - lastGrainPpq) > 2.0 * division && division > 0.0)
                lastGrainPpq = blockPpq;
            if (blockPpq >= lastGrainPpq + division)
            {
                shouldSpawn = true;
                lastGrainPpq = blockPpq;
            }
            // Advance block-local PPQ per sample (does not persist across blocks)
            blockPpq += currentBpm / (60.0 * sampleRate);
        }
        else
        {
            // Free-running Poisson
            shouldSpawn = (samplesSinceLastGrain >= nextGrainInterval);
        }

        if (shouldSpawn && canSpawn)
        {
            samplesSinceLastGrain -= nextGrainInterval;
            for (int v = 0; v < numVoices; ++v)
            {
                const float vPitch = basePitchRate
                    * std::pow(2.0f, static_cast<float>(v) * voiceIntervalSemitones / 12.0f);
                spawnGrains(readAnchor, scatter, vPitch, pitchMod,
                            grainSizeSamples, effectiveDensity, overlapFactor, stereoWidth, isStereo);
            }
            rebuildActiveList();
        }

        // Input envelope following — high floor so grains are always audible
        const float envFollow = frozen ? 1.0f : (0.7f + 0.3f * inputFollower.getEnvelope());

        // Process active grains (using packed index array — skips inactive slots)
        float grainOutputL = 0.0f, grainOutputR = 0.0f;
        for (int ai = 0; ai < activeCount; ++ai)
        {
            auto& g = grains[static_cast<size_t>(activeIndices[static_cast<size_t>(ai)])];
            processGrain(g, isStereo, sr, envFollow, grainOutputL, grainOutputR);
        }
        // Rebuild active list only when grains deactivated during processing (fix 2.6)
        if (activeListDirty) { rebuildActiveList(); activeListDirty = false; }

        // Density normalization — use base density (not effectiveDensity) during tail so
        // normSmoothed doesn't ramp toward kMaxNormGain as tail fades density to 0 (+13dB burst)
        const float normBase = inTailMode ? density : effectiveDensity;
        const float densNorm = 1.0f / std::sqrt(std::max(1.0f, normBase));
        normSmoothed += kNormSmoothing * (std::min(kMaxNormGain, densNorm) - normSmoothed);
        grainOutputL *= normSmoothed;
        grainOutputR *= normSmoothed;

        // Smooth output fade during tail — removes any residual burst on stop
        if (inTailMode && tailDurationSamples > 0)
        {
            const float tailFade = static_cast<float>(tailSamplesRemaining) / static_cast<float>(tailDurationSamples);
            grainOutputL *= tailFade;
            grainOutputR *= tailFade;
        }

        // Dattorro plate reverb (space controls wet amount, bypass when dry)
        if (space > 0.001f)
        {
            float reverbL, reverbR;
            dattorro.processSample(grainOutputL, grainOutputR, reverbL, reverbR);
            const float fdnDry = 1.0f - space * 0.6f;
            grainOutputL = grainOutputL * fdnDry + reverbL * space;
            grainOutputR = grainOutputR * fdnDry + reverbR * space;
        }

        // Polynomial soft clipper
        for (float* s : { &grainOutputL, &grainOutputR })
        {
            const float ax = std::abs(*s);
            if (ax > 0.75f)
            {
                const float over = std::min(1.0f, (ax - 0.75f) / 0.25f);
                const float knee = over * over * (3.0f - 2.0f * over);
                *s = *s * (1.0f - knee) + fastTanh(*s) * knee;
            }
        }

        // Loopback: feed a fraction of grain output back into the input buffer
        // (injected ahead of write position so grains read it on the next cycle)
        if (loopbackAmount > 0.001f && !frozen)
        {
            const int loopbackPos = (writePos + 1) & kInputBufferMask;
            (*inputBuffer)[0][static_cast<size_t>(loopbackPos)] +=
                grainOutputL * loopbackAmount * 0.5f;
            if (isStereo)
                (*inputBuffer)[1][static_cast<size_t>(loopbackPos)] +=
                    grainOutputR * loopbackAmount * 0.5f;
            else
                (*inputBuffer)[1][static_cast<size_t>(loopbackPos)] +=
                    grainOutputL * loopbackAmount * 0.5f;
        }

        // Feedback (LP filtered + cross-channel coupling for stereo evolution)
        feedbackLpState[0] += feedbackLpAlpha * (grainOutputL - feedbackLpState[0]);
        feedbackLpState[1] += feedbackLpAlpha * (grainOutputR - feedbackLpState[1]);
        if (isStereo)
        {
            // Hadamard-style cross-coupling (5% bleed for subtle stereo widening in self-oscillation)
            constexpr float kCrossBleed = 0.05f;
            feedbackBuffer[0] = feedbackLpState[0] + kCrossBleed * (feedbackLpState[1] - feedbackLpState[0]);
            feedbackBuffer[1] = feedbackLpState[1] + kCrossBleed * (feedbackLpState[0] - feedbackLpState[1]);
        }
        else
        {
            feedbackBuffer[0] = feedbackLpState[0];
        }

        // Dry/wet (precompute once — mix changes slowly)
        const float dryGain = std::cos(mix * 1.5707963f);
        const float wetGain = std::sin(mix * 1.5707963f);
        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto* data = buffer.getWritePointer(ch);
            data[i] = data[i] * dryGain + ((ch == 0) ? grainOutputL : grainOutputR) * wetGain;
        }

        // Advance write position (always advance when feedback is active, even during freeze)
        if (!frozen || feedback > 0.001f)
            writePos = (writePos + 1) & kInputBufferMask;

        // Advance LFOs via incremental oscillators (zero trig per sample)
        posLfo.advance(densityRateScale);
        sizeLfo.advance(densityRateScale);
        pitchLfo.advance(densityRateScale);
        if (++lfoNormCounter >= 512)
        {
            lfoNormCounter = 0;
            posLfo.normalize(); sizeLfo.normalize(); pitchLfo.normalize();
        }

        // Freeze noise-shaped drift (LP-filtered random walk — organic, not periodic)
        if (frozen)
        {
            freezePosNoise += freezeNoiseAlpha * (fastNoise(freezeNoiseState) - freezePosNoise);
            freezePitchNoise += freezeNoiseAlpha * (fastNoise(freezeNoiseState) - freezePitchNoise);
        }
    }
}

void GranularEngine::getGrainDisplayInfo(std::array<GrainDisplayInfo, kMaxGrains>& out) const
{
    const int ewp = frozen ? frozenWritePos : writePos;
    for (int i = 0; i < kMaxGrains; ++i)
    {
        const auto& g = grains[i];
        if (!g.active) { out[i] = {}; continue; }
        // Distance of grain's current read position behind the write head
        const int readIdx = static_cast<int>(g.currentPosition) & kInputBufferMask;
        const int age = (ewp - readIdx + kInputBufferSize) & kInputBufferMask;
        // Normalize to 0.75s window — covers full scatter range (±0.5s) with headroom
        const int maxDisplayAge = std::max(1, static_cast<int>(sampleRate * 0.75));
        out[i].normPos = 1.0f - juce::jlimit(0.0f, 1.0f, static_cast<float>(age) / static_cast<float>(maxDisplayAge));
        out[i].phase   = (g.lengthInSamples > 0)
            ? juce::jlimit(0.0f, 1.0f, static_cast<float>(g.currentSample) / static_cast<float>(g.lengthInSamples))
            : 0.0f;
        out[i].active = true;
    }
}
