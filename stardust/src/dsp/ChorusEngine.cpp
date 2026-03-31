#include "ChorusEngine.h"

void ChorusEngine::prepare(double newSampleRate, int /*samplesPerBlock*/)
{
    sampleRate = newSampleRate;

    // Build sin LUT (PERF-3)
    for (int i = 0; i < kSinLUTSize; ++i)
        lfoSinLUT[i] = std::sin(static_cast<float>(i) * juce::MathConstants<float>::twoPi
                                / static_cast<float>(kSinLUTSize));
    mixSmoothed.reset(sampleRate, 0.02);
    speedSmoothed.reset(sampleRate, 0.02);
    feedbackSmoothed.reset(sampleRate, 0.02);
    for (int ch = 0; ch < kMaxChannels; ++ch) feedbackState[ch] = 0.0f;

    for (auto& ch : delayBuffer)
        ch.fill(0.0f);
    writePos = 0;

    const float sr = static_cast<float>(sampleRate);
    const float pi = juce::MathConstants<float>::pi;

    // --- EQ coefficients for wet signal ---

    // High-pass ~50Hz (low cut) — 2nd order Butterworth
    {
        const float freq = 50.0f;
        const float w0 = 2.0f * pi * freq / sr;
        const float cosW0 = std::cos(w0);
        const float alpha = std::sin(w0) / (2.0f * 0.707f);
        const float a0 = 1.0f + alpha;
        hpB0 = ((1.0f + cosW0) / 2.0f) / a0;
        hpB1 = -(1.0f + cosW0) / a0;
        hpB2 = ((1.0f + cosW0) / 2.0f) / a0;
        hpA1 = (-2.0f * cosW0) / a0;
        hpA2 = (1.0f - alpha) / a0;
    }

    // Low-pass ~5kHz (high shelf rolloff) — 2nd order Butterworth
    {
        const float freq = 8000.0f;
        const float w0 = 2.0f * pi * freq / sr;
        const float cosW0 = std::cos(w0);
        const float alpha = std::sin(w0) / (2.0f * 0.707f);
        const float a0 = 1.0f + alpha;
        lpB0 = ((1.0f - cosW0) / 2.0f) / a0;
        lpB1 = (1.0f - cosW0) / a0;
        lpB2 = ((1.0f - cosW0) / 2.0f) / a0;
        lpA1 = (-2.0f * cosW0) / a0;
        lpA2 = (1.0f - alpha) / a0;
    }

    for (auto& s : hpState) { s.z1 = 0.0f; s.z2 = 0.0f; }
    for (auto& s : lpState) { s.z1 = 0.0f; s.z2 = 0.0f; }

    // BBD bucket-capacitor HF rolloff: 1-pole LP at ~8kHz (Roland Juno/Dimension D style)
    bbdLPCoeff = std::exp(-2.0f * pi * 8000.0f / sr);
    for (auto& v : voices) { v.bbdLP[0] = 0.0f; v.bbdLP[1] = 0.0f; }

    // Input envelope follower coefficients (5ms attack, 100ms release)
    envAttackAlpha  = 1.0f - std::exp(-1.0f / (sr * 0.005f));
    envReleaseAlpha = 1.0f - std::exp(-1.0f / (sr * 0.100f));
    inputEnvelope = 0.0f;

    // --- Voice setup ---

    // Tighter delay spread (closer to 0ms pre-delay like Multiply default)
    const float baseDelaysMs[kNumVoices] = { 1.0f, 3.0f, 5.5f, 8.0f };
    const float pans[kNumVoices] = { 0.0f, 1.0f, 0.2f, 0.8f };

    const float allpassCoeffs[kAllpassStages] = {
        0.55f, 0.62f, 0.48f, 0.67f, 0.53f, 0.60f, 0.45f, 0.58f
    };

    const int allpassDelays[kNumVoices][kAllpassStages] = {
        { 13, 29, 41, 67, 89, 113, 149, 179 },
        { 17, 37, 53, 73, 97, 127, 157, 191 },
        { 19, 31, 47, 71, 101, 131, 163, 197 },
        { 23, 43, 59, 79, 107, 137, 167, 199 }
    };

    for (int i = 0; i < kNumVoices; ++i)
    {
        auto& v = voices[static_cast<size_t>(i)];
        const float voiceIdx = static_cast<float>(i);
        const float phaseOffset = voiceIdx / static_cast<float>(kNumVoices)
                                  * juce::MathConstants<float>::twoPi;

        v.baseDelaySamples = baseDelaysMs[i] * 0.001f * sr;
        v.pan = pans[i];

        // Subtle FM: ~1Hz, 20% depth of 1ms = 0.2ms modulation
        // Just enough to add life without audible pitch shift
        v.fmLfoRate = 1.0f + (voiceIdx - 1.5f) * 0.1f; // 0.85, 0.95, 1.05, 1.15 Hz
        v.fmLfoRateBase   = v.fmLfoRate;
        v.fmLfoRateTarget = v.fmLfoRate;
        v.lfoRateDriftCounter = 0;
        v.fmLfoPhase = phaseOffset;
        v.fmDepthSamples = 0.2f * 0.001f * sr; // 0.2ms

        // AM: volume shimmer
        v.amLfoRate = 0.8f + voiceIdx * 0.15f;
        v.amLfoPhase = phaseOffset + juce::MathConstants<float>::halfPi;
        v.amDepth = 0.3f;

        // Allpass chain
        for (int s = 0; s < kAllpassStages; ++s)
        {
            auto& ap = v.allpass[static_cast<size_t>(s)];
            ap.coeff = allpassCoeffs[s] + static_cast<float>(allpassDelays[i][s] % 7) * 0.02f;
            ap.coeff = juce::jlimit(0.3f, 0.8f, ap.coeff);
            ap.bufferL = 0.0f;
            ap.bufferR = 0.0f;
        }
    }

    // Seed each voice's jitter PRNG with a different value for decorrelated noise
    for (int i = 0; i < kNumVoices; ++i)
        voices[static_cast<size_t>(i)].jitterPrng = 12345u + static_cast<uint32_t>(i) * 6364136223846793005u;

    // Voice 4 — extra width, slightly slower LFO
    {
        auto& v = voices[4];
        v.baseDelaySamples = 11.0f * 0.001f * sr;
        v.pan = 0.35f; v.panTarget = 0.35f;
        v.fmLfoPhase  = juce::MathConstants<float>::pi * 4.0f / 4.0f;
        v.fmLfoRate   = 0.78f;
        v.fmLfoRateBase   = 0.78f;
        v.fmLfoRateTarget = 0.78f;
        v.lfoRateDriftCounter = 0;
        v.fmDepthSamples = 0.2f * 0.001f * sr;
        v.amLfoRate   = 0.92f; v.amDepth = 0.2f;
        v.amLfoPhase  = juce::MathConstants<float>::halfPi;
        v.shHoldValue = 0.0f; v.shPrevPhase = 0.0f;
        for (int s = 0; s < kAllpassStages; ++s)
        {
            v.allpass[static_cast<size_t>(s)].coeff
                = juce::jlimit(0.3f, 0.8f, 0.45f + static_cast<float>(s) * 0.04f + 0.03f);
            v.allpass[static_cast<size_t>(s)].bufferL = 0.0f;
            v.allpass[static_cast<size_t>(s)].bufferR = 0.0f;
        }
    }

    // Voice 5 — extra width, slightly faster LFO
    {
        auto& v = voices[5];
        v.baseDelaySamples = 13.0f * 0.001f * sr;
        v.pan = 0.65f; v.panTarget = 0.65f;
        v.fmLfoPhase  = juce::MathConstants<float>::pi * 5.0f / 4.0f;
        v.fmLfoRate   = 1.22f;
        v.fmLfoRateBase   = 1.22f;
        v.fmLfoRateTarget = 1.22f;
        v.lfoRateDriftCounter = 0;
        v.fmDepthSamples = 0.2f * 0.001f * sr;
        v.amLfoRate   = 1.18f; v.amDepth = 0.2f;
        v.amLfoPhase  = juce::MathConstants<float>::halfPi + juce::MathConstants<float>::pi;
        v.shHoldValue = 0.0f; v.shPrevPhase = 0.0f;
        for (int s = 0; s < kAllpassStages; ++s)
        {
            v.allpass[static_cast<size_t>(s)].coeff
                = juce::jlimit(0.3f, 0.8f, 0.48f + static_cast<float>(s) * 0.04f + 0.05f);
            v.allpass[static_cast<size_t>(s)].bufferL = 0.0f;
            v.allpass[static_cast<size_t>(s)].bufferR = 0.0f;
        }
    }

    // Initialize per-voice allpass drift (very slow, 0.03–0.09 Hz oscillator)
    for (int i = 0; i < kNumVoices; ++i)
    {
        auto& v = voices[static_cast<size_t>(i)];
        // Copy current coefficients as initial targets
        for (int s = 0; s < kAllpassStages; ++s)
            v.allpassTarget[s] = v.allpass[s].coeff;
        v.driftRate  = 0.03f + static_cast<float>(i) * 0.012f;  // 0.030, 0.042, 0.054, 0.066, 0.078, 0.090 Hz
        v.driftPhase = static_cast<float>(i) * 0.9f;            // stagger phases so voices drift independently
    }
}

void ChorusEngine::setMix(float mix)
{
    if (currentMix != mix)
    {
        currentMix = mix;
        mixSmoothed.setTargetValue(mix);
    }
}

void ChorusEngine::setSpeed(float speed)
{
    if (currentSpeed != speed)
    {
        currentSpeed = speed;
        speedSmoothed.setTargetValue(speed);
    }
}

void ChorusEngine::setDepth(float d)
{
    const float sr = static_cast<float>(sampleRate);
    const float depthMs = 0.01f + d * 1.49f; // 0.01ms–1.5ms
    for (auto& v : voices)
        v.fmDepthSamples = depthMs * 0.001f * sr;
}

void ChorusEngine::setTone(float t)
{
    // LP cutoff: 3kHz (t=0) → 20kHz (t=1), exponential sweep
    const float sr = static_cast<float>(sampleRate);
    const float freq = 3000.0f * std::pow(20000.0f / 3000.0f, t);
    const float pi = juce::MathConstants<float>::pi;
    const float w0 = 2.0f * pi * freq / sr;
    const float cosW0 = std::cos(w0);
    const float alpha = std::sin(w0) / (2.0f * 0.707f);
    const float a0 = 1.0f + alpha;
    lpB0 = ((1.0f - cosW0) / 2.0f) / a0;
    lpB1 = (1.0f - cosW0) / a0;
    lpB2 = ((1.0f - cosW0) / 2.0f) / a0;
    lpA1 = (-2.0f * cosW0) / a0;
    lpA2 = (1.0f - alpha) / a0;
}

void ChorusEngine::setFeedback(float fb)
{
    if (currentFeedback != fb)
    {
        currentFeedback = fb;
        feedbackSmoothed.setTargetValue(fb);
    }
}

void ChorusEngine::setLfoShape(int shape)
{
    currentLfoShape = juce::jlimit(0, 2, shape);
}

void ChorusEngine::setShimmer(float s)
{
    for (auto& v : voices)
        v.amDepth = s;  // full range 0–1 (was capped at 35%)
}

void ChorusEngine::setVintageMode(int mode)
{
    // Determine stage count for this mode (hardware-accurate: Juno=6, DimD=4, TriCh=6)
    const int stageCount = (mode == 0) ? kAllpassStages
                         : (mode == 2) ? 4   // Dimension D
                         : 6;               // Juno-60 and Tri-Chorus
    if (stageCount != currentAllpassStages)
    {
        currentAllpassStages = stageCount;
        // Zero allpass state beyond active stages to prevent clicks on mode switch
        for (auto& v : voices)
            for (int s = stageCount; s < kAllpassStages; ++s)
            {
                v.allpass[static_cast<size_t>(s)].bufferL = 0.0f;
                v.allpass[static_cast<size_t>(s)].bufferR = 0.0f;
            }
    }
    if (mode == 0) return; // Custom — user controls all base params
    const float sr = static_cast<float>(sampleRate);
    if (mode == 1) // Juno-60: wide pan, slow 0.5Hz LFO, 15-20ms delays
    {
        voices[0].baseDelaySamples = 15.0f * 0.001f * sr; voices[0].pan = 0.1f; voices[0].panTarget = 0.1f;
        voices[0].fmLfoRate = 0.5f; voices[0].fmLfoRateBase = 0.5f; voices[0].fmLfoRateTarget = 0.5f;
        voices[0].fmDepthSamples = 0.5f * 0.001f * sr;
        voices[1].baseDelaySamples = 20.0f * 0.001f * sr; voices[1].pan = 0.9f; voices[1].panTarget = 0.9f;
        voices[1].fmLfoRate = 0.5f; voices[1].fmLfoRateBase = 0.5f; voices[1].fmLfoRateTarget = 0.5f;
        voices[1].fmDepthSamples = 0.5f * 0.001f * sr;
    }
    else if (mode == 2) // Dimension D: narrow pan, 1Hz LFO, 10-13ms delays
    {
        const float delays[] = { 10.0f, 11.5f, 12.0f, 13.0f };
        const float pans[]   = { 0.2f,  0.4f,  0.6f,  0.8f  };
        for (int i = 0; i < 4 && i < kNumVoices; ++i)
        {
            voices[static_cast<size_t>(i)].baseDelaySamples = delays[i] * 0.001f * sr;
            voices[static_cast<size_t>(i)].pan = pans[i];
            voices[static_cast<size_t>(i)].panTarget = pans[i];
            voices[static_cast<size_t>(i)].fmLfoRate = 1.0f;
            voices[static_cast<size_t>(i)].fmLfoRateBase = 1.0f;
            voices[static_cast<size_t>(i)].fmLfoRateTarget = 1.0f;
            voices[static_cast<size_t>(i)].fmDepthSamples = 0.15f * 0.001f * sr;
        }
    }
    else if (mode == 3) // Tri-Chorus: 6 voices, medium pan, 1.5Hz LFO, 12-16ms delays
    {
        const float delays[] = { 12.0f, 13.0f, 14.0f, 14.5f, 15.0f, 16.0f };
        const float pans[]   = { 0.1f,  0.35f, 0.5f,  0.5f,  0.65f, 0.9f  };
        for (int i = 0; i < kNumVoices; ++i)
        {
            voices[static_cast<size_t>(i)].baseDelaySamples = delays[i] * 0.001f * sr;
            voices[static_cast<size_t>(i)].pan = pans[i];
            voices[static_cast<size_t>(i)].panTarget = pans[i];
            voices[static_cast<size_t>(i)].fmLfoRate = 1.5f;
            voices[static_cast<size_t>(i)].fmLfoRateBase = 1.5f;
            voices[static_cast<size_t>(i)].fmLfoRateTarget = 1.5f;
            voices[static_cast<size_t>(i)].fmDepthSamples = 0.2f * 0.001f * sr;
        }
    }
    else if (mode == 4) // Flanger: very short delays (0.5-5ms), 4 voices, high-LFO
    {
        const float delays[] = { 0.5f, 1.5f, 2.5f, 4.0f, 3.0f, 5.0f };
        const float pans[]   = { 0.0f, 1.0f, 0.3f, 0.7f, 0.4f, 0.6f };
        for (int i = 0; i < kNumVoices; ++i)
        {
            voices[static_cast<size_t>(i)].baseDelaySamples = delays[i] * 0.001f * sr;
            voices[static_cast<size_t>(i)].pan = pans[i];
            voices[static_cast<size_t>(i)].panTarget = pans[i];
            voices[static_cast<size_t>(i)].fmLfoRate = 0.3f + static_cast<float>(i) * 0.08f;
            voices[static_cast<size_t>(i)].fmLfoRateBase = voices[static_cast<size_t>(i)].fmLfoRate;
            voices[static_cast<size_t>(i)].fmLfoRateTarget = voices[static_cast<size_t>(i)].fmLfoRate;
            voices[static_cast<size_t>(i)].fmDepthSamples = 0.4f * 0.001f * sr; // deep sweep
        }
    }
}

void ChorusEngine::setDrift(float amount)
{
    // Maps 0-1 to 0%-10% LFO rate drift range
    driftAmount = amount * 0.10f;
}

void ChorusEngine::setTempoSync(bool enabled, double bpm)
{
    tempoSyncEnabled = enabled;
    if (enabled)
        tempoSyncedRate = static_cast<float>(bpm) / 240.0f; // 1 cycle per bar at 4/4
}

void ChorusEngine::setPans(float outer, float inner)
{
    voices[0].panTarget = 0.5f - outer * 0.5f;
    voices[1].panTarget = 0.5f + outer * 0.5f;
    voices[2].panTarget = 0.5f - inner * 0.5f;
    voices[3].panTarget = 0.5f + inner * 0.5f;
}

float ChorusEngine::readInterpolated(int channel, float delaySamples) const
{
    const float readPos = static_cast<float>(writePos) - delaySamples;
    const int i1 = (static_cast<int>(std::floor(readPos)) + kDelayBufferSize) & (kDelayBufferSize - 1);
    const int i0 = (i1 - 1 + kDelayBufferSize) & (kDelayBufferSize - 1);
    const int i2 = (i1 + 1) & (kDelayBufferSize - 1);
    const int i3 = (i1 + 2) & (kDelayBufferSize - 1);
    const float t = readPos - std::floor(readPos);

    const auto& buf = delayBuffer[static_cast<size_t>(channel)];
    const float p0 = buf[static_cast<size_t>(i0)];
    const float p1 = buf[static_cast<size_t>(i1)];
    const float p2 = buf[static_cast<size_t>(i2)];
    const float p3 = buf[static_cast<size_t>(i3)];

    const float a = 0.5f * (-p0 + 3.0f * p1 - 3.0f * p2 + p3);
    const float b = 0.5f * (2.0f * p0 - 5.0f * p1 + 4.0f * p2 - p3);
    const float c = 0.5f * (-p0 + p2);
    return ((a * t + b) * t + c) * t + p1;
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
    const auto numChannels = buffer.getNumChannels();
    const auto numSamples = buffer.getNumSamples();
    const bool isStereo = numChannels > 1;
    const float lfoInc = 1.0f / static_cast<float>(sampleRate);

    // Block-level envelope follower — compute peak once per block (PERF-2)
    {
        float blockPeak = 0.0f;
        const float* pL = buffer.getReadPointer(0);
        const float* pR = isStereo ? buffer.getReadPointer(1) : pL;
        for (int j = 0; j < numSamples; ++j)
            blockPeak = std::max(blockPeak, std::abs(pL[j] + pR[j]) * 0.5f);
        const float alpha = (blockPeak > inputEnvelope) ? envAttackAlpha : envReleaseAlpha;
        const float alphaBlock = 1.0f - std::pow(1.0f - alpha, static_cast<float>(numSamples));
        inputEnvelope += alphaBlock * (blockPeak - inputEnvelope);
    }
    const float envDepthScale = 0.5f + juce::jlimit(0.0f, 1.0f, inputEnvelope * 3.0f) * 1.5f;

    for (int i = 0; i < numSamples; ++i)
    {
        const float mix = mixSmoothed.getNextValue();
        const float speedMul = speedSmoothed.getNextValue();
        const float fb = feedbackSmoothed.getNextValue();

        // Write input + feedback to delay buffer
        for (int ch = 0; ch < numChannels; ++ch)
            delayBuffer[static_cast<size_t>(ch)][static_cast<size_t>(writePos)]
                = buffer.getReadPointer(ch)[i] + fb * feedbackState[ch];

        if (mix < 0.001f)
        {
            for (int ch = 0; ch < numChannels; ++ch) feedbackState[ch] = 0.0f;
            writePos = (writePos + 1) & (kDelayBufferSize - 1);
            continue;
        }

        float outL = 0.0f;
        float outR = 0.0f;

        for (auto& v : voices)
        {
            // Smooth pan towards target (avoids zipper noise)
            constexpr float panSmooth = 0.001f;
            v.pan += panSmooth * (v.panTarget - v.pan);

            // FM LFO: waveform shape determined by currentLfoShape
            float fmLfo;
            if (currentLfoShape == 1) // Triangle
            {
                const float norm = v.fmLfoPhase / juce::MathConstants<float>::twoPi;
                fmLfo = 1.0f - 2.0f * std::abs(2.0f * norm - std::floor(2.0f * norm + 0.5f) * 1.0f);
            }
            else if (currentLfoShape == 2) // Random S&H
            {
                if (v.fmLfoPhase < v.shPrevPhase)
                    v.shHoldValue = juce::Random::getSystemRandom().nextFloat() * 2.0f - 1.0f;
                v.shPrevPhase = v.fmLfoPhase;
                fmLfo = v.shHoldValue;
            }
            else // Sine — LUT + lerp (PERF-3)
                fmLfo = lfoSin(v.fmLfoPhase);

            // Envelope-scaled FM and AM depth (louder input → wider modulation)
            const float effectiveFmDepth = v.fmDepthSamples * envDepthScale;
            const float effectiveAmDepth = v.amDepth * envDepthScale;

            // BBD clock jitter: ~0.15% period-to-period variation (hardware-accurate)
            v.jitterPrng = v.jitterPrng * 1664525u + 1013904223u;
            const float jitterNoise = static_cast<float>(static_cast<int32_t>(v.jitterPrng))
                                      * (1.0f / 2147483648.0f);  // [-1, 1]
            // Absolute ±0.3 sample jitter — constant regardless of delay length (hardware-accurate)
            const float clockJitter = jitterNoise * 0.3f;
            const float delaySamples = std::max(0.0f, v.baseDelaySamples + fmLfo * effectiveFmDepth + clockJitter);

            // Read from delay line (1.5% cross-channel crosstalk — BBD channel bleed)
            const float rawL = readInterpolated(0, delaySamples);
            const float rawR = isStereo ? readInterpolated(1, delaySamples) : rawL;
            float sampleL = isStereo ? (0.985f * rawL + 0.015f * rawR) : rawL;
            float sampleR = isStereo ? (0.985f * rawR + 0.015f * rawL) : rawL;

            // BBD bucket-capacitor HF rolloff (~8kHz 1-pole LP — Roland Juno/Dimension D character)
            sampleL = sampleL * (1.0f - bbdLPCoeff) + v.bbdLP[0] * bbdLPCoeff;
            v.bbdLP[0] = sampleL;
            sampleR = sampleR * (1.0f - bbdLPCoeff) + v.bbdLP[1] * bbdLPCoeff;
            v.bbdLP[1] = sampleR;
            // BBD input stage saturation — 4× oversampled to reduce aliasing fold-back
            // Upsample by linear interpolation at 0.25/0.50/0.75/1.0 fractions, saturate, decimate by averaging
            {
                float samples[2] = { sampleL, sampleR };
                for (int ch = 0; ch < 2; ++ch)
                {
                    const float prev = v.osHistory[ch];
                    v.osHistory[ch] = samples[ch];
                    const float d = samples[ch] - prev;
                    const float up0 = prev + 0.25f * d;
                    const float up1 = prev + 0.50f * d;
                    const float up2 = prev + 0.75f * d;
                    const float up3 = samples[ch];
                    const float sat0 = std::tanh(up0 * 1.15f) * 0.87f;
                    const float sat1 = std::tanh(up1 * 1.15f) * 0.87f;
                    const float sat2 = std::tanh(up2 * 1.15f) * 0.87f;
                    const float sat3 = std::tanh(up3 * 1.15f) * 0.87f;
                    samples[ch] = (sat0 + sat1 + sat2 + sat3) * 0.25f;
                }
                sampleL = samples[0];
                sampleR = samples[1];
            }

            // Phase randomization: cascaded allpass filters (C3: stage count per vintage mode)
            for (int s = 0; s < currentAllpassStages; ++s)
            {
                auto& ap = v.allpass[static_cast<size_t>(s)];
                float outApL = -ap.coeff * sampleL + ap.bufferL;
                ap.bufferL = sampleL + ap.coeff * outApL;
                sampleL = outApL;

                float outApR = -ap.coeff * sampleR + ap.bufferR;
                ap.bufferR = sampleR + ap.coeff * outApR;
                sampleR = outApR;
            }

            // AM: volume shimmer (depth scaled by input envelope) — LUT + lerp (PERF-3)
            const float amLfo = lfoSin(v.amLfoPhase);
            const float ampMod = 1.0f - effectiveAmDepth + effectiveAmDepth * (0.5f + 0.5f * amLfo);
            sampleL *= ampMod;
            sampleR *= ampMod;

            // BBD clock-rate sideband: alias tone at f_clock = numStages/(2*T)
            // At typical chorus delays (5-15ms), the clock folds into the audio band,
            // creating the characteristic subtle tonal "shimmer" of vintage BBD chips.
            {
                static constexpr float kBBDStages = 512.0f; // Juno-60 MN3007 stage count
                static constexpr float kClockBleed = 0.0008f; // ~-62dBFS — authentic clock bleed level
                const float bbdClockHz = kBBDStages * 0.5f * static_cast<float>(sampleRate)
                                         / std::max(1.0f, delaySamples);
                const float bbdClockNorm = bbdClockHz / static_cast<float>(sampleRate);
                if (bbdClockNorm < 0.45f) // only add when alias falls within the audio band
                {
                    const float clockTone = kClockBleed * lfoSin(v.bbdClockPhase);
                    sampleL += clockTone;
                    sampleR += clockTone;
                }
                v.bbdClockPhase += juce::MathConstants<float>::twoPi * bbdClockNorm;
                if (v.bbdClockPhase >= juce::MathConstants<float>::twoPi)
                    v.bbdClockPhase -= juce::MathConstants<float>::twoPi;
            }

            // Hard stereo placement
            const float gainL = 1.0f - v.pan;
            const float gainR = v.pan;

            outL += sampleL * gainL;
            outR += sampleR * gainR;

            // Per-voice LFO rate drift: ±3% random walk, new target every ~4096 samples
            if (++v.lfoRateDriftCounter >= 4096)
            {
                v.lfoRateDriftCounter = 0;
                v.jitterPrng = v.jitterPrng * 1664525u + 1013904223u;
                const float r = static_cast<float>(static_cast<int32_t>(v.jitterPrng))
                                * (1.0f / 2147483648.0f); // [-1, 1]
                v.fmLfoRateTarget = v.fmLfoRateBase * (1.0f + r * driftAmount);
            }
            // Smooth chase toward target (~0.23s time constant at 44.1kHz)
            constexpr float kLfoRateDriftAlpha = 0.0001f;
            v.fmLfoRate += kLfoRateDriftAlpha * (v.fmLfoRateTarget - v.fmLfoRate);

            // Advance FM LFO (tempo sync overrides speed multiplier when enabled)
            const float fmRate = tempoSyncEnabled
                ? tempoSyncedRate
                : v.fmLfoRate * speedMul;
            v.fmLfoPhase += juce::MathConstants<float>::twoPi * fmRate * lfoInc;
            if (v.fmLfoPhase >= juce::MathConstants<float>::twoPi)
                v.fmLfoPhase -= juce::MathConstants<float>::twoPi;

            // Drift allpass coefficients toward targets (slow, ~2–5s time constant)
            v.driftPhase += juce::MathConstants<float>::twoPi * v.driftRate * lfoInc;
            if (v.driftPhase >= juce::MathConstants<float>::twoPi)
            {
                v.driftPhase -= juce::MathConstants<float>::twoPi;
                // At each drift oscillator wrap (~every 11–33s), pick new random targets
                v.jitterPrng = v.jitterPrng * 1664525u + 1013904223u;
                for (int s = 0; s < kAllpassStages; ++s)
                {
                    const float r = static_cast<float>(static_cast<int32_t>(
                        v.jitterPrng = v.jitterPrng * 1664525u + 1013904223u))
                        * (1.0f / 2147483648.0f);  // [-1, 1]
                    // New target: base coefficient ± 0.12 variation, clamped to [0.3, 0.8]
                    v.allpassTarget[s] = juce::jlimit(0.3f, 0.8f,
                        v.allpass[static_cast<size_t>(s)].coeff + r * 0.12f);
                }
            }

            // Per-sample interpolation: very slow (alpha = 0.0000075 ≈ 3s at 44.1kHz)
            constexpr float kDriftAlpha = 0.0000075f;
            for (int s = 0; s < kAllpassStages; ++s)
            {
                auto& ap = v.allpass[static_cast<size_t>(s)];
                ap.coeff += kDriftAlpha * (v.allpassTarget[s] - ap.coeff);
            }

            v.amLfoPhase += juce::MathConstants<float>::twoPi * v.amLfoRate * speedMul * lfoInc;
            if (v.amLfoPhase >= juce::MathConstants<float>::twoPi)
                v.amLfoPhase -= juce::MathConstants<float>::twoPi;
        }

        // EQ on wet signal: low cut 50Hz + high shelf rolloff 5kHz
        outL = processBiquad(outL, hpB0, hpB1, hpB2, hpA1, hpA2, hpState[0]);
        outL = processBiquad(outL, lpB0, lpB1, lpB2, lpA1, lpA2, lpState[0]);

        outR = processBiquad(outR, hpB0, hpB1, hpB2, hpA1, hpA2, hpState[1]);
        outR = processBiquad(outR, lpB0, lpB1, lpB2, lpA1, lpA2, lpState[1]);

        // Constant-power crossfade
        const float dryGain = std::cos(mix * juce::MathConstants<float>::halfPi);
        const float wetGain = std::sin(mix * juce::MathConstants<float>::halfPi);
        auto* dataL = buffer.getWritePointer(0);
        dataL[i] = dataL[i] * dryGain + outL * wetGain;
        feedbackState[0] = std::tanh(outL * 1.1f) * 0.91f;  // soft saturation: warmth + prevents runaway

        if (isStereo)
        {
            auto* dataR = buffer.getWritePointer(1);
            dataR[i] = dataR[i] * dryGain + outR * wetGain;
            feedbackState[1] = std::tanh(outR * 1.1f) * 0.91f;
        }
        else
        {
            feedbackState[1] = feedbackState[0];
        }

        writePos = (writePos + 1) & (kDelayBufferSize - 1);
    }
}
