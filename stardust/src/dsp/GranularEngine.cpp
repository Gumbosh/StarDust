#include "GranularEngine.h"

void GranularEngine::prepare(double newSampleRate, int /*samplesPerBlock*/)
{
    sampleRate = newSampleRate;
    const double rampTimeSec = 0.02;

    grainSizeSmoothed.reset(sampleRate, rampTimeSec);
    grainDensitySmoothed.reset(sampleRate, rampTimeSec);
    grainScatterSmoothed.reset(sampleRate, rampTimeSec);
    grainMixSmoothed.reset(sampleRate, rampTimeSec);
    grainStereoWidthSmoothed.reset(sampleRate, rampTimeSec);
    basePitchRateSmoothed.reset(sampleRate, rampTimeSec);

    for (int ch = 0; ch < kMaxChannels; ++ch)
        inputBuffer[ch].fill(0.0f);

    writePos = 0;
    samplesSinceLastGrain = 0;
    nextGrainInterval = 0;
    warmupSamples = 512;

    for (auto& g : grains)
        g.active = false;
}

void GranularEngine::setParameters(float grainSize, float grainDensity,
                                    float grainScatter, float grainMix,
                                    float grainStereoWidth)
{
    if (currentGrainSize != grainSize) { currentGrainSize = grainSize; grainSizeSmoothed.setTargetValue(grainSize); }
    if (currentDensity != grainDensity) { currentDensity = grainDensity; grainDensitySmoothed.setTargetValue(grainDensity); }
    if (currentScatter != grainScatter) { currentScatter = grainScatter; grainScatterSmoothed.setTargetValue(grainScatter); }
    if (currentMix != grainMix) { currentMix = grainMix; grainMixSmoothed.setTargetValue(grainMix); }
    if (currentStereoWidth != grainStereoWidth) { currentStereoWidth = grainStereoWidth; grainStereoWidthSmoothed.setTargetValue(grainStereoWidth); }
}

void GranularEngine::setBasePitch(float semitones)
{
    if (currentBasePitch != semitones)
    {
        currentBasePitch = semitones;
        basePitchRateSmoothed.setTargetValue(std::pow(2.0f, semitones / 12.0f));
    }
}

float GranularEngine::readFromBuffer(int channel, float position) const
{
    // Ensure positive modulus even if position is negative
    int pos0 = static_cast<int>(std::floor(position)) % kInputBufferSize;
    if (pos0 < 0) pos0 += kInputBufferSize;
    const int pos1 = (pos0 + 1) % kInputBufferSize;
    const float frac = position - std::floor(position);
    return inputBuffer[static_cast<size_t>(channel)][static_cast<size_t>(pos0)] * (1.0f - frac)
         + inputBuffer[static_cast<size_t>(channel)][static_cast<size_t>(pos1)] * frac;
}

void GranularEngine::process(juce::AudioBuffer<float>& buffer)
{
    juce::ScopedNoDenormals noDenormals;
    const auto numChannels = buffer.getNumChannels();
    const auto numSamples = buffer.getNumSamples();
    const bool isStereo = numChannels > 1;

    for (int i = 0; i < numSamples; ++i)
    {
        const float grainSizeMs = grainSizeSmoothed.getNextValue();
        const float density = grainDensitySmoothed.getNextValue();
        const float scatter = grainScatterSmoothed.getNextValue();
        const float mix = grainMixSmoothed.getNextValue();
        const float stereoWidth = grainStereoWidthSmoothed.getNextValue();
        const float basePitchRate = basePitchRateSmoothed.getNextValue();

        const int grainSizeSamples = static_cast<int>(grainSizeMs * 0.001f * static_cast<float>(sampleRate));

        // Write input samples into the circular buffer
        for (int ch = 0; ch < numChannels; ++ch)
            inputBuffer[static_cast<size_t>(ch)][static_cast<size_t>(writePos)] = buffer.getReadPointer(ch)[i];

        // Warmup: let buffer fill before spawning grains
        if (warmupSamples > 0)
        {
            --warmupSamples;
            writePos = (writePos + 1) % kInputBufferSize;
            continue;
        }

        // Grain scheduling
        ++samplesSinceLastGrain;
        const float clampedDensity = density;

        if (samplesSinceLastGrain >= nextGrainInterval && mix > 0.001f)
        {
            samplesSinceLastGrain = 0;

            const int baseInterval = (clampedDensity > 0.5f)
                ? static_cast<int>(static_cast<float>(grainSizeSamples) / clampedDensity)
                : grainSizeSamples * 2;
            const float scatterOffset = 1.0f + (random.nextFloat() * 2.0f - 1.0f) * scatter;
            nextGrainInterval = std::max(1, static_cast<int>(static_cast<float>(baseInterval) * scatterOffset));

            // Find an inactive grain slot and spawn
            for (auto& g : grains)
            {
                if (!g.active)
                {
                    const float randomOffset = random.nextFloat()
                        * static_cast<float>(grainSizeSamples) * 2.0f;
                    float startPos = static_cast<float>(writePos) - randomOffset;
                    if (startPos < 0.0f)
                        startPos += static_cast<float>(kInputBufferSize);

                    g.startPosition = startPos;
                    g.currentPosition = startPos;
                    // Per-grain pitch randomization: ±5 cents for richer clouds
                    const float centsOffset = (random.nextFloat() * 10.0f - 5.0f) / 1200.0f;
                    g.playbackRate = basePitchRate * std::pow(2.0f, centsOffset);

                    // Stereo pan: center when width=0, random when width=1
                    g.pan = 0.5f + (random.nextFloat() - 0.5f) * stereoWidth;

                    const float lengthVariation = 1.0f + (random.nextFloat() * 0.4f - 0.2f);
                    g.lengthInSamples = std::max(1, static_cast<int>(
                        static_cast<float>(grainSizeSamples) * lengthVariation));
                    g.currentSample = 0;
                    g.active = true;
                    break;
                }
            }
        }

        // Process all active grains
        const float normalization = 1.0f / std::sqrt(std::max(1.0f, density));

        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto* data = buffer.getWritePointer(ch);
            const float drySample = data[i];
            float grainOutput = 0.0f;

            for (auto& g : grains)
            {
                if (!g.active)
                    continue;

                // Grain envelope
                const float phase = static_cast<float>(g.currentSample)
                    / static_cast<float>(g.lengthInSamples);
                float envelope;
                switch (shape)
                {
                    case GrainShape::Gaussian:
                    {
                        const float x = (phase - 0.5f) * 4.0f;
                        envelope = std::exp(-0.5f * x * x);
                        break;
                    }
                    case GrainShape::Triangle:
                        envelope = 1.0f - std::abs(2.0f * phase - 1.0f);
                        break;
                    case GrainShape::Trapezoid:
                    {
                        constexpr float fadeLen = 0.2f;
                        if (phase < fadeLen)
                            envelope = phase / fadeLen;
                        else if (phase > (1.0f - fadeLen))
                            envelope = (1.0f - phase) / fadeLen;
                        else
                            envelope = 1.0f;
                        break;
                    }
                    case GrainShape::Hanning:
                    default:
                        envelope = 0.5f * (1.0f - std::cos(
                            2.0f * juce::MathConstants<float>::pi * phase));
                        break;
                }

                float grainSample = readFromBuffer(ch, g.currentPosition);

                // Apply stereo panning (constant-power) for stereo output
                float panGain = 1.0f;
                if (isStereo)
                {
                    panGain = (ch == 0)
                        ? std::cos(g.pan * juce::MathConstants<float>::halfPi)
                        : std::sin(g.pan * juce::MathConstants<float>::halfPi);
                }

                grainOutput += grainSample * envelope * panGain;

                // Advance grain position (only on last channel)
                if (ch == numChannels - 1)
                {
                    g.currentPosition += g.playbackRate;
                    if (g.currentPosition >= static_cast<float>(kInputBufferSize))
                        g.currentPosition -= static_cast<float>(kInputBufferSize);
                    if (g.currentPosition < 0.0f)
                        g.currentPosition += static_cast<float>(kInputBufferSize);

                    g.currentSample++;
                    if (g.currentSample >= g.lengthInSamples)
                        g.active = false;
                }
            }

            grainOutput *= normalization;

            const float dryGain = std::cos(mix * juce::MathConstants<float>::halfPi);
            const float wetGain = std::sin(mix * juce::MathConstants<float>::halfPi);
            const float output = drySample * dryGain + grainOutput * wetGain;
            data[i] = output;
        }

        writePos = (writePos + 1) % kInputBufferSize;
    }
}
