#include "ReverserEngine.h"

void ReverserEngine::prepare(double sr, int /*samplesPerBlock*/)
{
    sampleRate = sr;
    crossfadeSmoothed.reset(sr, 0.02);
    mixSmoothed.reset(sr, 0.02);

    capturePos = 0;
    playbackPos = 0;
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

void ReverserEngine::setRepeatCount(int count) { repeatCount.store(juce::jlimit(1, 64, count), std::memory_order_relaxed); }
void ReverserEngine::setDivision(int idx)      { divisionIndex.store(juce::jlimit(0, kNumDivisions - 1, idx), std::memory_order_relaxed); }
void ReverserEngine::setCrossfade(float cf)    { crossfadeSmoothed.setTargetValue(juce::jlimit(0.0f, 1.0f, cf)); }
void ReverserEngine::setMix(float m)           { mixSmoothed.setTargetValue(juce::jlimit(0.0f, 1.0f, m)); }
void ReverserEngine::setBPM(double bpm)        { currentBPM = juce::jlimit(20.0, 300.0, bpm); }

int ReverserEngine::computeChunkSamples() const
{
    // Total chunk = repeatCount × single division duration
    const int div = divisionIndex.load(std::memory_order_relaxed);
    const int rep = repeatCount.load(std::memory_order_relaxed);
    const double divSeconds = static_cast<double>(kDivisionBeats[div]) * 60.0 / currentBPM;
    const double chunkSeconds = divSeconds * static_cast<double>(rep);
    return juce::jlimit(64, kBufferSize - 1, static_cast<int>(chunkSeconds * sampleRate));
}

void ReverserEngine::process(juce::AudioBuffer<float>& buffer)
{
    juce::ScopedNoDenormals noDenormals;
    const auto numChannels = std::min(buffer.getNumChannels(), kMaxChannels);
    const auto numSamples = buffer.getNumSamples();

    chunkSamples = computeChunkSamples();

    for (int i = 0; i < numSamples; ++i)
    {
        const float crossfade = crossfadeSmoothed.getNextValue();
        const float mix = mixSmoothed.getNextValue();

        auto& capBuf = aIsCapturing ? bufferA : bufferB;
        auto& playBuf = aIsCapturing ? bufferB : bufferA;

        // Write input to capture buffer (bitmask-safe)
        for (int ch = 0; ch < numChannels; ++ch)
            capBuf[static_cast<size_t>(ch)][static_cast<size_t>(capturePos & kBufferMask)] = buffer.getSample(ch, i);

        // Read reversed from playback buffer (bitmask-safe)
        const int playIdx = juce::jlimit(0, latchedChunkSamples - 1, playbackPos);
        float revL = playBuf[0][static_cast<size_t>(playIdx & kBufferMask)];
        float revR = numChannels > 1 ? playBuf[1][static_cast<size_t>(playIdx & kBufferMask)] : revL;

        // Crossfade at boundaries (use latched chunk length)
        const int crossfadeSamples = std::max(1, static_cast<int>(crossfade * 0.5f * static_cast<float>(latchedChunkSamples)));
        float fadeGain = 1.0f;
        if (playbackPos < crossfadeSamples)
            fadeGain = static_cast<float>(playbackPos) / static_cast<float>(crossfadeSamples);
        else if (playbackPos > latchedChunkSamples - crossfadeSamples - 1)
            fadeGain = static_cast<float>(latchedChunkSamples - 1 - playbackPos) / static_cast<float>(crossfadeSamples);
        fadeGain = juce::jlimit(0.0f, 1.0f, fadeGain);

        revL *= fadeGain;
        revR *= fadeGain;

        // Advance positions
        ++capturePos;
        --playbackPos;

        // Swap buffers at chunk boundary — latch new chunk length here
        if (capturePos >= latchedChunkSamples)
        {
            latchedChunkSamples = chunkSamples;
            capturePos = 0;
            playbackPos = latchedChunkSamples - 1;
            aIsCapturing = !aIsCapturing;
        }
        if (playbackPos < 0) playbackPos = 0;

        // Constant-power dry/wet mix
        const float dryGain = std::cos(mix * juce::MathConstants<float>::halfPi);
        const float wetGain = std::sin(mix * juce::MathConstants<float>::halfPi);

        const float dryL = buffer.getSample(0, i);
        const float dryR = numChannels > 1 ? buffer.getSample(1, i) : dryL;

        buffer.setSample(0, i, dryL * dryGain + revL * wetGain);
        if (numChannels > 1)
            buffer.setSample(1, i, dryR * dryGain + revR * wetGain);
    }
}
