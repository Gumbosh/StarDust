#include "DestroyDrive.h"

static float fastTanh(float x) noexcept
{
    if (x > 4.0f) return 1.0f;
    if (x < -4.0f) return -1.0f;
    const float x2 = x * x;
    return x * (27.0f + x2) / (27.0f + 9.0f * x2);
}

static constexpr float kSatKnee = 0.7f;
static constexpr float kInvTanhKnee = 1.6544f; // ≈ 1/tanh(0.7)

void DestroyDrive::prepare(double sampleRate, int /*samplesPerBlock*/)
{
    constexpr double rampTimeSec = 0.005; // 5ms smoothing
    inputGainSmoothed.reset(sampleRate, rampTimeSec);
    outputGainSmoothed.reset(sampleRate, rampTimeSec);
}

void DestroyDrive::setInputDrive(float gainDb)
{
    if (currentInputDb != gainDb)
    {
        currentInputDb = gainDb;
        inputGainSmoothed.setTargetValue(dBToLinear(gainDb));
    }
}

void DestroyDrive::setOutputColor(float gainDb)
{
    if (currentOutputDb != gainDb)
    {
        currentOutputDb = gainDb;
        outputGainSmoothed.setTargetValue(dBToLinear(gainDb));
    }
}

void DestroyDrive::processInput(juce::AudioBuffer<float>& buffer)
{
    juce::ScopedNoDenormals noDenormals;
    const auto numChannels = std::min(buffer.getNumChannels(), 2);
    const auto numSamples = buffer.getNumSamples();

    for (int i = 0; i < numSamples; ++i)
    {
        const float gain = inputGainSmoothed.getNextValue();
        for (int ch = 0; ch < numChannels; ++ch)
        {
            float& s = buffer.getWritePointer(ch)[i];
            s *= gain;
            s = fastTanh(s * kSatKnee) * kInvTanhKnee;
        }
    }
}

void DestroyDrive::processOutput(juce::AudioBuffer<float>& buffer)
{
    juce::ScopedNoDenormals noDenormals;
    const auto numChannels = std::min(buffer.getNumChannels(), 2);
    const auto numSamples = buffer.getNumSamples();

    for (int i = 0; i < numSamples; ++i)
    {
        const float gain = outputGainSmoothed.getNextValue();
        for (int ch = 0; ch < numChannels; ++ch)
            buffer.getWritePointer(ch)[i] *= gain;
    }
}
