#include "GranularEngine.h"

static constexpr float kMaxOverlap = 8.0f;
static constexpr float kTailDurationMs = 300.0f;
static constexpr float kNormSmoothing = 0.05f;
static constexpr float kMaxNormGain = 2.0f;
static constexpr float kGrainGainVariation = 0.25f;

static constexpr float kPitchIntervals[] = { 0.0f, 0.0f, 0.0f, 0.0f,
                                              7.0f, -12.0f, 12.0f, 5.0f, -7.0f };
static constexpr int kNumPitchIntervals = 9;

float GranularEngine::fastNoise(uint32_t& state)
{
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    return static_cast<float>(state) / static_cast<float>(UINT32_MAX) * 2.0f - 1.0f;
}

void GranularEngine::prepare(double newSampleRate, int /*samplesPerBlock*/)
{
    sampleRate = newSampleRate;
    const float sr = static_cast<float>(sampleRate);
    const float twoPi = 6.28318530718f;

    GrainEnvelopeLUT::initialize();

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
    feedbackLpAlpha = 1.0f - std::exp(-twoPi * 3000.0f / sr);

    writePos = 0; frozenWritePos = 0;
    samplesSinceLastGrain = 0; nextGrainInterval = 0;
    warmupSamples = 512; frozen = false;
    freezeFade = 0.0f; freezeTarget = false;
    freezeFadeInc = 1.0f / (0.01f * sr); // 10ms crossfade

    for (auto& g : grains) g.active = false;

    inputRmsState = 0.0f;
    rmsAlpha = 1.0f - std::exp(-twoPi * 30.0f / sr);
    inTailMode = false; tailSamplesRemaining = 0;
    tailDurationSamples = static_cast<int>(kTailDurationMs * 0.001f * sr);
    normSmoothed = 1.0f;

    posLfoPhase = 0.0f;   posLfoInc = twoPi * 0.1f / sr;
    sizeLfoPhase = 0.0f;  sizeLfoInc = twoPi * 0.07f / sr;
    pitchLfoPhase = 0.0f; pitchLfoInc = twoPi * 0.13f / sr;

    freezeJitterPhase = 0.0f;      freezeJitterInc = twoPi * 0.3f / sr;
    freezePitchJitterPhase = 0.0f;  freezePitchJitterInc = twoPi * 0.17f / sr;

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
    if (freeze && !freezeTarget) frozenWritePos = writePos;
    freezeTarget = freeze;
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

float GranularEngine::readHermite(int channel, float position) const
{
    int idx1 = static_cast<int>(std::floor(position)) % kInputBufferSize;
    if (idx1 < 0) idx1 += kInputBufferSize;
    const int idx0 = (idx1 - 1 + kInputBufferSize) % kInputBufferSize;
    const int idx2 = (idx1 + 1) % kInputBufferSize;
    const int idx3 = (idx1 + 2) % kInputBufferSize;
    const float frac = position - std::floor(position);
    const auto& buf = (*inputBuffer)[static_cast<size_t>(channel)];
    const float y0 = buf[static_cast<size_t>(idx0)], y1 = buf[static_cast<size_t>(idx1)];
    const float y2 = buf[static_cast<size_t>(idx2)], y3 = buf[static_cast<size_t>(idx3)];
    const float c1 = 0.5f * (y2 - y0);
    const float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
    const float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
    return ((c3 * frac + c2) * frac + c1) * frac + y1;
}

bool GranularEngine::isBufferSilent(float position, int lengthSamples) const
{
    constexpr int kCheckPoints = 8;
    const float step = static_cast<float>(lengthSamples) / static_cast<float>(kCheckPoints);
    float peak = 0.0f;
    for (int p = 0; p < kCheckPoints; ++p)
    {
        int idx = static_cast<int>(position + step * static_cast<float>(p)) % kInputBufferSize;
        if (idx < 0) idx += kInputBufferSize;
        peak = std::max(peak, std::abs((*inputBuffer)[0][static_cast<size_t>(idx)]));
    }
    return peak < kSilenceThreshold;
}

int GranularEngine::findNearestZeroCrossing(int startIdx, int searchRange) const
{
    int bestIdx = startIdx;
    float bestAbs = 999.0f;
    for (int off = -searchRange; off <= searchRange; ++off)
    {
        int idx = (startIdx + off + kInputBufferSize) % kInputBufferSize;
        float s = std::abs((*inputBuffer)[0][static_cast<size_t>(idx)]);
        if (s < bestAbs) { bestAbs = s; bestIdx = idx; }
    }
    return bestIdx;
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

    // Poisson scheduling
    const float rate = effectiveDensity * overlapFactor / static_cast<float>(grainSizeSamples);
    const float U = std::max(0.001f, fastNoise(noiseState) * 0.5f + 0.5f);
    nextGrainInterval = std::max(1, static_cast<int>(-std::log(U) / rate));

    const int spawnCount = effectiveDensity > 8.0f ? 3 : (effectiveDensity > 4.0f ? 2 : 1);
    int spawned = 0;

    for (auto& g : grains)
    {
        if (spawned >= spawnCount) break;
        if (g.active) continue;

        const float maxScatter = scatter * bufDepth * 0.3f;
        float startPos = readAnchor + fastNoise(noiseState) * maxScatter;

        // Freeze micro-variation: add position and pitch jitter
        if (frozen)
            startPos += std::sin(freezeJitterPhase) * sr * 0.020f; // ±20ms

        startPos = std::fmod(startPos + static_cast<float>(kInputBufferSize),
                             static_cast<float>(kInputBufferSize));

        // Zero-crossing alignment for small grains
        if (grainSizeSamples < static_cast<int>(sr * 0.01f))
        {
            const int searchRange = std::min(32, grainSizeSamples / 2);
            startPos = static_cast<float>(findNearestZeroCrossing(static_cast<int>(startPos), searchRange));
        }

        if (!frozen && isBufferSilent(startPos, grainSizeSamples))
            continue;

        g.currentPosition = startPos;

        // Pitch with quantized scatter
        float pitchRate = basePitchRate * (1.0f + pitchMod);
        if (frozen)
            pitchRate *= 1.0f + std::sin(freezePitchJitterPhase) * 0.015f; // ±25 cents

        if (scatter > 0.3f)
        {
            const float chance = (scatter - 0.3f) * 0.3f;
            if (std::abs(fastNoise(noiseState)) < chance)
            {
                const int ci = std::max(0, std::min(kNumPitchIntervals - 1,
                    static_cast<int>((fastNoise(noiseState) * 0.5f + 0.5f) * static_cast<float>(kNumPitchIntervals))));
                pitchRate *= std::pow(2.0f, kPitchIntervals[ci] / 12.0f);
            }
        }
        g.playbackRate = pitchRate * std::pow(2.0f, fastNoise(noiseState) * 5.0f / 1200.0f);

        // Reverse
        g.reverse = (fastNoise(noiseState) * 0.5f + 0.5f) < currentDriftRaw * 0.25f;
        if (g.reverse) g.playbackRate = -std::abs(g.playbackRate);

        // Pan + gain
        const float pan = 0.5f + fastNoise(noiseState) * 0.5f * stereoWidth;
        g.panL = std::cos(pan * 1.5707963f);
        g.panR = std::sin(pan * 1.5707963f);
        g.gainMul = 1.0f + fastNoise(noiseState) * kGrainGainVariation;

        // Length
        g.lengthInSamples = std::max(16, static_cast<int>(
            static_cast<float>(grainSizeSamples) * (1.0f + fastNoise(noiseState) * 0.2f)));
        g.currentSample = 0;

        // Pitch drift
        const float driftCents = fastNoise(noiseState) * 10.0f * scatter;
        const float driftTarget = g.playbackRate * std::pow(2.0f, driftCents / 1200.0f);
        g.pitchDriftRate = (driftTarget - g.playbackRate) / static_cast<float>(g.lengthInSamples);

        // AA biquad
        const float absRate = std::abs(g.playbackRate);
        if (absRate > 1.05f)
        {
            const float w0 = twoPi * std::min(sr * 0.45f, (sr * 0.45f) / absRate) / sr;
            const float alpha = std::sin(w0) / 1.414f;
            const float cosw0 = std::cos(w0);
            const float a0 = 1.0f + alpha;
            g.aaB0 = ((1.0f - cosw0) * 0.5f) / a0;
            g.aaB1 = (1.0f - cosw0) / a0; g.aaB2 = g.aaB0;
            g.aaA1 = (-2.0f * cosw0) / a0; g.aaA2 = (1.0f - alpha) / a0;
            g.aaActive = true;
        }
        else g.aaActive = false;
        g.aaS1[0] = g.aaS1[1] = g.aaS2[0] = g.aaS2[1] = 0.0f;

        // SVF mode (scatter-adaptive)
        g.svfActive = true;
        if (scatter < 0.3f)
        {
            g.svfBaseFreq = 1000.0f + effectiveDensity * 600.0f;
            g.svfRange = 8000.0f; g.svfQ = 1.2f;
        }
        else if (scatter < 0.7f)
        {
            g.svfBaseFreq = std::max(500.0f, absRate * 2000.0f);
            g.svfRange = 6000.0f; g.svfQ = 0.8f + (fastNoise(noiseState) * 0.5f + 0.5f) * 0.8f;
        }
        else
        {
            g.svfBaseFreq = 2000.0f + (fastNoise(noiseState) * 0.5f + 0.5f) * 10000.0f;
            g.svfRange = 3000.0f + (fastNoise(noiseState) * 0.5f + 0.5f) * 8000.0f;
            g.svfQ = 0.7f + (fastNoise(noiseState) * 0.5f + 0.5f) * 1.5f;
        }
        g.svfIc1[0] = g.svfIc1[1] = g.svfIc2[0] = g.svfIc2[1] = 0.0f;
        g.svfUpdateCounter = 0;

        // Decorrelation allpass
        g.decorrelCoeff = 0.3f + (fastNoise(noiseState) * 0.5f + 0.5f) * 0.5f;
        g.decorrelState = 0.0f;

        g.active = true;
        ++spawned;
    }
}

// ============================================================================
// Per-grain processing (extracted from process loop)
// ============================================================================
void GranularEngine::processGrain(Grain& g, bool isStereo, float sr, float envFollow,
                                   float& outL, float& outR)
{
    if (g.lengthInSamples < 1) { g.active = false; return; }

    const float phase = static_cast<float>(g.currentSample) / static_cast<float>(g.lengthInSamples);

    // Auto-shape from Cloud: smooth blend between envelope shapes
    // Low cloud (0→0.5): Hanning→Gaussian, High cloud (0.5→1): Gaussian→Trapezoid
    float envelope;
    if (currentCloud < 0.5f)
    {
        envelope = GrainEnvelopeLUT::lookupBlended(
            GrainEnvelopeLUT::Hanning, GrainEnvelopeLUT::Gaussian,
            currentCloud * 2.0f, phase);
    }
    else
    {
        envelope = GrainEnvelopeLUT::lookupBlended(
            GrainEnvelopeLUT::Gaussian, GrainEnvelopeLUT::Trapezoid,
            (currentCloud - 0.5f) * 2.0f, phase);
    }

    // Enhanced short-grain envelope
    if (currentCloud < 0.3f && g.lengthInSamples < static_cast<int>(sr * 0.01f))
        envelope = std::sqrt(envelope * envelope * envelope);

    // Read from buffer
    float sL = readHermite(0, g.currentPosition);
    float sR = isStereo ? readHermite(1, g.currentPosition) : sL;

    // AA biquad
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
            g.svfCachedG0 = std::tan(3.14159265f * cutoff / sr);
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

    // Advance grain
    g.currentPosition += g.playbackRate;
    g.playbackRate += g.pitchDriftRate;
    g.currentPosition = std::fmod(g.currentPosition + static_cast<float>(kInputBufferSize),
                                  static_cast<float>(kInputBufferSize));
    g.currentSample++;
    if (g.currentSample >= g.lengthInSamples) g.active = false;
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
        const float freezeGoal = freezeTarget ? 1.0f : 0.0f;
        if (freezeFade < freezeGoal)
            freezeFade = std::min(freezeGoal, freezeFade + freezeFadeInc);
        else if (freezeFade > freezeGoal)
            freezeFade = std::max(freezeGoal, freezeFade - freezeFadeInc);
        frozen = freezeFade > 0.5f;

        // Update Dattorro reverb from texture parameter
        dattorro.setDecay(0.2f + texture * 0.78f);
        dattorro.setDamping(1.0f - texture * 0.7f);
        dattorro.setDiffusion(0.4f + texture * 0.5f);

        const float densityRateScale = 1.5f - std::min(density, 12.0f) / 12.0f;
        const float sizeModDepth = scatter * 0.15f;
        const float sizeMod = 1.0f + std::sin(sizeLfoPhase) * sizeModDepth;
        const float pitchMod = std::sin(pitchLfoPhase) * scatter * 0.003f;

        const int grainSizeSamples = std::max(16, static_cast<int>(grainSizeMs * sizeMod * 0.001f * sr));
        const float overlapFactor = 2.0f + (std::min(density, 16.0f) / 16.0f) * (kMaxOverlap - 2.0f);

        // Write input (crossfaded during freeze transition)
        const float writeMix = 1.0f - freezeFade;
        if (writeMix > 0.001f)
        {
            for (int ch = 0; ch < numChannels; ++ch)
            {
                float in = buffer.getReadPointer(ch)[i] + feedbackBuffer[ch] * feedback;
                in = std::tanh(in);
                auto& bufSample = (*inputBuffer)[static_cast<size_t>(ch)][static_cast<size_t>(writePos)];
                bufSample = bufSample * (1.0f - writeMix) + in * writeMix;
            }
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
            if (!frozen) writePos = (writePos + 1) % kInputBufferSize;
            for (int ch = 0; ch < numChannels; ++ch)
                buffer.getWritePointer(ch)[i] = 0.0f;
            continue;
        }

        // Read anchor
        const int ewp = frozen ? frozenWritePos : writePos;
        const float bufDepth = static_cast<float>(kInputBufferSize) * 0.9f;
        const float lfoOff = std::sin(posLfoPhase) * 0.02f;
        const float effPos = std::max(0.0f, std::min(1.0f, position + lfoOff));
        float readAnchor = static_cast<float>(ewp) - (1.0f - effPos) * bufDepth;
        if (readAnchor < 0.0f) readAnchor += static_cast<float>(kInputBufferSize);

        // Grain scheduling
        ++samplesSinceLastGrain;
        const bool canSpawn = mix > 0.001f && effectiveDensity > 0.01f
                              && !(inTailMode && tailSamplesRemaining <= 0);

        if (samplesSinceLastGrain >= nextGrainInterval && canSpawn)
        {
            samplesSinceLastGrain = 0;
            spawnGrains(readAnchor, scatter, basePitchRate, pitchMod,
                        grainSizeSamples, effectiveDensity, overlapFactor, stereoWidth, isStereo);
        }

        // Input envelope following (bypass during freeze)
        const float envFollow = frozen ? 1.0f : (0.3f + 0.7f * inputFollower.getEnvelope());

        // Process active grains
        float grainOutputL = 0.0f, grainOutputR = 0.0f;
        for (auto& g : grains)
        {
            if (!g.active) continue;
            processGrain(g, isStereo, sr, envFollow, grainOutputL, grainOutputR);
        }

        // Density normalization
        const float densNorm = 1.0f / std::sqrt(std::max(1.0f, effectiveDensity * overlapFactor));
        normSmoothed += kNormSmoothing * (std::min(kMaxNormGain, densNorm) - normSmoothed);
        grainOutputL *= normSmoothed;
        grainOutputR *= normSmoothed;

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
                *s = *s * (1.0f - knee) + std::tanh(*s) * knee;
            }
        }

        // Feedback (LP filtered)
        feedbackLpState[0] += feedbackLpAlpha * (grainOutputL - feedbackLpState[0]);
        feedbackBuffer[0] = feedbackLpState[0];
        if (isStereo)
        {
            feedbackLpState[1] += feedbackLpAlpha * (grainOutputR - feedbackLpState[1]);
            feedbackBuffer[1] = feedbackLpState[1];
        }

        // Dry/wet (precompute once — mix changes slowly)
        const float dryGain = std::cos(mix * 1.5707963f);
        const float wetGain = std::sin(mix * 1.5707963f);
        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto* data = buffer.getWritePointer(ch);
            data[i] = data[i] * dryGain + ((ch == 0) ? grainOutputL : grainOutputR) * wetGain;
        }

        if (!frozen) writePos = (writePos + 1) % kInputBufferSize;

        // Advance LFOs (rate scales with density)
        posLfoPhase += posLfoInc * densityRateScale; if (posLfoPhase >= twoPi) posLfoPhase -= twoPi;
        sizeLfoPhase += sizeLfoInc * densityRateScale; if (sizeLfoPhase >= twoPi) sizeLfoPhase -= twoPi;
        pitchLfoPhase += pitchLfoInc * densityRateScale; if (pitchLfoPhase >= twoPi) pitchLfoPhase -= twoPi;

        // Freeze jitter LFOs (only advance when frozen)
        if (frozen)
        {
            freezeJitterPhase += freezeJitterInc; if (freezeJitterPhase >= twoPi) freezeJitterPhase -= twoPi;
            freezePitchJitterPhase += freezePitchJitterInc; if (freezePitchJitterPhase >= twoPi) freezePitchJitterPhase -= twoPi;
        }
    }
}
