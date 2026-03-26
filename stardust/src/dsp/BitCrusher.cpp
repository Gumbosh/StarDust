#include "BitCrusher.h"

void BitCrusher::prepare(double sampleRate, int /*samplesPerBlock*/)
{
    hostSampleRate = sampleRate;
    bitDepthSmoothed.reset(sampleRate, 0.005);  // 5ms — fast, quantization doesn't click
    targetRateSmoothed.reset(sampleRate, 0.005); // 5ms — AA filter handles the smoothing

    for (int ch = 0; ch < kMaxChannels; ++ch)
    {
        holdCounter[ch] = 0.0f;
        holdSample[ch] = 0.0f;
        aaState[ch] = 0.0f;
    }
    aaAlpha = 1.0f;
    lastTargetRate = 0.0f;
}

void BitCrusher::setBitDepth(float bits)
{
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
    const auto numChannels = buffer.getNumChannels();
    const auto numSamples = buffer.getNumSamples();

    for (int i = 0; i < numSamples; ++i)
    {
        const float bitDepth = bitDepthSmoothed.getNextValue();
        const float targetRate = targetRateSmoothed.getNextValue();

        // Update anti-aliasing filter coefficient when target rate changes
        if (std::abs(targetRate - lastTargetRate) > 1.0f)
        {
            lastTargetRate = targetRate;
            // Low-pass at half the target rate (Nyquist of the reduced rate)
            const float cutoff = targetRate * 0.5f;
            const float fc = cutoff / static_cast<float>(hostSampleRate);
            aaAlpha = 1.0f - std::exp(-2.0f * 3.14159265f * fc);
        }

        const float levels = std::pow(2.0f, bitDepth);
        const float ratio = targetRate / static_cast<float>(hostSampleRate);

        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto* data = buffer.getWritePointer(ch);

            // No anti-aliasing filter — SP-1200 deliberately omitted it,
            // producing the bright aliasing artifacts that define the sound
            float sample = data[i];

            // ADC hard-clip at ±1.0 (12-bit converter rail clipping)
            sample = juce::jlimit(-1.0f, 1.0f, sample);

            // Bit depth reduction (quantize)
            sample = std::floor(sample * levels + 0.5f) / levels;

            // Step 3: Sample rate reduction (sample-and-hold)
            holdCounter[ch] += ratio;
            if (holdCounter[ch] >= 1.0f)
            {
                holdCounter[ch] -= 1.0f;
                holdSample[ch] = sample;
            }

            data[i] = holdSample[ch];
        }
    }
}
