#include "ChorusEngine.h"

void ChorusEngine::prepare(double newSampleRate, int /*samplesPerBlock*/)
{
    sampleRate = newSampleRate;
    mixSmoothed.reset(sampleRate, 0.02);

    for (auto& ch : delayBuffer)
        ch.fill(0.0f);
    writePos = 0;

    // Acon Digital Multiply defaults:
    // FM: Rate=1.00Hz, Depth=20%
    // AM: Rate=1.00Hz, Depth=50%
    // Voice count=4, Stereo spread=100%, Pre-delay=0ms
    //
    // Key difference from basic chorus: each voice reads a mono sum of input
    // and distributes to L/R via constant-power pan. This creates true stereo
    // separation rather than just delaying each channel independently.

    const float fmDepthMs = 2.0f; // 20% of 10ms range

    for (int i = 0; i < kNumVoices; ++i)
    {
        auto& v = voices[static_cast<size_t>(i)];
        const float voiceIdx = static_cast<float>(i);
        const float phaseOffset = voiceIdx / static_cast<float>(kNumVoices)
                                  * juce::MathConstants<float>::twoPi;

        // FM LFO: ~1Hz with slight per-voice detuning for organic feel
        v.fmLfoRate = 1.0f + (voiceIdx - 1.5f) * 0.08f; // 0.88, 0.96, 1.04, 1.12 Hz
        v.fmLfoPhase = phaseOffset;
        v.fmDepthSamples = fmDepthMs * 0.001f * static_cast<float>(sampleRate);

        // Very short base delay, slightly staggered per voice
        v.baseDelaySamples = (0.3f + voiceIdx * 0.2f) * 0.001f * static_cast<float>(sampleRate);

        // AM LFO: ~1Hz, offset from FM for movement
        v.amLfoRate = 1.0f + (voiceIdx - 1.5f) * 0.1f; // 0.85, 0.95, 1.05, 1.15 Hz
        v.amLfoPhase = phaseOffset + juce::MathConstants<float>::halfPi;
        v.amDepth = 0.50f;
    }

    // Full stereo spread — voices distributed evenly across the stereo field
    voices[0].pan = 0.0f;   // hard left
    voices[1].pan = 1.0f;   // hard right
    voices[2].pan = 0.33f;  // left-center
    voices[3].pan = 0.67f;  // right-center
}

void ChorusEngine::setMix(float mix)
{
    if (currentMix != mix)
    {
        currentMix = mix;
        mixSmoothed.setTargetValue(mix);
    }
}

float ChorusEngine::readInterpolated(int channel, float delaySamples) const
{
    const float readPos = static_cast<float>(writePos) - delaySamples;
    const int pos0 = (static_cast<int>(readPos) % kDelayBufferSize + kDelayBufferSize) % kDelayBufferSize;
    const int pos1 = (pos0 + 1) % kDelayBufferSize;
    const float frac = readPos - std::floor(readPos);

    return delayBuffer[static_cast<size_t>(channel)][static_cast<size_t>(pos0)] * (1.0f - frac)
         + delayBuffer[static_cast<size_t>(channel)][static_cast<size_t>(pos1)] * frac;
}

void ChorusEngine::process(juce::AudioBuffer<float>& buffer)
{
    const auto numChannels = buffer.getNumChannels();
    const auto numSamples = buffer.getNumSamples();
    const bool isStereo = numChannels > 1;
    const float lfoInc = 1.0f / static_cast<float>(sampleRate);

    for (int i = 0; i < numSamples; ++i)
    {
        const float mix = mixSmoothed.getNextValue();

        // Write input to delay buffer
        for (int ch = 0; ch < numChannels; ++ch)
            delayBuffer[static_cast<size_t>(ch)][static_cast<size_t>(writePos)] = buffer.getReadPointer(ch)[i];

        if (mix < 0.001f)
        {
            writePos = (writePos + 1) % kDelayBufferSize;
            continue;
        }

        float chorusL = 0.0f;
        float chorusR = 0.0f;

        for (auto& v : voices)
        {
            // FM: modulate delay time
            const float fmLfo = std::sin(v.fmLfoPhase);
            const float delaySamples = v.baseDelaySamples + (1.0f + fmLfo) * v.fmDepthSamples;

            // AM: modulate amplitude
            const float amLfo = std::sin(v.amLfoPhase);
            const float ampMod = 1.0f - v.amDepth + v.amDepth * (0.5f + 0.5f * amLfo);

            // Read mono sum from delay buffer (like Multiply — cleaner separation)
            float monoSample = readInterpolated(0, delaySamples);
            if (isStereo)
                monoSample = (monoSample + readInterpolated(1, delaySamples)) * 0.5f;
            monoSample *= ampMod;

            // Distribute to stereo via constant-power pan
            const float panL = std::cos(v.pan * juce::MathConstants<float>::halfPi);
            const float panR = std::sin(v.pan * juce::MathConstants<float>::halfPi);

            chorusL += monoSample * panL;
            chorusR += monoSample * panR;

            // Advance LFOs
            v.fmLfoPhase += juce::MathConstants<float>::twoPi * v.fmLfoRate * lfoInc;
            if (v.fmLfoPhase >= juce::MathConstants<float>::twoPi)
                v.fmLfoPhase -= juce::MathConstants<float>::twoPi;

            v.amLfoPhase += juce::MathConstants<float>::twoPi * v.amLfoRate * lfoInc;
            if (v.amLfoPhase >= juce::MathConstants<float>::twoPi)
                v.amLfoPhase -= juce::MathConstants<float>::twoPi;
        }

        // Normalize
        const float norm = 1.0f / static_cast<float>(kNumVoices);
        chorusL *= norm;
        chorusR *= norm;

        // Dry stays at unity, chorus added on top (like Multiply's 0dB dry)
        auto* dataL = buffer.getWritePointer(0);
        dataL[i] = dataL[i] + chorusL * mix;

        if (isStereo)
        {
            auto* dataR = buffer.getWritePointer(1);
            dataR[i] = dataR[i] + chorusR * mix;
        }

        writePos = (writePos + 1) % kDelayBufferSize;
    }
}
