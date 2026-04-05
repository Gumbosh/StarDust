#include "BitCrusher.h"

void BitCrusher::prepare(double sampleRate, int /*samplesPerBlock*/)
{
    hostSampleRate = sampleRate;
    bitDepthSmoothed.reset(sampleRate, 0.005);
    targetRateSmoothed.reset(sampleRate, 0.005);

    for (int ch = 0; ch < kMaxChannels; ++ch)
    {
        holdCounter[ch] = 0.0f;
        holdSample[ch] = 0.0f;
        jitterState[ch] = 0xDEAD0001u + static_cast<uint32_t>(ch);
    }
}

void BitCrusher::setBitDepth(float bits)
{
    bits = juce::jlimit(1.0f, 24.0f, bits);
    if (currentBitDepth != bits)
    {
        currentBitDepth = bits;
        bitDepthSmoothed.setTargetValue(bits);
    }
}

void BitCrusher::setSampleRate(float targetRate)
{
    if (currentTargetRate != targetRate)
    {
        currentTargetRate = targetRate;
        targetRateSmoothed.setTargetValue(targetRate);
    }
}

void BitCrusher::process(juce::AudioBuffer<float>& buffer)
{
    juce::ScopedNoDenormals noDenormals;
    const auto numChannels = std::min(buffer.getNumChannels(), kMaxChannels);
    const auto numSamples = buffer.getNumSamples();
    const float sr = static_cast<float>(hostSampleRate);

    // Bit depth: cache exp2 (expensive) — update every 16 samples
    float cachedLevels = std::exp2f(bitDepthSmoothed.getCurrentValue());
    int bitUpdateCounter = 0;

    for (int i = 0; i < numSamples; ++i)
    {
        const float targetRate = targetRateSmoothed.getNextValue();
        const float currentBits = bitDepthSmoothed.getNextValue();
        if (++bitUpdateCounter >= 16)
        {
            bitUpdateCounter = 0;
            cachedLevels = std::exp2f(currentBits);
        }
        const float levels = cachedLevels;
        const float ratio = targetRate / sr;

        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto* data = buffer.getWritePointer(ch);
            const float sample = data[i];

            // --- Sample-and-hold with clock jitter ---
            float jitteredRatio = ratio;
            if (jitterAmount > 0.0f)
            {
                jitterState[ch] = jitterState[ch] * 1664525u + 1013904223u;
                const float noise = static_cast<float>(static_cast<int32_t>(jitterState[ch]))
                                    * (1.0f / 2147483648.0f);
                jitteredRatio = ratio * (1.0f + noise * jitterAmount * 0.5f);
            }

            holdCounter[ch] += jitteredRatio;
            if (holdCounter[ch] >= 1.0f)
            {
                holdCounter[ch] -= 1.0f;
                holdSample[ch] = sample; // ZOH: grab and hold
            }

            // --- Bit depth reduction: raw truncation, no dither ---
            data[i] = (levels <= 2.0f)
                ? (holdSample[ch] >= 0.0f ? 0.5f : -0.5f)
                : std::floor(holdSample[ch] * levels) / levels;
        }
    }
}
