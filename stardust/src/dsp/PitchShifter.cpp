#include "PitchShifter.h"
#include "FastMath.h"

void PitchShifter::prepare(double sr, int /*samplesPerBlock*/)
{
    sampleRate = sr;
    constexpr double kRampSec = 0.02;
    pitchSmoothed.reset(sr, kRampSec);
    feedbackSmoothed.reset(sr, kRampSec);
    toneSmoothed.reset(sr, kRampSec);
    mixSmoothed.reset(sr, kRampSec);

    writePos = 0;
    sawPhase = 0.0f;

    for (int ch = 0; ch < kMaxChannels; ++ch)
    {
        delayBuffer[ch].fill(0.0f);
        lpState[ch] = 0.0f;
        feedbackSample[ch] = 0.0f;
    }

    // Precompute Hann window (full period)
    const float pi = juce::MathConstants<float>::pi;
    for (int i = 0; i < kWindowSamples; ++i)
        hannWindow[i] = 0.5f * (1.0f - std::cos(2.0f * pi * static_cast<float>(i) / static_cast<float>(kWindowSamples)));
}

void PitchShifter::setPitch(float st)
{
    const float snapped = std::round(juce::jlimit(-24.0f, 24.0f, st));
    pitchSmoothed.setTargetValue(snapped);
}
void PitchShifter::setFeedback(float fb) { feedbackSmoothed.setTargetValue(juce::jlimit(0.0f, 0.95f, fb)); }
void PitchShifter::setTone(float hz)     { toneSmoothed.setTargetValue(juce::jlimit(200.0f, 20000.0f, hz)); }
void PitchShifter::setMix(float m)       { mixSmoothed.setTargetValue(juce::jlimit(0.0f, 1.0f, m)); }

float PitchShifter::readHermite(int channel, float delaySamples) const
{
    const float readPos = static_cast<float>(writePos) - delaySamples;
    const int i0 = (static_cast<int>(std::floor(readPos)) - 1 + kDelaySize) & kDelayMask;
    const int i1 = (i0 + 1) & kDelayMask;
    const int i2 = (i0 + 2) & kDelayMask;
    const int i3 = (i0 + 3) & kDelayMask;
    const float frac = readPos - std::floor(readPos);

    const auto& buf = delayBuffer[channel];
    const float y0 = buf[i0], y1 = buf[i1], y2 = buf[i2], y3 = buf[i3];

    const float c0 = y1;
    const float c1 = 0.5f * (y2 - y0);
    const float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
    const float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);

    return ((c3 * frac + c2) * frac + c1) * frac + c0;
}

void PitchShifter::process(juce::AudioBuffer<float>& buffer)
{
    juce::ScopedNoDenormals noDenormals;
    const auto numChannels = std::min(buffer.getNumChannels(), kMaxChannels);
    const auto numSamples = buffer.getNumSamples();
    const float sr = static_cast<float>(sampleRate);

    for (int i = 0; i < numSamples; ++i)
    {
        const float pitch = pitchSmoothed.getNextValue();
        const float fb = feedbackSmoothed.getNextValue();
        const float tone = toneSmoothed.getNextValue();
        const float mix = mixSmoothed.getNextValue();

        const float pitchRatio = std::exp2f(pitch / 12.0f);
        const float sawRate = (1.0f - pitchRatio) / static_cast<float>(kWindowSamples);

        // LP coefficient for feedback tone filter
        const float lpCoeff = 1.0f - std::exp(-juce::MathConstants<float>::twoPi * tone / sr);

        // Write input + feedback into delay line
        for (int ch = 0; ch < numChannels; ++ch)
            delayBuffer[ch][writePos] = buffer.getSample(ch, i) + feedbackSample[ch];

        // Read two taps with Hann crossfade
        const float phaseA = sawPhase;
        float phaseBRaw = sawPhase + 0.5f;
        if (phaseBRaw >= 1.0f) phaseBRaw -= 1.0f;
        const float phaseB = phaseBRaw;

        const float delayA = phaseA * static_cast<float>(kWindowSamples) + 1.0f; // +1 to avoid reading write head
        const float delayB = phaseB * static_cast<float>(kWindowSamples) + 1.0f;

        const int winIdxA = static_cast<int>(phaseA * static_cast<float>(kWindowSamples)) % kWindowSamples;
        const int winIdxB = static_cast<int>(phaseB * static_cast<float>(kWindowSamples)) % kWindowSamples;
        const float windowA = hannWindow[winIdxA];
        const float windowB = hannWindow[winIdxB];

        for (int ch = 0; ch < numChannels; ++ch)
        {
            const float tapA = readHermite(ch, delayA) * windowA;
            const float tapB = readHermite(ch, delayB) * windowB;
            const float wet = tapA + tapB;

            // Feedback path: tone LP → soft clip → scale
            lpState[ch] += lpCoeff * (wet - lpState[ch]);
            const float clipped = FastMath::tanh(lpState[ch]);
            feedbackSample[ch] = clipped * fb;

            // Constant-power dry/wet mix
            const float dryGain = std::cos(mix * juce::MathConstants<float>::halfPi);
            const float wetGain = std::sin(mix * juce::MathConstants<float>::halfPi);
            buffer.setSample(ch, i, buffer.getSample(ch, i) * dryGain + wet * wetGain);
        }

        // Advance sawtooth phase
        sawPhase += sawRate;
        if (sawPhase >= 1.0f) sawPhase -= 1.0f;
        if (sawPhase < 0.0f)  sawPhase += 1.0f;

        writePos = (writePos + 1) & kDelayMask;
    }
}
