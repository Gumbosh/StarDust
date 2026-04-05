#include "GranularEngine.h"
#include "FastMath.h"

void GranularEngine::prepare(double sr, int /*samplesPerBlock*/)
{
    sampleRate = sr;
    constexpr double kRampSec = 0.02;
    sizeSmoothed.reset(sr, kRampSec);
    densitySmoothed.reset(sr, kRampSec);
    pitchSmoothed.reset(sr, kRampSec);
    scatterSmoothed.reset(sr, kRampSec);
    mixSmoothed.reset(sr, kRampSec);

    writePos = 0;
    grainAccumulator = 0.0f;
    nextGrainIndex = 0;
    rngState = 42u;
    reverseAmount = 0.0f;
    syncDivision = 0;
    hostBpm = 120.0;

    for (int ch = 0; ch < kMaxChannels; ++ch)
        captureBuffer[ch].fill(0.0f);

    for (auto& g : grains)
        g.active = false;

    // Precompute Hann window
    const float pi = juce::MathConstants<float>::pi;
    for (int i = 0; i < kWindowSize; ++i)
        hannWindow[i] = 0.5f * (1.0f - std::cos(2.0f * pi * static_cast<float>(i) / static_cast<float>(kWindowSize - 1)));
}

void GranularEngine::setSize(float ms)     { sizeSmoothed.setTargetValue(juce::jlimit(5.0f, 200.0f, ms)); }
void GranularEngine::setDensity(float d)   { densitySmoothed.setTargetValue(juce::jlimit(1.0f, 16.0f, d)); }
void GranularEngine::setPitch(float st)    { pitchSmoothed.setTargetValue(juce::jlimit(-12.0f, 12.0f, st)); }
void GranularEngine::setScatter(float s)   { scatterSmoothed.setTargetValue(juce::jlimit(0.0f, 1.0f, s)); }
void GranularEngine::setReverse(float r)     { reverseAmount = juce::jlimit(0.0f, 1.0f, r); }
void GranularEngine::setMix(float m)         { mixSmoothed.setTargetValue(juce::jlimit(0.0f, 1.0f, m)); }
void GranularEngine::setSyncDivision(int d)  { syncDivision = d; }
void GranularEngine::setHostBpm(double b)    { hostBpm = std::max(20.0, b); }

float GranularEngine::nextRandom()
{
    rngState = rngState * 1664525u + 1013904223u;
    return static_cast<float>(rngState >> 8) / 16777216.0f; // 0-1
}

float GranularEngine::readHermite(int channel, float pos) const
{
    const int i0 = (static_cast<int>(pos) - 1 + kCaptureSize) & kCaptureMask;
    const int i1 = (i0 + 1) & kCaptureMask;
    const int i2 = (i0 + 2) & kCaptureMask;
    const int i3 = (i0 + 3) & kCaptureMask;
    const float frac = pos - std::floor(pos);

    const auto& buf = captureBuffer[channel];
    const float y0 = buf[i0], y1 = buf[i1], y2 = buf[i2], y3 = buf[i3];

    const float c0 = y1;
    const float c1 = 0.5f * (y2 - y0);
    const float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
    const float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);

    return ((c3 * frac + c2) * frac + c1) * frac + c0;
}

float GranularEngine::getWindowValue(float phase) const
{
    // phase is 0-1, map into window table with linear interpolation
    const float idx = phase * static_cast<float>(kWindowSize - 1);
    const int i = static_cast<int>(idx);
    const float frac = idx - static_cast<float>(i);
    const int iClamped = juce::jlimit(0, kWindowSize - 2, i);
    return hannWindow[iClamped] + frac * (hannWindow[iClamped + 1] - hannWindow[iClamped]);
}

float GranularEngine::divisionToMs() const
{
    // Division index to beats:
    // 0=Off, 1=1/64, 2=1/32T, 3=1/32, 4=1/16T, 5=1/16, 6=1/8T,
    // 7=1/8, 8=1/4T, 9=1/4, 10=1/2T, 11=1/2, 12=1bar
    static constexpr float divisions[] = {
        0.0f,          // 0: Off (unused)
        1.0f / 16.0f,  // 1: 1/64
        1.0f / 12.0f,  // 2: 1/32T
        1.0f / 8.0f,   // 3: 1/32
        1.0f / 6.0f,   // 4: 1/16T
        1.0f / 4.0f,   // 5: 1/16
        1.0f / 3.0f,   // 6: 1/8T
        1.0f / 2.0f,   // 7: 1/8
        2.0f / 3.0f,   // 8: 1/4T
        1.0f,          // 9: 1/4
        4.0f / 3.0f,   // 10: 1/2T
        2.0f,          // 11: 1/2
        4.0f,          // 12: 1 bar
    };
    const int idx = juce::jlimit(1, 12, syncDivision.load(std::memory_order_relaxed));
    const float safeBpm = std::max(20.0f, static_cast<float>(hostBpm));
    const float beatMs = 60000.0f / safeBpm;
    return divisions[idx] * beatMs;
}

void GranularEngine::triggerGrain(float sizeSamples, float scatterAmt, float pitchRatio)
{
    auto& g = grains[nextGrainIndex];
    nextGrainIndex = (nextGrainIndex + 1) % kMaxGrains;

    // Scatter: random offset backwards into capture buffer (up to 1.5 seconds)
    const float maxScatter = std::min(static_cast<float>(kCaptureSize) - sizeSamples * 2.0f,
                                       static_cast<float>(sampleRate) * 1.5f);
    const int scatterOffset = static_cast<int>(nextRandom() * scatterAmt * std::max(0.0f, maxScatter));

    // Reverse: probabilistic based on reverseAmount knob
    const bool isReverse = nextRandom() < reverseAmount;

    g.active = true;
    g.startPos = (writePos - static_cast<int>(sizeSamples) - scatterOffset + kCaptureSize) & kCaptureMask;
    g.readPos = isReverse ? sizeSamples : 0.0f;
    g.playbackRate = isReverse ? -pitchRatio : pitchRatio;
    g.grainLength = static_cast<int>(sizeSamples);
    g.reverse = isReverse;
}

void GranularEngine::process(juce::AudioBuffer<float>& buffer)
{
    juce::ScopedNoDenormals noDenormals;
    const auto numChannels = std::min(buffer.getNumChannels(), kMaxChannels);
    const auto numSamples = buffer.getNumSamples();

    for (int i = 0; i < numSamples; ++i)
    {
        // Write input to capture buffer
        for (int ch = 0; ch < numChannels; ++ch)
            captureBuffer[ch][writePos] = buffer.getSample(ch, i);
        writePos = (writePos + 1) & kCaptureMask;

        // Read smoothed parameters
        const float sizeMs = (syncDivision > 0) ? divisionToMs() : sizeSmoothed.getNextValue();
        const float sizeSmp = std::min(sizeMs * static_cast<float>(sampleRate) / 1000.0f,
                                       static_cast<float>(kCaptureSize - 1));
        const float density = densitySmoothed.getNextValue();
        const float pitchSt = pitchSmoothed.getNextValue();
        const float pitchRatio = std::exp2f(pitchSt / 12.0f);
        const float scatter = scatterSmoothed.getNextValue();
        const float mix = mixSmoothed.getNextValue();

        // Grain scheduling: trigger grains based on density
        const float grainRate = density / std::max(sizeSmp, 1.0f);
        grainAccumulator += grainRate;
        while (grainAccumulator >= 1.0f)
        {
            grainAccumulator -= 1.0f;
            triggerGrain(sizeSmp, scatter, pitchRatio);
        }

        // Sum active grains
        float wet[kMaxChannels] = {};
        int activeCount = 0;

        for (auto& g : grains)
        {
            if (!g.active) continue;

            // Phase calculation: 0-1 regardless of direction
            const float phase = g.reverse
                ? 1.0f - (g.readPos / std::max(static_cast<float>(g.grainLength), 1.0f))
                : g.readPos / std::max(static_cast<float>(g.grainLength), 1.0f);

            if (phase >= 1.0f || phase < 0.0f)
            {
                g.active = false;
                continue;
            }

            const float window = getWindowValue(phase);
            for (int ch = 0; ch < numChannels; ++ch)
            {
                const float absReadPos = std::abs(g.readPos);
                const float capturePos = static_cast<float>((g.startPos + static_cast<int>(absReadPos)) & kCaptureMask)
                                       + (absReadPos - std::floor(absReadPos));
                wet[ch] += readHermite(ch, capturePos) * window;
            }

            g.readPos += g.playbackRate;
            ++activeCount;
        }

        // Normalize to prevent loudness scaling with density
        if (activeCount > 1)
        {
            const float norm = 1.0f / std::sqrt(static_cast<float>(activeCount));
            for (int ch = 0; ch < numChannels; ++ch)
                wet[ch] *= norm;
        }

        // Constant-power dry/wet mix
        const float dryGain = std::cos(mix * juce::MathConstants<float>::halfPi);
        const float wetGain = std::sin(mix * juce::MathConstants<float>::halfPi);
        for (int ch = 0; ch < numChannels; ++ch)
            buffer.setSample(ch, i, buffer.getSample(ch, i) * dryGain + wet[ch] * wetGain);
    }
}
