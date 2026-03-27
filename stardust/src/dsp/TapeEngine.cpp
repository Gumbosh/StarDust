#include "TapeEngine.h"

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
        hissHpState[ch] = 0.0f;
        hissLpState[ch] = 0.0f;
        tapeFilterState[ch] = 0.0f;
        headBumpState[ch] = 0.0f;
        wowNoise[ch] = 0.0f;
        wowNoiseState[ch] = 0.0f;
        flutterNoise[ch] = 0.0f;
        flutterNoiseState[ch] = 0.0f;
    }
    writePos = 0;
    wowPhase = 0.0f;
    flutterPhase = 0.0f;

    const float sr = static_cast<float>(sampleRate);
    const float twoPi = juce::MathConstants<float>::twoPi;

    // Cassette bandwidth ceiling: 1-pole LP at ~11kHz
    tapeFilterAlpha = 1.0f - std::exp(-twoPi * 11000.0f / sr);

    // Head bump: gentle LP for extracting ~100Hz region
    headBumpAlpha = 1.0f - std::exp(-twoPi * 100.0f / sr);
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

float TapeEngine::readInterpolated(int channel, float delaySamples) const
{
    const float readPos = static_cast<float>(writePos) - delaySamples;
    const int pos0 = (static_cast<int>(std::floor(readPos)) % kDelayBufferSize + kDelayBufferSize) % kDelayBufferSize;
    const int pos1 = (pos0 + 1) % kDelayBufferSize;
    const float frac = readPos - std::floor(readPos);
    return delayBuffer[channel][pos0] * (1.0f - frac) + delayBuffer[channel][pos1] * frac;
}

void TapeEngine::process(juce::AudioBuffer<float>& buffer)
{
    juce::ScopedNoDenormals noDenormals;

    // Bypass when all parameters are at zero — avoid delay line latency and coloration
    if (currentWow < 0.001f && currentFlutter < 0.001f && currentHiss < 0.001f
        && !wowSmoothed.isSmoothing() && !flutterSmoothed.isSmoothing() && !hissSmoothed.isSmoothing())
        return;

    const auto numChannels = buffer.getNumChannels();
    const auto numSamples = buffer.getNumSamples();
    const float sr = static_cast<float>(sampleRate);
    const float twoPi = juce::MathConstants<float>::twoPi;

    // Hiss filter coefficients: bandpass 2kHz–6kHz (cassette emphasis range)
    const float hpAlpha = 1.0f - std::exp(-twoPi * 2000.0f / sr);
    const float lpAlpha = 1.0f - std::exp(-twoPi * 6000.0f / sr);

    // Noise LP filter coefficient for wow/flutter randomization (~2Hz for wow, ~20Hz for flutter)
    const float wowNoiseAlpha = 1.0f - std::exp(-twoPi * 2.0f / sr);
    const float flutterNoiseAlpha = 1.0f - std::exp(-twoPi * 20.0f / sr);

    for (int i = 0; i < numSamples; ++i)
    {
        const float wow = wowSmoothed.getNextValue();
        const float flutter = flutterSmoothed.getNextValue();
        const float hiss = hissSmoothed.getNextValue();

        // Wow: sine (~0.4Hz) — shared across channels
        const float wowSine = std::sin(wowPhase) * 0.6f;

        // Flutter: sine (~7Hz) — shared across channels
        const float flutterSine = std::sin(flutterPhase) * 0.5f;

        const float baseDelay = 40.0f;

        for (int ch = 0; ch < numChannels && ch < kMaxChannels; ++ch)
        {
            // Per-channel noise for stereo decorrelation
            const float wowRand = (random.nextFloat() * 2.0f - 1.0f);
            wowNoiseState[ch] += wowNoiseAlpha * (wowRand - wowNoiseState[ch]);
            const float wowMod = wowSine + wowNoiseState[ch] * 0.4f;
            const float wowDelay = wow * 18.0f * wowMod;

            const float flutterRand = (random.nextFloat() * 2.0f - 1.0f);
            flutterNoiseState[ch] += flutterNoiseAlpha * (flutterRand - flutterNoiseState[ch]);
            const float flutterMod = flutterSine + flutterNoiseState[ch] * 0.5f;
            const float flutterDelay = flutter * 4.0f * flutterMod;

            const float totalDelay = std::max(1.0f, baseDelay + wowDelay + flutterDelay);

            auto* data = buffer.getWritePointer(ch);

            // Write to delay buffer
            delayBuffer[ch][writePos] = data[i];

            // Read pitch-modulated signal
            float wet = readInterpolated(ch, totalDelay);

            // Tape saturation: soft-knee compression at ~0.8 threshold
            // Simulates magnetic tape saturation — warm, gentle, rounds transients
            {
                const float absWet = std::abs(wet);
                if (absWet > 0.4f)
                {
                    const float overshoot = (absWet - 0.4f) / 0.6f; // 0 at 0.4, 1 at 1.0
                    const float reduction = 1.0f - overshoot * 0.3f; // up to 30% gain reduction
                    wet *= reduction;
                }
            }

            // Head bump: subtle low-frequency boost (~100Hz, +2dB)
            headBumpState[ch] += headBumpAlpha * (wet - headBumpState[ch]);
            wet += headBumpState[ch] * 0.15f; // ~2dB boost at 100Hz

            // Cassette bandwidth ceiling: LP at ~11kHz
            tapeFilterState[ch] += tapeFilterAlpha * (wet - tapeFilterState[ch]);
            wet = tapeFilterState[ch];

            // Bandpass-filtered hiss (2kHz–6kHz, cassette character)
            if (hiss > 0.001f)
            {
                float noise = (random.nextFloat() * 2.0f - 1.0f) * hiss * 0.018f;
                hissHpState[ch] += hpAlpha * (noise - hissHpState[ch]);
                noise -= hissHpState[ch];
                hissLpState[ch] += lpAlpha * (noise - hissLpState[ch]);
                wet += hissLpState[ch];
            }

            data[i] = wet;
        }

        writePos = (writePos + 1) % kDelayBufferSize;

        // Advance LFOs
        wowPhase += twoPi * 0.4f / sr;    // ~0.4 Hz (capstan speed variation)
        if (wowPhase >= twoPi) wowPhase -= twoPi;
        flutterPhase += twoPi * 7.0f / sr; // ~7 Hz (motor/belt)
        if (flutterPhase >= twoPi) flutterPhase -= twoPi;
    }
}
