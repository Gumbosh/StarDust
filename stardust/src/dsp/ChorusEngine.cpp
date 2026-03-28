#include "ChorusEngine.h"

static constexpr int kOscNormalizeInterval = 512;
static constexpr float kPanSmoothFreq = 80.0f; // ~2ms time constant for pan smoothing

void ChorusEngine::prepare(double newSampleRate, int /*samplesPerBlock*/)
{
    sampleRate = newSampleRate;
    mixSmoothed.reset(sampleRate, 0.02);
    speedSmoothed.reset(sampleRate, 0.02);

    for (auto& ch : delayBuffer) ch.fill(0.0f);
    writePos = 0;
    oscNormCounter = 0;

    const float sr = static_cast<float>(sampleRate);
    const float pi = juce::MathConstants<float>::pi;

    // Sample-rate-dependent pan smoothing (~2ms time constant)
    panSmoothAlpha = 1.0f - std::exp(-juce::MathConstants<float>::twoPi * kPanSmoothFreq / sr);

    // --- EQ coefficients ---
    // High-pass ~50Hz (Butterworth)
    {
        const float w0 = 2.0f * pi * kHpFreq / sr;
        const float cosW0 = std::cos(w0);
        const float alpha = std::sin(w0) / (2.0f * kSqrt2);
        const float a0 = 1.0f + alpha;
        hpB0 = ((1.0f + cosW0) / 2.0f) / a0;
        hpB1 = -(1.0f + cosW0) / a0;
        hpB2 = ((1.0f + cosW0) / 2.0f) / a0;
        hpA1 = (-2.0f * cosW0) / a0;
        hpA2 = (1.0f - alpha) / a0;
    }

    // Low-pass ~9kHz (Butterworth)
    {
        const float w0 = 2.0f * pi * kLpFreq / sr;
        const float cosW0 = std::cos(w0);
        const float alpha = std::sin(w0) / (2.0f * kSqrt2);
        const float a0 = 1.0f + alpha;
        lpB0 = ((1.0f - cosW0) / 2.0f) / a0;
        lpB1 = (1.0f - cosW0) / a0;
        lpB2 = ((1.0f - cosW0) / 2.0f) / a0;
        lpA1 = (-2.0f * cosW0) / a0;
        lpA2 = (1.0f - alpha) / a0;
    }

    for (auto& s : hpState) s = {};
    for (auto& s : lpState) s = {};

    // --- Voice setup ---
    static constexpr float kBaseDelaysMs[kNumVoices] = { 1.0f, 3.0f, 5.5f, 8.0f };
    static constexpr float kPans[kNumVoices] = { 0.1f, 0.9f, 0.25f, 0.75f };

    static constexpr int kApDelays[kNumVoices][kAllpassStages] = {
        { 13, 29, 41, 67, 89, 113, 149, 179 },
        { 17, 37, 53, 73, 97, 127, 157, 191 },
        { 19, 31, 47, 71, 101, 131, 163, 197 },
        { 23, 43, 59, 79, 107, 137, 167, 199 }
    };

    static constexpr float kApCoeffs[kAllpassStages] = {
        0.55f, 0.62f, 0.48f, 0.67f, 0.53f, 0.60f, 0.45f, 0.58f
    };

    for (int i = 0; i < kNumVoices; ++i)
    {
        auto& v = voices[static_cast<size_t>(i)];
        const float voiceIdx = static_cast<float>(i);
        const float phaseOffset = voiceIdx / static_cast<float>(kNumVoices)
                                  * juce::MathConstants<float>::twoPi;

        v.baseDelaySamples = kBaseDelaysMs[i] * 0.001f * sr;
        v.pan = kPans[i];
        v.panTarget.store(kPans[i], std::memory_order_relaxed);

        // Pre-compute initial pan gains
        const float panAngle = v.pan * juce::MathConstants<float>::halfPi;
        v.panGainL = std::cos(panAngle);
        v.panGainR = std::sin(panAngle);

        // FM LFO: ~1Hz with slight voice spread
        const float fmRate = 1.0f + (voiceIdx - 1.5f) * 0.1f;
        v.fmLfo.initWithPhase(fmRate, sr, phaseOffset);
        v.fmBaseRate = fmRate;

        // AM LFO: subtle shimmer
        const float amRate = 0.8f + voiceIdx * 0.15f;
        v.amLfo.initWithPhase(amRate, sr, phaseOffset + juce::MathConstants<float>::halfPi);
        v.amBaseRate = amRate;

        // Allpass chain with true multi-sample delay buffers
        for (int s = 0; s < kAllpassStages; ++s)
        {
            auto& ap = v.allpass[static_cast<size_t>(s)];
            ap.coeff = kApCoeffs[s];
            ap.delayLen = std::clamp(static_cast<int>(kApDelays[i][s] * sr / 44100.0f), 1, kMaxAllpassDelay - 1);
            ap.writeIdx = 0;
            std::fill(std::begin(ap.bufferL), std::end(ap.bufferL), 0.0f);
            std::fill(std::begin(ap.bufferR), std::end(ap.bufferR), 0.0f);
        }
    }
}

void ChorusEngine::setMix(float mix)
{
    if (currentMix != mix) { currentMix = mix; mixSmoothed.setTargetValue(mix); }
}

void ChorusEngine::setSpeed(float speed)
{
    if (currentSpeed != speed) { currentSpeed = speed; speedSmoothed.setTargetValue(speed); }
}

void ChorusEngine::setPans(float outer, float inner)
{
    voices[0].panTarget.store(0.5f - outer * 0.5f, std::memory_order_relaxed);
    voices[1].panTarget.store(0.5f + outer * 0.5f, std::memory_order_relaxed);
    voices[2].panTarget.store(0.5f - inner * 0.5f, std::memory_order_relaxed);
    voices[3].panTarget.store(0.5f + inner * 0.5f, std::memory_order_relaxed);
}

// Hermite 4-point interpolation with bitmask wrapping
float ChorusEngine::readHermite(int channel, float delaySamples) const
{
    const float readPos = static_cast<float>(writePos) - delaySamples;
    const int idx1 = static_cast<int>(std::floor(readPos)) & kDelayMask;
    const int idx0 = (idx1 - 1) & kDelayMask;
    const int idx2 = (idx1 + 1) & kDelayMask;
    const int idx3 = (idx1 + 2) & kDelayMask;
    const float frac = readPos - std::floor(readPos);
    const auto& buf = delayBuffer[static_cast<size_t>(channel)];
    const float y0 = buf[idx0], y1 = buf[idx1], y2 = buf[idx2], y3 = buf[idx3];
    const float c1 = 0.5f * (y2 - y0);
    const float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
    const float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
    return ((c3 * frac + c2) * frac + c1) * frac + y1;
}

float ChorusEngine::processBiquad(float input, float b0, float b1, float b2,
                                   float a1, float a2, BiquadState& state)
{
    const float output = b0 * input + state.z1;
    state.z1 = b1 * input - a1 * output + state.z2;
    state.z2 = b2 * input - a2 * output;
    return output;
}

void ChorusEngine::process(juce::AudioBuffer<float>& buffer)
{
    juce::ScopedNoDenormals noDenormals;
    const int numChannels = std::min(buffer.getNumChannels(), kMaxChannels);
    const auto numSamples = buffer.getNumSamples();
    const bool isStereo = numChannels > 1;
    const float fmDepthSamples = kFmDepthMs * 0.001f * static_cast<float>(sampleRate);

    // Hoist pointers outside sample loop
    auto* dataL = buffer.getWritePointer(0);
    auto* dataR = isStereo ? buffer.getWritePointer(1) : nullptr;
    const auto* readL = buffer.getReadPointer(0);
    const auto* readR = isStereo ? buffer.getReadPointer(1) : nullptr;

    for (int i = 0; i < numSamples; ++i)
    {
        const float mix = mixSmoothed.getNextValue();
        const float speedMul = speedSmoothed.getNextValue();

        // Write input to delay buffer (bitmask wrap)
        delayBuffer[0][static_cast<size_t>(writePos)] = readL[i];
        if (isStereo)
            delayBuffer[1][static_cast<size_t>(writePos)] = readR[i];

        if (mix < 0.001f)
        {
            writePos = (writePos + 1) & kDelayMask;
            // Write silence through allpass buffers to prevent stale data transient on mix-in (fix 1.5)
            for (auto& v : voices)
                for (auto& ap : v.allpass)
                {
                    ap.bufferL[ap.writeIdx] = 0.0f;
                    if (isStereo) ap.bufferR[ap.writeIdx] = 0.0f;
                    ap.writeIdx = (ap.writeIdx + 1) & kApMask;
                }
            // Still advance LFOs to prevent phase pop on mix-in (fix 2.13)
            for (auto& v : voices) { v.fmLfo.advance(); v.amLfo.advance(); }
            if (++oscNormCounter >= kOscNormalizeInterval)
            {
                oscNormCounter = 0;
                const float curSpeed = speedSmoothed.getCurrentValue();
                for (auto& v : voices)
                {
                    v.fmLfo.normalize(); v.amLfo.normalize();
                    v.fmLfo.setFreq(v.fmBaseRate * curSpeed, static_cast<float>(sampleRate));
                    v.amLfo.setFreq(v.amBaseRate * curSpeed, static_cast<float>(sampleRate));
                }
            }
            continue;
        }

        float outL = 0.0f;
        float outR = 0.0f;

        for (auto& v : voices)
        {
            // Smooth pan and update cached gains only when pan changes
            const float panDiff = v.panTarget.load(std::memory_order_relaxed) - v.pan;
            if (std::abs(panDiff) > 1e-6f)
            {
                v.pan += panSmoothAlpha * panDiff;
                const float panAngle = v.pan * juce::MathConstants<float>::halfPi;
                v.panGainL = std::cos(panAngle);
                v.panGainR = std::sin(panAngle);
            }

            // FM: modulate delay time (incremental oscillator)
            const float delaySamples = v.baseDelaySamples + v.fmLfo.sinVal * fmDepthSamples;

            // Hermite-interpolated delay read
            float sampleL = readHermite(0, delaySamples);
            float sampleR = isStereo ? readHermite(1, delaySamples) : sampleL;

            // Delayed allpass chain for true phase diffusion (bitmask wrap)
            for (auto& ap : v.allpass)
            {
                const int readIdx = (ap.writeIdx - ap.delayLen) & kApMask;
                const float delL = ap.bufferL[readIdx];

                const float outApL = -ap.coeff * sampleL + delL;
                ap.bufferL[ap.writeIdx] = sampleL + ap.coeff * outApL;
                sampleL = outApL;

                if (isStereo)
                {
                    const float delR = ap.bufferR[readIdx];
                    const float outApR = -ap.coeff * sampleR + delR;
                    ap.bufferR[ap.writeIdx] = sampleR + ap.coeff * outApR;
                    sampleR = outApR;
                }

                ap.writeIdx = (ap.writeIdx + 1) & kApMask;
            }

            // AM: subtle volume shimmer
            const float ampMod = 1.0f - kAmDepth + kAmDepth * (0.5f + 0.5f * v.amLfo.sinVal);
            sampleL *= ampMod;
            sampleR *= ampMod;

            // Use cached constant-power pan gains (zero trig per sample)
            outL += sampleL * v.panGainL * kVoiceGain;
            outR += sampleR * v.panGainR * kVoiceGain;

            // Advance LFOs (frequency updated when speed changes)
            v.fmLfo.advance();
            v.amLfo.advance();
        }

        // EQ on wet signal: HP 50Hz + LP 9kHz
        outL = processBiquad(outL, hpB0, hpB1, hpB2, hpA1, hpA2, hpState[0]);
        outL = processBiquad(outL, lpB0, lpB1, lpB2, lpA1, lpA2, lpState[0]);
        if (isStereo)
        {
            outR = processBiquad(outR, hpB0, hpB1, hpB2, hpA1, hpA2, hpState[1]);
            outR = processBiquad(outR, lpB0, lpB1, lpB2, lpA1, lpA2, lpState[1]);
        }

        // Constant-power crossfade: quadratic approximation (max error < 0.01, zero trig)
        // sin(x*pi/2) ≈ x*(1.8284 - 0.8284*x), exact at x=0, 0.5, 1
        const float wetGain = mix * (1.8284f - 0.8284f * mix);           // quadratic ≈ sin(mix*pi/2)
        const float omix = 1.0f - mix;
        const float dryGain = omix * (1.8284f - 0.8284f * omix);        // quadratic ≈ cos(mix*pi/2)

        dataL[i] = dataL[i] * dryGain + outL * wetGain;
        if (isStereo)
            dataR[i] = dataR[i] * dryGain + outR * wetGain;

        writePos = (writePos + 1) & kDelayMask;

        // Periodic oscillator normalization + speed-based frequency update
        if (++oscNormCounter >= kOscNormalizeInterval)
        {
            oscNormCounter = 0;
            const float curSpeed = speedSmoothed.getCurrentValue();
            for (auto& v : voices)
            {
                v.fmLfo.normalize();
                v.amLfo.normalize();
                // Update LFO frequencies when speed changes (fix 2.11)
                v.fmLfo.setFreq(v.fmBaseRate * curSpeed, static_cast<float>(sampleRate));
                v.amLfo.setFreq(v.amBaseRate * curSpeed, static_cast<float>(sampleRate));
            }
        }
    }
}
