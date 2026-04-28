#include "HarmonicExciter.h"
#include "FastMath.h"
#include <cmath>

void HarmonicExciter::prepare(double sampleRate, int /*samplesPerBlock*/)
{
    sampleRateHz = sampleRate > 0.0 ? sampleRate : 44100.0;
    driveSmoothed.reset(sampleRateHz, 0.015);
    focusCoeffSmoothed.reset(sampleRateHz, 0.015);
    airCoeffSmoothed.reset(sampleRateHz, 0.015);
    mixSmoothed.reset(sampleRateHz, 0.015);
    driveSmoothed.setCurrentAndTargetValue(0.0f);
    lastToneHz = 6000.0f;
    focusCoeffSmoothed.setCurrentAndTargetValue(onePoleCoefficient(lastToneHz, sampleRateHz));
    airCoeffSmoothed.setCurrentAndTargetValue(onePoleCoefficient(lastToneHz * 2.35f, sampleRateHz));
    mixSmoothed.setCurrentAndTargetValue(0.0f);
    reset();
}

void HarmonicExciter::reset()
{
    for (auto& state : states)
        state = {};
}

void HarmonicExciter::process(juce::AudioBuffer<float>& buffer, float drive, float toneHz, float mix)
{
    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();
    if (numSamples == 0 || numChannels == 0)
        return;

    const float targetMix = juce::jlimit(0.0f, 1.0f, mix);
    const float targetToneHz = juce::jlimit(800.0f, 16000.0f, toneHz);
    driveSmoothed.setTargetValue(juce::jlimit(0.0f, 1.0f, drive));
    mixSmoothed.setTargetValue(targetMix);
    if (std::abs(targetToneHz - lastToneHz) > 1.0f)
    {
        lastToneHz = targetToneHz;
        focusCoeffSmoothed.setTargetValue(onePoleCoefficient(lastToneHz, sampleRateHz));
        airCoeffSmoothed.setTargetValue(onePoleCoefficient(juce::jmin(lastToneHz * 2.35f, 19000.0f), sampleRateHz));
    }

    if (targetMix <= 0.0f && mixSmoothed.getCurrentValue() <= 0.0001f && !mixSmoothed.isSmoothing())
        return;

    for (int i = 0; i < numSamples; ++i)
    {
        const float amount = mixSmoothed.getNextValue();
        const float intensity = driveSmoothed.getNextValue();
        const float focusCoeff = focusCoeffSmoothed.getNextValue();
        const float airCoeff = airCoeffSmoothed.getNextValue();
        const float driveGain = 1.0f + intensity * 9.0f;
        const float harmonicLevel = amount * (0.08f + intensity * 0.42f);

        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto& state = stateForChannel(ch);
            auto* data = buffer.getWritePointer(ch);
            const float dry = data[i];

            state.focusLow += focusCoeff * (dry - state.focusLow);
            const float high = dry - state.focusLow;
            const float driven = high * driveGain;

            const float even = FastMath::tanhFast(std::abs(driven) * 1.65f);
            const float odd = FastMath::tanhFast(driven * 2.0f) - FastMath::tanhFast(driven * 0.65f);
            const float generated = even * 0.62f + odd * 0.38f;

            const float dcBlocked = generated - state.dcX1 + 0.995f * state.dcY1;
            state.dcX1 = generated;
            state.dcY1 = dcBlocked;

            state.airLow += airCoeff * (dcBlocked - state.airLow);
            const float airBand = dcBlocked - state.airLow;

            data[i] = dry + airBand * harmonicLevel;
        }
    }
}

float HarmonicExciter::onePoleCoefficient(float cutoffHz, double sampleRate)
{
    const float safeCutoff = juce::jlimit(20.0f, static_cast<float>(sampleRate * 0.45), cutoffHz);
    return 1.0f - std::exp(-2.0f * juce::MathConstants<float>::pi * safeCutoff / static_cast<float>(sampleRate));
}

HarmonicExciter::ChannelState& HarmonicExciter::stateForChannel(int channel) noexcept
{
    return states[static_cast<size_t>(juce::jlimit(0, static_cast<int>(states.size()) - 1, channel))];
}
