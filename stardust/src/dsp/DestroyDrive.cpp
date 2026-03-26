#include "DestroyDrive.h"

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
    const auto numChannels = buffer.getNumChannels();
    const auto numSamples = buffer.getNumSamples();

    for (int i = 0; i < numSamples; ++i)
    {
        const float gain = inputGainSmoothed.getNextValue();
        for (int ch = 0; ch < numChannels; ++ch)
            buffer.getWritePointer(ch)[i] *= gain;
    }
}

void DestroyDrive::processOutput(juce::AudioBuffer<float>& buffer)
{
    juce::ScopedNoDenormals noDenormals;
    const auto numChannels = buffer.getNumChannels();
    const auto numSamples = buffer.getNumSamples();

    for (int i = 0; i < numSamples; ++i)
    {
        const float gain = outputGainSmoothed.getNextValue();
        for (int ch = 0; ch < numChannels; ++ch)
            buffer.getWritePointer(ch)[i] *= gain;
    }
}
