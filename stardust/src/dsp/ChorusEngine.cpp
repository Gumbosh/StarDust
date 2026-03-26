#include "ChorusEngine.h"

void ChorusEngine::prepare(double newSampleRate, int /*samplesPerBlock*/)
{
    sampleRate = newSampleRate;
    mixSmoothed.reset(sampleRate, 0.02);

    for (auto& ch : delayBuffer)
        ch.fill(0.0f);
    writePos = 0;

    const float sr = static_cast<float>(sampleRate);
    const float pi = juce::MathConstants<float>::pi;

    // --- EQ coefficients for wet signal ---

    // High-pass ~50Hz (low cut) — 2nd order Butterworth
    {
        const float freq = 50.0f;
        const float w0 = 2.0f * pi * freq / sr;
        const float cosW0 = std::cos(w0);
        const float alpha = std::sin(w0) / (2.0f * 0.707f);
        const float a0 = 1.0f + alpha;
        hpB0 = ((1.0f + cosW0) / 2.0f) / a0;
        hpB1 = -(1.0f + cosW0) / a0;
        hpB2 = ((1.0f + cosW0) / 2.0f) / a0;
        hpA1 = (-2.0f * cosW0) / a0;
        hpA2 = (1.0f - alpha) / a0;
    }

    // Low-pass ~5kHz (high shelf rolloff) — 2nd order Butterworth
    {
        const float freq = 5000.0f;
        const float w0 = 2.0f * pi * freq / sr;
        const float cosW0 = std::cos(w0);
        const float alpha = std::sin(w0) / (2.0f * 0.707f);
        const float a0 = 1.0f + alpha;
        lpB0 = ((1.0f - cosW0) / 2.0f) / a0;
        lpB1 = (1.0f - cosW0) / a0;
        lpB2 = ((1.0f - cosW0) / 2.0f) / a0;
        lpA1 = (-2.0f * cosW0) / a0;
        lpA2 = (1.0f - alpha) / a0;
    }

    for (auto& s : hpState) { s.z1 = 0.0f; s.z2 = 0.0f; }
    for (auto& s : lpState) { s.z1 = 0.0f; s.z2 = 0.0f; }

    // --- Voice setup ---

    // Tighter delay spread (closer to 0ms pre-delay like Multiply default)
    const float baseDelaysMs[kNumVoices] = { 1.0f, 3.0f, 5.5f, 8.0f };
    const float pans[kNumVoices] = { 0.0f, 1.0f, 0.2f, 0.8f };

    const float allpassCoeffs[kAllpassStages] = {
        0.55f, 0.62f, 0.48f, 0.67f, 0.53f, 0.60f, 0.45f, 0.58f
    };

    const int allpassDelays[kNumVoices][kAllpassStages] = {
        { 13, 29, 41, 67, 89, 113, 149, 179 },
        { 17, 37, 53, 73, 97, 127, 157, 191 },
        { 19, 31, 47, 71, 101, 131, 163, 197 },
        { 23, 43, 59, 79, 107, 137, 167, 199 }
    };

    for (int i = 0; i < kNumVoices; ++i)
    {
        auto& v = voices[static_cast<size_t>(i)];
        const float voiceIdx = static_cast<float>(i);
        const float phaseOffset = voiceIdx / static_cast<float>(kNumVoices)
                                  * juce::MathConstants<float>::twoPi;

        v.baseDelaySamples = baseDelaysMs[i] * 0.001f * sr;
        v.pan = pans[i];

        // Subtle FM: ~1Hz, 20% depth of 1ms = 0.2ms modulation
        // Just enough to add life without audible pitch shift
        v.fmLfoRate = 1.0f + (voiceIdx - 1.5f) * 0.1f; // 0.85, 0.95, 1.05, 1.15 Hz
        v.fmLfoPhase = phaseOffset;
        v.fmDepthSamples = 0.2f * 0.001f * sr; // 0.2ms

        // AM: volume shimmer
        v.amLfoRate = 0.8f + voiceIdx * 0.15f;
        v.amLfoPhase = phaseOffset + juce::MathConstants<float>::halfPi;
        v.amDepth = 0.3f;

        // Allpass chain
        for (int s = 0; s < kAllpassStages; ++s)
        {
            auto& ap = v.allpass[static_cast<size_t>(s)];
            ap.coeff = allpassCoeffs[s] + static_cast<float>(allpassDelays[i][s] % 7) * 0.02f;
            ap.coeff = juce::jlimit(0.3f, 0.8f, ap.coeff);
            ap.bufferL = 0.0f;
            ap.bufferR = 0.0f;
        }
    }
}

void ChorusEngine::setMix(float mix)
{
    if (currentMix != mix)
    {
        currentMix = mix;
        mixSmoothed.setTargetValue(mix);
    }
}

void ChorusEngine::setPans(float outer, float inner)
{
    voices[0].pan = 0.5f - outer * 0.5f;
    voices[1].pan = 0.5f + outer * 0.5f;
    voices[2].pan = 0.5f - inner * 0.5f;
    voices[3].pan = 0.5f + inner * 0.5f;
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

float ChorusEngine::processBiquad(float input, float b0, float b1, float b2,
                                   float a1, float a2, BiquadState& state)
{
    const float output = b0 * input + state.z1;
    state.z1 = b1 * input - a1 * output + state.z2;
    state.z2 = b2 * input - a2 * output;
    return output;
}

void ChorusEngine::process(juce::AudioBuffer<float>& buffer)
{
    juce::ScopedNoDenormals noDenormals;
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

        float outL = 0.0f;
        float outR = 0.0f;

        for (auto& v : voices)
        {
            // Subtle FM: modulate delay time slightly
            const float fmLfo = std::sin(v.fmLfoPhase);
            const float delaySamples = v.baseDelaySamples + fmLfo * v.fmDepthSamples;

            // Read from delay line
            float sampleL = readInterpolated(0, delaySamples);
            float sampleR = isStereo ? readInterpolated(1, delaySamples) : sampleL;

            // Phase randomization: cascaded allpass filters
            for (auto& ap : v.allpass)
            {
                float outApL = -ap.coeff * sampleL + ap.bufferL;
                ap.bufferL = sampleL + ap.coeff * outApL;
                sampleL = outApL;

                float outApR = -ap.coeff * sampleR + ap.bufferR;
                ap.bufferR = sampleR + ap.coeff * outApR;
                sampleR = outApR;
            }

            // AM: volume shimmer
            const float amLfo = std::sin(v.amLfoPhase);
            const float ampMod = 1.0f - v.amDepth + v.amDepth * (0.5f + 0.5f * amLfo);
            sampleL *= ampMod;
            sampleR *= ampMod;

            // Hard stereo placement
            const float gainL = 1.0f - v.pan;
            const float gainR = v.pan;

            outL += sampleL * gainL;
            outR += sampleR * gainR;

            // Advance LFOs
            v.fmLfoPhase += juce::MathConstants<float>::twoPi * v.fmLfoRate * lfoInc;
            if (v.fmLfoPhase >= juce::MathConstants<float>::twoPi)
                v.fmLfoPhase -= juce::MathConstants<float>::twoPi;

            v.amLfoPhase += juce::MathConstants<float>::twoPi * v.amLfoRate * lfoInc;
            if (v.amLfoPhase >= juce::MathConstants<float>::twoPi)
                v.amLfoPhase -= juce::MathConstants<float>::twoPi;
        }

        // EQ on wet signal: low cut 50Hz + high shelf rolloff 5kHz
        outL = processBiquad(outL, hpB0, hpB1, hpB2, hpA1, hpA2, hpState[0]);
        outL = processBiquad(outL, lpB0, lpB1, lpB2, lpA1, lpA2, lpState[0]);

        outR = processBiquad(outR, hpB0, hpB1, hpB2, hpA1, hpA2, hpState[1]);
        outR = processBiquad(outR, lpB0, lpB1, lpB2, lpA1, lpA2, lpState[1]);

        // Constant-power crossfade
        const float dryGain = std::cos(mix * juce::MathConstants<float>::halfPi);
        const float wetGain = std::sin(mix * juce::MathConstants<float>::halfPi);
        auto* dataL = buffer.getWritePointer(0);
        dataL[i] = dataL[i] * dryGain + outL * wetGain;

        if (isStereo)
        {
            auto* dataR = buffer.getWritePointer(1);
            dataR[i] = dataR[i] * dryGain + outR * wetGain;
        }

        writePos = (writePos + 1) % kDelayBufferSize;
    }
}
