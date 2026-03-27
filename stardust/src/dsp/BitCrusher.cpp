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
        nyquistState[ch] = 0.0f;
    }

    // SP-1200 bandwidth ceiling: 1-pole LP at 13kHz (Nyquist of 26.04kHz)
    constexpr float nyquistCutoff = 13000.0f;
    const float fc = nyquistCutoff / static_cast<float>(sampleRate);
    nyquistAlpha = 1.0f - std::exp(-2.0f * 3.14159265f * fc);
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
        const float levels = std::pow(2.0f, bitDepth);
        const float ratio = targetRate / static_cast<float>(hostSampleRate);

        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto* data = buffer.getWritePointer(ch);
            float sample = data[i];

            // ADC hard-clip at ±1.0 (12-bit converter rail clipping)
            sample = juce::jlimit(-1.0f, 1.0f, sample);

            // Bit depth reduction (quantize)
            sample = std::floor(sample * levels + 0.5f) / levels;

            // Sample rate reduction (sample-and-hold)
            holdCounter[ch] += ratio;
            if (holdCounter[ch] >= 1.0f)
            {
                holdCounter[ch] -= 1.0f;
                holdSample[ch] = sample;
            }

            // SP-1200 bandwidth ceiling: 13kHz low-pass
            // Simulates the Nyquist limit of 26.04kHz hardware
            nyquistState[ch] += nyquistAlpha * (holdSample[ch] - nyquistState[ch]);
            data[i] = nyquistState[ch];
        }
    }
}
