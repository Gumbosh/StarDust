#include "TapeEngine.h"

// Ampex 456 reel-to-reel saturation (gentler than cassette — +6dB headroom)
static constexpr float kDrive = 1.5f;
static constexpr float kInvTanhDrive = 1.0f / 0.9051f; // 1/tanh(1.5)
static constexpr float kAsymmetry = 1.10f; // subtler asymmetry than cassette

// Hiss: lower level than cassette (wider ¼" track)
static constexpr float kHissGain = 0.012f;
static constexpr float kHissReductionScale = 1.2f;
static constexpr float kHissReductionMax = 0.3f;

float TapeEngine::fastNoise(uint32_t& state)
{
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    return static_cast<float>(state) / static_cast<float>(UINT32_MAX) * 2.0f - 1.0f;
}

void TapeEngine::prepare(double newSampleRate, int /*samplesPerBlock*/)
{
    sampleRate = newSampleRate;
    constexpr double rampTimeSec = 0.02;
    wowSmoothed.reset(sampleRate, rampTimeSec);
    flutterSmoothed.reset(sampleRate, rampTimeSec);
    hissSmoothed.reset(sampleRate, rampTimeSec);

    for (int ch = 0; ch < kMaxChannels; ++ch)
    {
        std::fill(std::begin(delayBuffer[ch]), std::end(delayBuffer[ch]), 0.0f);
        wowNoiseState[ch] = 0.0f;
        flutterNoiseState[ch] = 0.0f;
        headBumpX1[ch] = headBumpX2[ch] = 0.0f;
        headBumpY1[ch] = headBumpY2[ch] = 0.0f;
        notchX1[ch] = notchX2[ch] = 0.0f;
        notchY1[ch] = notchY2[ch] = 0.0f;
        hissBpX1[ch] = hissBpX2[ch] = 0.0f;
        hissBpY1[ch] = hissBpY2[ch] = 0.0f;
        preEmpY1[ch] = 0.0f;
        deEmpY1[ch] = 0.0f;
        dynLpState[ch] = 0.0f;
        dynLpEnvState[ch] = 0.0f;
    }
    writePos = 0;
    wowPhase = 0.0f;
    flutterPhase1 = 0.0f;
    flutterPhase2 = 0.0f;

    const float sr = static_cast<float>(sampleRate);
    const float twoPi = juce::MathConstants<float>::twoPi;

    // LFO increments (reel-to-reel: tighter transport)
    wowPhaseInc = twoPi * 0.5f / sr;     // 0.5 Hz wow
    flutter1PhaseInc = twoPi * 8.0f / sr; // 8 Hz primary flutter
    flutter2PhaseInc = twoPi * 15.0f / sr; // 15 Hz scrape flutter

    // Noise filter coefficients (tighter than cassette)
    wowNoiseAlpha = 1.0f - std::exp(-twoPi * 3.0f / sr);
    flutterNoiseAlpha = 1.0f - std::exp(-twoPi * 25.0f / sr);
    envAlpha = 1.0f - std::exp(-twoPi * 100.0f / sr);

    // Head bump: peaking EQ at 60Hz, +3dB, Q=0.7 (wider, warmer than cassette)
    {
        const float f0 = 60.0f, gainDb = 3.0f, Q = 0.7f;
        const float A = std::pow(10.0f, gainDb / 40.0f);
        const float w0 = twoPi * f0 / sr;
        const float alpha = std::sin(w0) / (2.0f * Q);
        const float a0 = 1.0f + alpha / A;
        hbB0 = (1.0f + alpha * A) / a0;
        hbB1 = (-2.0f * std::cos(w0)) / a0;
        hbB2 = (1.0f - alpha * A) / a0;
        hbA1 = (-2.0f * std::cos(w0)) / a0;
        hbA2 = (1.0f - alpha / A) / a0;
    }

    // Head notch at 120Hz, -1.5dB, Q=1.0
    {
        const float f0 = 120.0f, gainDb = -1.5f, Q = 1.0f;
        const float A = std::pow(10.0f, gainDb / 40.0f);
        const float w0 = twoPi * f0 / sr;
        const float alpha = std::sin(w0) / (2.0f * Q);
        const float a0 = 1.0f + alpha / A;
        nB0 = (1.0f + alpha * A) / a0;
        nB1 = (-2.0f * std::cos(w0)) / a0;
        nB2 = (1.0f - alpha * A) / a0;
        nA1 = (-2.0f * std::cos(w0)) / a0;
        nA2 = (1.0f - alpha / A) / a0;
    }

    // NAB pre/de-emphasis: 50µs time constant (~3183Hz)
    {
        const float tau = 50.0e-6f;
        const float wc = 1.0f / tau;
        const float T = 1.0f / sr;
        const float g = std::tan(wc * T / 2.0f);
        peB0 = g / (1.0f + g); // LP coefficient for shelf
        deB0 = g / (1.0f + g);
    }

    // Hiss: biquad bandpass at 6kHz, Q=0.8 (broader than cassette)
    {
        const float f0 = 6000.0f, Q = 0.8f;
        const float w0 = twoPi * f0 / sr;
        const float alpha = std::sin(w0) / (2.0f * Q);
        const float a0 = 1.0f + alpha;
        hBpB0 = alpha / a0;
        hBpB1 = 0.0f;
        hBpB2 = -alpha / a0;
        hBpA1 = (-2.0f * std::cos(w0)) / a0;
        hBpA2 = (1.0f - alpha) / a0;
    }
}

void TapeEngine::setWow(float amount)
{
    if (currentWow != amount) { currentWow = amount; wowSmoothed.setTargetValue(amount); }
}

void TapeEngine::setFlutter(float amount)
{
    if (currentFlutter != amount) { currentFlutter = amount; flutterSmoothed.setTargetValue(amount); }
}

void TapeEngine::setHiss(float amount)
{
    if (currentHiss != amount) { currentHiss = amount; hissSmoothed.setTargetValue(amount); }
}

float TapeEngine::readHermite(int channel, float delaySamples) const
{
    const float readPos = static_cast<float>(writePos) - delaySamples;
    const int idx1 = (static_cast<int>(std::floor(readPos)) % kDelayBufferSize + kDelayBufferSize) % kDelayBufferSize;
    const int idx0 = (idx1 - 1 + kDelayBufferSize) % kDelayBufferSize;
    const int idx2 = (idx1 + 1) % kDelayBufferSize;
    const int idx3 = (idx1 + 2) % kDelayBufferSize;
    const float frac = readPos - std::floor(readPos);
    const float y0 = delayBuffer[channel][idx0], y1 = delayBuffer[channel][idx1];
    const float y2 = delayBuffer[channel][idx2], y3 = delayBuffer[channel][idx3];
    const float c0 = y1;
    const float c1 = 0.5f * (y2 - y0);
    const float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
    const float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
    return ((c3 * frac + c2) * frac + c1) * frac + c0;
}

void TapeEngine::process(juce::AudioBuffer<float>& buffer)
{
    juce::ScopedNoDenormals noDenormals;

    const auto numChannels = buffer.getNumChannels();
    const auto numSamples = buffer.getNumSamples();
    const float sr = static_cast<float>(sampleRate);
    const float twoPi = juce::MathConstants<float>::twoPi;

    const int maxSafeSamples = juce::jmin(numSamples, kDelayBufferSize / 2);

    constexpr int kLpUpdateInterval = 16;
    float dynLpAlpha[kMaxChannels] = {};
    for (int ch = 0; ch < numChannels && ch < kMaxChannels; ++ch)
    {
        const float cutoffHz = 18000.0f - std::min(1.0f, dynLpEnvState[ch] * 2.0f) * 8000.0f;
        dynLpAlpha[ch] = 1.0f - std::exp(-twoPi * cutoffHz / sr);
    }

    for (int i = 0; i < maxSafeSamples; ++i)
    {
        const float wowRaw = wowSmoothed.getNextValue();
        const float flutterRaw = flutterSmoothed.getNextValue();
        const float hiss = hissSmoothed.getNextValue();

        // Sqrt curves for responsive low-knob values
        const float wow = std::sqrt(wowRaw);
        const float flutter = std::sqrt(flutterRaw);

        // Wow: 0.5Hz sine + filtered noise
        const float wowSine = std::sin(wowPhase) * 0.6f;

        // Flutter: 2 sources (8Hz primary + 15Hz scrape) — no tertiary on pro transport
        const float f1 = std::sin(flutterPhase1) * 0.6f;
        const float f2 = std::sin(flutterPhase2) * 0.25f;

        for (int ch = 0; ch < numChannels && ch < kMaxChannels; ++ch)
        {
            const float wowRand = fastNoise(noiseState[ch]);
            wowNoiseState[ch] += wowNoiseAlpha * (wowRand - wowNoiseState[ch]);
            const float wowMod = wowSine + wowNoiseState[ch] * 0.4f;
            const float wowDelay = wow * kWowDepth * wowMod;

            const float flutterRand = fastNoise(noiseState[ch]);
            flutterNoiseState[ch] += flutterNoiseAlpha * (flutterRand - flutterNoiseState[ch]);
            const float flutterMod = f1 + f2 + flutterNoiseState[ch] * 0.3f;
            const float flutterDelay = flutter * kFlutterDepth * flutterMod;

            const float totalDelay = std::max(1.0f, kBaseDelay + wowDelay + flutterDelay);

            auto* data = buffer.getWritePointer(ch);
            delayBuffer[ch][writePos] = data[i];

            float wet = readHermite(ch, totalDelay);

            // NAB pre-emphasis: +4dB shelf above ~3.2kHz (record EQ)
            {
                const float lp = preEmpY1[ch] + peB0 * (wet - preEmpY1[ch]);
                preEmpY1[ch] = lp;
                const float hp = wet - lp;
                wet = wet + hp * 1.2f; // +4dB shelf (gentler than cassette's +6dB)
            }

            // Ampex 456 saturation: gentle asymmetric tanh
            {
                const float driven = kDrive * wet;
                const float pos = std::tanh(driven);
                const float neg = std::tanh(driven * kAsymmetry);
                wet = (driven >= 0.0f ? pos : neg) * kInvTanhDrive;
            }

            // NAB de-emphasis: -3dB shelf above ~3.2kHz (playback EQ)
            {
                const float lp = deEmpY1[ch] + deB0 * (wet - deEmpY1[ch]);
                deEmpY1[ch] = lp;
                const float hp = wet - lp;
                wet = wet - hp * 0.45f;
            }

            // Head bump: peaking EQ at 60Hz (+3dB)
            {
                const float x = wet;
                const float y = hbB0 * x + hbB1 * headBumpX1[ch] + hbB2 * headBumpX2[ch]
                               - hbA1 * headBumpY1[ch] - hbA2 * headBumpY2[ch];
                headBumpX2[ch] = headBumpX1[ch]; headBumpX1[ch] = x;
                headBumpY2[ch] = headBumpY1[ch]; headBumpY1[ch] = y;
                wet = y;
            }

            // Head notch at 120Hz (-1.5dB)
            {
                const float x = wet;
                const float y = nB0 * x + nB1 * notchX1[ch] + nB2 * notchX2[ch]
                               - nA1 * notchY1[ch] - nA2 * notchY2[ch];
                notchX2[ch] = notchX1[ch]; notchX1[ch] = x;
                notchY2[ch] = notchY1[ch]; notchY1[ch] = y;
                wet = y;
            }

            // Level-dependent LP: 18kHz quiet → 10kHz loud
            {
                const float absWet = std::abs(wet);
                dynLpEnvState[ch] += envAlpha * (absWet - dynLpEnvState[ch]);

                if ((i & (kLpUpdateInterval - 1)) == 0)
                {
                    const float envLevel = std::min(1.0f, dynLpEnvState[ch] * 2.0f);
                    const float cutoffHz = 18000.0f - envLevel * 8000.0f;
                    dynLpAlpha[ch] = 1.0f - std::exp(-twoPi * cutoffHz / sr);
                }
                dynLpState[ch] += dynLpAlpha[ch] * (wet - dynLpState[ch]);
                wet = dynLpState[ch];
            }

            // Broadband hiss (6kHz center, Q=0.8, level-dependent)
            if (hiss > 0.001f)
            {
                const float hissReduction = 1.0f - std::min(1.0f, dynLpEnvState[ch] * kHissReductionScale) * kHissReductionMax;
                const float noise = fastNoise(noiseState[ch]) * hiss * kHissGain * hissReduction;

                const float y = hBpB0 * noise + hBpB1 * hissBpX1[ch] + hBpB2 * hissBpX2[ch]
                               - hBpA1 * hissBpY1[ch] - hBpA2 * hissBpY2[ch];
                hissBpX2[ch] = hissBpX1[ch]; hissBpX1[ch] = noise;
                hissBpY2[ch] = hissBpY1[ch]; hissBpY1[ch] = y;
                wet += y;
            }

            data[i] = wet;
        }

        writePos = (writePos + 1) % kDelayBufferSize;

        // Advance LFOs
        wowPhase += wowPhaseInc;
        if (wowPhase >= twoPi) wowPhase -= twoPi;
        flutterPhase1 += flutter1PhaseInc;
        if (flutterPhase1 >= twoPi) flutterPhase1 -= twoPi;
        flutterPhase2 += flutter2PhaseInc;
        if (flutterPhase2 >= twoPi) flutterPhase2 -= twoPi;
    }
}
