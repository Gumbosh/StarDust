#include "Saturation.h"

void Saturation::prepare(double newSampleRate, int /*samplesPerBlock*/)
{
    sampleRate = newSampleRate;
    const double rampTimeSec = 0.02; // 20ms ramp
    inputGainSmoothed.reset(sampleRate, rampTimeSec);
    outputGainSmoothed.reset(sampleRate, rampTimeSec);
    driveSmoothed.reset(sampleRate, rampTimeSec);

    for (int ch = 0; ch < kMaxChannels; ++ch)
    {
        dcX1[ch] = 0.0f;
        dcY1[ch] = 0.0f;
    }
}

void Saturation::setInputGain(float gainDb)
{
    if (currentInputGainDb != gainDb)
    {
        currentInputGainDb = gainDb;
        inputGainSmoothed.setTargetValue(dBToLinear(gainDb));
    }
}

void Saturation::setOutputGain(float gainDb)
{
    if (currentOutputGainDb != gainDb)
    {
        currentOutputGainDb = gainDb;
        outputGainSmoothed.setTargetValue(dBToLinear(gainDb));
    }
}

void Saturation::setDrive(float driveAmount)
{
    if (currentDrive != driveAmount)
    {
        currentDrive = driveAmount;
        driveSmoothed.setTargetValue(driveAmount);
    }
}

void Saturation::processInput(juce::AudioBuffer<float>& buffer)
{
    juce::ScopedNoDenormals noDenormals;
    applyGainAndSaturation(buffer, inputGainSmoothed, driveSmoothed);

    // DC blocker: y[n] = x[n] - x[n-1] + R * y[n-1], R = 0.995
    const auto numChannels = buffer.getNumChannels();
    const auto numSamples = buffer.getNumSamples();
    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto* data = buffer.getWritePointer(ch);
        for (int i = 0; i < numSamples; ++i)
        {
            const float x = data[i];
            const float y = x - dcX1[ch] + kDcCoeff * dcY1[ch];
            dcX1[ch] = x;
            dcY1[ch] = y;
            data[i] = y;
        }
    }
}

void Saturation::processOutput(juce::AudioBuffer<float>& buffer)
{
    const auto numChannels = buffer.getNumChannels();
    const auto numSamples = buffer.getNumSamples();

    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto* data = buffer.getWritePointer(ch);
        for (int i = 0; i < numSamples; ++i)
        {
            const float gain = (ch == 0)
                ? outputGainSmoothed.getNextValue()
                : outputGainSmoothed.getCurrentValue();
            data[i] *= gain;
        }
    }
}

void Saturation::applyGainAndSaturation(juce::AudioBuffer<float>& buffer,
                                         juce::SmoothedValue<float>& gainSmoothed,
                                         juce::SmoothedValue<float>& drvSmoothed)
{
    const auto numChannels = buffer.getNumChannels();
    const auto numSamples = buffer.getNumSamples();

    for (int i = 0; i < numSamples; ++i)
    {
        const float gain = gainSmoothed.getNextValue();
        const float drive = drvSmoothed.getNextValue();
        const float scale = 1.0f + drive * 4.0f;
        const float normalization = (scale > 1.0f) ? 1.0f / std::tanh(scale) : 1.0f;

        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto* data = buffer.getWritePointer(ch);
            float sample = data[i] * gain;
            sample = std::tanh(sample * scale) * normalization;
            data[i] = sample;
        }
    }
}
