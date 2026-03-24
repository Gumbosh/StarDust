#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <cmath>

class Saturation
{
public:
    Saturation() = default;

    void prepare(double sampleRate, int samplesPerBlock);

    void setInputGain(float gainDb);
    void setOutputGain(float gainDb);
    void setDrive(float driveAmount);

    void processInput(juce::AudioBuffer<float>& buffer);
    void processOutput(juce::AudioBuffer<float>& buffer);

private:
    void applyGainAndSaturation(juce::AudioBuffer<float>& buffer,
                                 juce::SmoothedValue<float>& gainSmoothed,
                                 juce::SmoothedValue<float>& driveSmoothed);

    static float dBToLinear(float dB) { return std::pow(10.0f, dB / 20.0f); }

    juce::SmoothedValue<float> inputGainSmoothed { 1.0f };
    juce::SmoothedValue<float> outputGainSmoothed { 1.0f };
    juce::SmoothedValue<float> driveSmoothed { 0.0f };

    float currentInputGainDb = 0.0f;
    float currentOutputGainDb = 0.0f;
    float currentDrive = 0.0f;

    double sampleRate = 44100.0;
};
