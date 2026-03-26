#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <cmath>

// Pure dB gain stages for SP-950 style input/output level control.
// Input gain drives signal into the 12-bit bitcrusher — saturation comes
// from the quantizer clipping, not from a waveshaper.
class DestroyDrive
{
public:
    DestroyDrive() = default;

    void prepare(double sampleRate, int samplesPerBlock);

    void setInputDrive(float gainDb);   // -12.0 to +12.0 dB
    void setOutputColor(float gainDb);  // -12.0 to +12.0 dB

    void processInput(juce::AudioBuffer<float>& buffer);
    void processOutput(juce::AudioBuffer<float>& buffer);

private:
    static float dBToLinear(float dB) { return std::pow(10.0f, dB / 20.0f); }

    juce::SmoothedValue<float> inputGainSmoothed { 1.0f };
    juce::SmoothedValue<float> outputGainSmoothed { 1.0f };

    float currentInputDb = 0.0f;
    float currentOutputDb = 0.0f;
};
