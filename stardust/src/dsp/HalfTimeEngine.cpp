#include "HalfTimeEngine.h"

void HalfTimeEngine::prepare(double sr, int /*samplesPerBlock*/)
{
    sampleRate = sr;
    fadeSmoothed.reset(sr, 0.02);
    mixSmoothed.reset(sr, 0.02);

    capturePos = 0;
    playbackPos = 0.0;
    aIsCapturing = true;
    currentBPM = 120.0;
    chunkSamples = computeChunkSamples();
    latchedChunkSamples = chunkSamples;

    for (int ch = 0; ch < kMaxChannels; ++ch)
    {
        bufferA[ch].fill(0.0f);
        bufferB[ch].fill(0.0f);
    }
}

void HalfTimeEngine::setDivision(int idx)  { divisionIndex.store(juce::jlimit(0, kNumDivisions - 1, idx), std::memory_order_relaxed); }
void HalfTimeEngine::setSpeed(int idx)     { speedIndex.store(juce::jlimit(0, 1, idx), std::memory_order_relaxed); }
void HalfTimeEngine::setFade(float f)      { fadeSmoothed.setTargetValue(juce::jlimit(0.0f, 1.0f, f)); }
void HalfTimeEngine::setMix(float m)       { mixSmoothed.setTargetValue(juce::jlimit(0.0f, 1.0f, m)); }
void HalfTimeEngine::setBPM(double bpm)    { currentBPM = juce::jlimit(20.0, 300.0, bpm); }

int HalfTimeEngine::computeChunkSamples() const
{
    const int div = divisionIndex.load(std::memory_order_relaxed);
    const double divSeconds = static_cast<double>(kDivisionBeats[div]) * 60.0 / currentBPM;
    return juce::jlimit(64, kBufferSize - 1, static_cast<int>(divSeconds * sampleRate));
}

float HalfTimeEngine::readHermite(const std::array<float, kBufferSize>& buf, double pos) const
{
    const int idx1 = static_cast<int>(std::floor(pos)) & kBufferMask;
    const int idx0 = (idx1 - 1 + kBufferSize) & kBufferMask;
    const int idx2 = (idx1 + 1) & kBufferMask;
    const int idx3 = (idx1 + 2) & kBufferMask;
    const float frac = static_cast<float>(pos - std::floor(pos));
    const float y0 = buf[static_cast<size_t>(idx0)];
    const float y1 = buf[static_cast<size_t>(idx1)];
    const float y2 = buf[static_cast<size_t>(idx2)];
    const float y3 = buf[static_cast<size_t>(idx3)];
    const float c1 = 0.5f * (y2 - y0);
    const float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
    const float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
    return ((c3 * frac + c2) * frac + c1) * frac + y1;
}

void HalfTimeEngine::process(juce::AudioBuffer<float>& buffer)
{
    juce::ScopedNoDenormals noDenormals;
    const auto numChannels = std::min(buffer.getNumChannels(), kMaxChannels);
    const auto numSamples = buffer.getNumSamples();

    chunkSamples = computeChunkSamples();

    const float speed = kSpeedMultipliers[speedIndex.load(std::memory_order_relaxed)];

    for (int i = 0; i < numSamples; ++i)
    {
        const float fade = fadeSmoothed.getNextValue();
        const float mix = mixSmoothed.getNextValue();

        auto& capBuf = aIsCapturing ? bufferA : bufferB;
        auto& playBuf = aIsCapturing ? bufferB : bufferA;

        // Write input to capture buffer (bitmask-safe)
        for (int ch = 0; ch < numChannels; ++ch)
            capBuf[static_cast<size_t>(ch)][static_cast<size_t>(capturePos & kBufferMask)] = buffer.getSample(ch, i);

        // Read from playback buffer at fractional position using Hermite interpolation
        const double clampedPos = juce::jlimit(0.0, static_cast<double>(latchedChunkSamples - 1), playbackPos);
        float halfL = readHermite(playBuf[0], clampedPos);
        float halfR = numChannels > 1 ? readHermite(playBuf[1], clampedPos) : halfL;

        // Crossfade at boundaries (use capturePos for fade envelope)
        const int fadeSamples = std::max(1, static_cast<int>(fade * 0.5f * static_cast<float>(latchedChunkSamples)));
        float fadeGain = 1.0f;
        if (capturePos < fadeSamples)
            fadeGain = static_cast<float>(capturePos) / static_cast<float>(fadeSamples);
        else if (capturePos > latchedChunkSamples - fadeSamples - 1)
            fadeGain = static_cast<float>(latchedChunkSamples - 1 - capturePos) / static_cast<float>(fadeSamples);
        fadeGain = juce::jlimit(0.0f, 1.0f, fadeGain);

        halfL *= fadeGain;
        halfR *= fadeGain;

        // Advance positions
        ++capturePos;
        playbackPos += static_cast<double>(speed);

        // Swap buffers at chunk boundary -- latch new chunk length here
        if (capturePos >= latchedChunkSamples)
        {
            latchedChunkSamples = chunkSamples;
            capturePos = 0;
            playbackPos = 0.0;
            aIsCapturing = !aIsCapturing;
        }

        // Constant-power dry/wet mix
        const float dryGain = std::cos(mix * juce::MathConstants<float>::halfPi);
        const float wetGain = std::sin(mix * juce::MathConstants<float>::halfPi);

        const float dryL = buffer.getSample(0, i);
        const float dryR = numChannels > 1 ? buffer.getSample(1, i) : dryL;

        buffer.setSample(0, i, dryL * dryGain + halfL * wetGain);
        if (numChannels > 1)
            buffer.setSample(1, i, dryR * dryGain + halfR * wetGain);
    }
}
