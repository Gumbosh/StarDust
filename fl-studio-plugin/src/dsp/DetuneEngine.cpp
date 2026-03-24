#include "DetuneEngine.h"

void DetuneEngine::prepare(double sampleRate, int /*samplesPerBlock*/)
{
    hostSampleRate = sampleRate;
    detuneSmoothed.reset(sampleRate, 0.02);

    for (int ch = 0; ch < kMaxChannels; ++ch)
    {
        holdCounter[ch] = 0.0f;
        holdSample[ch] = 0.0f;
        holdCounter2[ch] = 0.0f;
        holdSample2[ch] = 0.0f;
    }
}

void DetuneEngine::setDetune(float rpmDetune)
{
    if (currentDetune != rpmDetune)
    {
        currentDetune = rpmDetune;
        detuneSmoothed.setTargetValue(rpmDetune);
    }
}

void DetuneEngine::process(juce::AudioBuffer<float>& buffer)
{
    const auto numChannels = buffer.getNumChannels();
    const auto numSamples = buffer.getNumSamples();

    for (int i = 0; i < numSamples; ++i)
    {
        const float detune = detuneSmoothed.getNextValue();

        // Bypass when detune is near zero
        if (detune < 0.001f)
            continue;

        // pitchFactor simulates the RPM ratio
        // detune=0.5 → ~1.68 (45/33 RPM), detune=1.0 → ~2.36 (78/33 RPM)
        const float pitchFactor = 1.0f + detune * 1.36f;

        // Pass 1: Resample at (hostRate / pitchFactor) — simulates "pitched up"
        // sampling through the ADC. This folds aliasing at a lower frequency.
        const float ratio1 = 1.0f / pitchFactor;

        // Pass 2: Resample at (hostRate * pitchFactor) — simulates the
        // inverse operation. Creates a second layer of aliasing artifacts.
        // Use a slightly different ratio to create the characteristic
        // metallic quality from double-resampling.
        const float ratio2 = 1.0f / (1.0f + detune * 0.68f);

        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto* data = buffer.getWritePointer(ch);

            // Pass 1: downsample (simulates pitch-up through low-res ADC)
            holdCounter[ch] += ratio1;
            if (holdCounter[ch] >= 1.0f)
            {
                holdCounter[ch] -= 1.0f;
                holdSample[ch] = data[i];
            }
            float sample = holdSample[ch];

            // Pass 2: second resample layer (simulates pitch-down reconstruction)
            holdCounter2[ch] += ratio2;
            if (holdCounter2[ch] >= 1.0f)
            {
                holdCounter2[ch] -= 1.0f;
                holdSample2[ch] = sample;
            }

            data[i] = holdSample2[ch];
        }
    }
}
