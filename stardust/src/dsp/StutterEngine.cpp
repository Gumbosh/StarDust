#include "StutterEngine.h"

void StutterEngine::prepare(double sr, int /*samplesPerBlock*/)
{
    sampleRate = sr;
    constexpr double kRampSec = 0.02;
    rateSmoothed.reset(sr, kRampSec);
    decaySmoothed.reset(sr, kRampSec);
    depthSmoothed.reset(sr, kRampSec);
    mixSmoothed.reset(sr, kRampSec);

    writePos = 0;
    readPos = 0;
    capturePos = 0;
    currentDecayGain = 1.0f;
    sliceLengthSamples = static_cast<int>(125.0 * sr / 1000.0);
    latchedSliceLen = sliceLengthSamples;

    for (int ch = 0; ch < kMaxChannels; ++ch)
        captureBuffer[ch].fill(0.0f);

    // Precompute crossfade window (raised cosine: 0→1)
    const float pi = juce::MathConstants<float>::pi;
    for (int i = 0; i < kCrossfadeLen; ++i)
        crossfadeWindow[i] = 0.5f * (1.0f - std::cos(pi * static_cast<float>(i) / static_cast<float>(kCrossfadeLen)));
}

void StutterEngine::setRate(float ms)    { rateSmoothed.setTargetValue(juce::jlimit(10.0f, 1000.0f, ms)); }
void StutterEngine::setDecay(float d)    { decaySmoothed.setTargetValue(juce::jlimit(0.0f, 1.0f, d)); }
void StutterEngine::setDepth(float d)    { depthSmoothed.setTargetValue(juce::jlimit(0.0f, 1.0f, d)); }
void StutterEngine::setMix(float m)      { mixSmoothed.setTargetValue(juce::jlimit(0.0f, 1.0f, m)); }

void StutterEngine::process(juce::AudioBuffer<float>& buffer)
{
    juce::ScopedNoDenormals noDenormals;
    const auto numChannels = std::min(buffer.getNumChannels(), kMaxChannels);
    const auto numSamples = buffer.getNumSamples();

    for (int i = 0; i < numSamples; ++i)
    {
        const float rate = rateSmoothed.getNextValue();
        const float decay = decaySmoothed.getNextValue();
        const float depth = depthSmoothed.getNextValue();
        const float mix = mixSmoothed.getNextValue();

        // Update target slice length (latched at loop boundary)
        sliceLengthSamples = juce::jlimit(64, kBufferSize - 1,
            static_cast<int>(rate * static_cast<float>(sampleRate) / 1000.0f));

        // Always write input to capture buffer
        for (int ch = 0; ch < numChannels; ++ch)
            captureBuffer[ch][writePos] = buffer.getSample(ch, i);

        // Read stutter playback
        float stutterOut[kMaxChannels] = {};
        const int readIdx = (capturePos + readPos) & kBufferMask;

        for (int ch = 0; ch < numChannels; ++ch)
            stutterOut[ch] = captureBuffer[ch][readIdx];

        // Crossfade at loop boundary to prevent clicks
        const int distToEnd = latchedSliceLen - readPos;
        if (distToEnd > 0 && distToEnd <= kCrossfadeLen)
        {
            const float fadeOut = crossfadeWindow[kCrossfadeLen - distToEnd]; // 1→0
            const float fadeIn  = 1.0f - fadeOut;                             // 0→1
            // Crossfade target: beginning of captured slice (readPos - latchedSliceLen + distToEnd == 0)
            const int nextIdx = (capturePos) & kBufferMask;

            for (int ch = 0; ch < numChannels; ++ch)
            {
                const float nextSample = captureBuffer[ch][nextIdx];
                stutterOut[ch] = stutterOut[ch] * fadeOut + nextSample * fadeIn;
            }
        }

        // Apply decay
        for (int ch = 0; ch < numChannels; ++ch)
            stutterOut[ch] *= currentDecayGain;

        // Advance read position
        ++readPos;
        if (readPos >= latchedSliceLen)
        {
            readPos = 0;
            currentDecayGain *= decay;

            // Re-capture when decay fades out
            if (currentDecayGain < 0.001f)
                currentDecayGain = 1.0f;

            // Re-capture: grab the most recent slice
            capturePos = (writePos - sliceLengthSamples + kBufferSize) & kBufferMask;
            latchedSliceLen = sliceLengthSamples; // latch new rate at boundary
        }

        writePos = (writePos + 1) & kBufferMask;

        // Depth blend: how much stutter replaces input
        // Then constant-power dry/wet mix
        const float dryGain = std::cos(mix * juce::MathConstants<float>::halfPi);
        const float wetGain = std::sin(mix * juce::MathConstants<float>::halfPi);

        for (int ch = 0; ch < numChannels; ++ch)
        {
            const float dry = buffer.getSample(ch, i);
            const float blended = dry * (1.0f - depth) + stutterOut[ch] * depth;
            buffer.setSample(ch, i, dry * dryGain + blended * wetGain);
        }
    }
}
