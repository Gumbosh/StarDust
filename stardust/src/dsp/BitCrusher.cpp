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
        postZ1[ch] = 0.0f;
        postZ2[ch] = 0.0f;
        jitterState[ch] = 0xDEAD0001u + static_cast<uint32_t>(ch);
    }
    postB0 = 1.0f; postB1 = 0.0f; postB2 = 0.0f; postA1 = 0.0f; postA2 = 0.0f;
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

    // Post-crush LP coefficients (2nd-order Butterworth) — block rate
    {
        const float targetRate = targetRateSmoothed.getCurrentValue();
        const float normCut = juce::jlimit(0.006f, 0.49f, targetRate * 0.45f / sr);
        const float w0 = juce::MathConstants<float>::twoPi * normCut;
        const float cosW0 = std::cos(w0);
        const float sinW0 = std::sin(w0);
        const float alpha = sinW0 / (2.0f * 0.7071f); // Q = 1/sqrt(2) Butterworth
        const float a0inv = 1.0f / (1.0f + alpha);
        postB0 = ((1.0f - cosW0) * 0.5f) * a0inv;
        postB1 = (1.0f - cosW0) * a0inv;
        postB2 = postB0;
        postA1 = (-2.0f * cosW0) * a0inv;
        postA2 = (1.0f - alpha) * a0inv;
    }

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
            // Jitter: random offset on S&H clock timing (like cheap ADC clock instability)
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
            // floor(x * levels) / levels — like vintage samplers
            const float crushed = (levels <= 2.0f)
                ? (holdSample[ch] >= 0.0f ? 0.5f : -0.5f)
                : std::floor(holdSample[ch] * levels) / levels;

            // --- Post-crush LP filter: tames harshest aliasing ---
            const float out = postB0 * crushed + postZ1[ch];
            postZ1[ch] = postB1 * crushed - postA1 * out + postZ2[ch];
            postZ2[ch] = postB2 * crushed - postA2 * out;

            data[i] = out;
        }
    }
}
