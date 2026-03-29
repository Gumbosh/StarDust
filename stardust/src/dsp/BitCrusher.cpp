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
        preAaState[ch] = 0.0f;
        nyquistState[ch] = 0.0f;
    }

    // SP-1200 bandwidth ceiling: 1-pole LP at 13kHz (Nyquist of 26.04kHz)
    constexpr float nyquistCutoff = 13000.0f;
    const float fc = nyquistCutoff / static_cast<float>(sampleRate);
    nyquistAlpha = 1.0f - std::exp(-juce::MathConstants<float>::twoPi * fc);
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
    const auto numChannels = std::min(buffer.getNumChannels(), kMaxChannels);
    const auto numSamples = buffer.getNumSamples();

    for (int i = 0; i < numSamples; ++i)
    {
        const float bitDepth = bitDepthSmoothed.getNextValue();
        const float targetRate = targetRateSmoothed.getNextValue();
        const float levels = std::exp2f(bitDepth);
        const float ratio = targetRate / static_cast<float>(hostSampleRate);

        // Pre-S&H anti-alias coefficient — computed once per sample, same for all channels
        const float preAaCutoff = targetRate * 0.5f / static_cast<float>(hostSampleRate);
        const float preAaAlpha = 1.0f - std::exp(-juce::MathConstants<float>::twoPi * preAaCutoff);

        // Clock jitter: perturb ratio once per sample (shared clock — same offset for all channels)
        float jitteredRatio = ratio;
        if (jitterAmount > 0.0f)
        {
            jitterState = jitterState * 1664525u + 1013904223u;
            const float noise = static_cast<float>(static_cast<int32_t>(jitterState)) * (1.0f / 2147483648.0f);
            jitteredRatio = ratio * (1.0f + noise * jitterAmount * 0.4f);
        }

        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto* data = buffer.getWritePointer(ch);
            float sample = data[i];

            // ADC hard-clip at ±1.0 (12-bit converter rail clipping)
            sample = juce::jlimit(-1.0f, 1.0f, sample);

            // Pre-S&H anti-alias (1-pole LP at targetRate/2)
            preAaState[ch] += preAaAlpha * (sample - preAaState[ch]);
            sample = preAaState[ch];

            // Sample rate reduction (sample-and-hold) — BEFORE quantization
            holdCounter[ch] += jitteredRatio;
            if (holdCounter[ch] >= 1.0f)
            {
                holdCounter[ch] -= 1.0f;
                holdSample[ch] = sample;
            }

            // Bit depth reduction (quantize the held sample)
            float quantized = std::floor(holdSample[ch] * levels + 0.5f) / levels;

            // SP-1200 bandwidth ceiling: 13kHz low-pass reconstruction
            nyquistState[ch] += nyquistAlpha * (quantized - nyquistState[ch]);
            data[i] = nyquistState[ch];
        }
    }
}
