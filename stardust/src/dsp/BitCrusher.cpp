#include "BitCrusher.h"

void BitCrusher::prepare(double sampleRate, int /*samplesPerBlock*/)
{
    hostSampleRate = sampleRate;
    const double rampTimeSec = 0.02;
    bitDepthSmoothed.reset(sampleRate, rampTimeSec);
    targetRateSmoothed.reset(sampleRate, rampTimeSec);

    for (int ch = 0; ch < kMaxChannels; ++ch)
    {
        holdCounter[ch] = 0.0f;
        holdSample[ch] = 0.0f;
    }
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

            // Step 1: Bit depth reduction (quantize)
            float sample = data[i];
            sample = std::floor(sample * levels + 0.5f) / levels;

            // Step 2: Sample rate reduction (sample-and-hold)
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
