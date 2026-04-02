#include "MultiplyEngine.h"

void MultiplyEngine::prepare(double newSampleRate, int /*samplesPerBlock*/)
{
    sampleRate = newSampleRate;
    mixSmoothed.reset(sampleRate, 0.02);
    speedSmoothed.reset(sampleRate, 0.02);

    for (auto& ch : delayBuffer)
        ch.fill(0.0f);
    writePos = 0;

    const float sr = static_cast<float>(sampleRate);

    // Acon Digital Multiply-style:
    //   4 voices, 0ms pre-delay (very short base), FM depth ~1ms @ 1Hz ≈ ±10 cents.
    //   No allpass chains — pure FM/AM unison, no phase smearing.
    //   FM: delay = fmDepth * (1 + sin(phase)), oscillates 0 → 2*depth, never negative.
    //   AM depth 50% — Acon default.
    const float fmDepthMs  = 1.0f;   // ≈ ±10 cents at 1Hz
    const float baseMs     = 0.05f;  // tiny offset so delay never hits 0
    const float defaultPans[kNumVoices] = { 0.0f, 1.0f, 0.33f, 0.67f };

    for (int i = 0; i < kNumVoices; ++i)
    {
        auto& v = voices[static_cast<size_t>(i)];
        const float fi          = static_cast<float>(i);
        const float phaseOffset = fi / static_cast<float>(kNumVoices)
                                  * juce::MathConstants<float>::twoPi;

        v.baseDelaySamples = baseMs * 0.001f * sr;
        v.pan              = defaultPans[i];
        v.panTarget        = defaultPans[i];

        // FM Rate ~1Hz, slight per-voice detuning for organic movement
        v.fmLfoRate      = 1.0f + (fi - 1.5f) * 0.08f; // 0.88, 0.96, 1.04, 1.12 Hz
        v.fmLfoPhase     = phaseOffset;
        v.fmDepthSamples = fmDepthMs * 0.001f * sr;

        // AM Rate ~1Hz, offset from FM
        v.amLfoRate  = 1.0f + (fi - 1.5f) * 0.1f;
        v.amLfoPhase = phaseOffset + juce::MathConstants<float>::halfPi;
        v.amDepth    = 0.5f; // Acon default 50%
    }
}

void MultiplyEngine::setMix(float mix)
{
    if (currentMix != mix)
    {
        currentMix = mix;
        mixSmoothed.setTargetValue(mix);
    }
}

void MultiplyEngine::setSpeed(float speed)
{
    if (currentSpeed != speed)
    {
        currentSpeed = speed;
        speedSmoothed.setTargetValue(speed);
    }
}

void MultiplyEngine::setPans(float outer, float inner)
{
    voices[0].panTarget = 0.5f - outer * 0.5f;
    voices[1].panTarget = 0.5f + outer * 0.5f;
    voices[2].panTarget = 0.5f - inner * 0.5f;
    voices[3].panTarget = 0.5f + inner * 0.5f;
}

float MultiplyEngine::readInterpolated(int channel, float delaySamples) const
{
    const float readPos = static_cast<float>(writePos) - delaySamples;
    const int   pos0    = (static_cast<int>(std::floor(readPos)) % kDelayBufferSize + kDelayBufferSize) % kDelayBufferSize;
    const int   pos1    = (pos0 + 1) % kDelayBufferSize;
    const float frac    = readPos - std::floor(readPos);
    return delayBuffer[static_cast<size_t>(channel)][static_cast<size_t>(pos0)] * (1.0f - frac)
         + delayBuffer[static_cast<size_t>(channel)][static_cast<size_t>(pos1)] * frac;
}

void MultiplyEngine::process(juce::AudioBuffer<float>& buffer)
{
    juce::ScopedNoDenormals noDenormals;
    const int  numChannels = buffer.getNumChannels();
    const int  numSamples  = buffer.getNumSamples();
    const bool isStereo    = numChannels > 1;
    const float lfoInc     = 1.0f / static_cast<float>(sampleRate);

    for (int i = 0; i < numSamples; ++i)
    {
        const float mix      = mixSmoothed.getNextValue();
        const float speedMul = speedSmoothed.getNextValue();

        // Write both channels to delay buffer
        for (int ch = 0; ch < numChannels; ++ch)
            delayBuffer[static_cast<size_t>(ch)][static_cast<size_t>(writePos)]
                = buffer.getReadPointer(ch)[i];

        if (mix < 0.001f)
        {
            writePos = (writePos + 1) % kDelayBufferSize;
            continue;
        }

        float outL = 0.0f, outR = 0.0f;

        for (int vi = 0; vi < kNumVoices; ++vi)
        {
            auto& v = voices[static_cast<size_t>(vi)];

            // Smooth pan (per-sample, no zipper noise)
            constexpr float kPanSlew = 0.001f;
            v.pan += kPanSlew * (v.panTarget - v.pan);

            // FM: delay = base + depth * (1 + sin) → always positive, oscillates 0→2*depth
            const float fmLfo    = std::sin(v.fmLfoPhase);
            const float delaySmp = v.baseDelaySamples + v.fmDepthSamples * (1.0f + fmLfo);

            // Read L and R separately (preserves stereo content from input)
            float sampleL = readInterpolated(0, delaySmp);
            float sampleR = isStereo ? readInterpolated(1, delaySmp) : sampleL;

            // AM shimmer (50% depth — Acon default)
            const float amLfo  = std::sin(v.amLfoPhase);
            const float ampMod = 1.0f - v.amDepth + v.amDepth * (0.5f + 0.5f * amLfo);
            sampleL *= ampMod;
            sampleR *= ampMod;

            // Linear stereo placement (1+2 outer, 3+4 inner)
            outL += sampleL * (1.0f - v.pan);
            outR += sampleR * v.pan;

            // Advance LFOs
            v.fmLfoPhase += juce::MathConstants<float>::twoPi * v.fmLfoRate * speedMul * lfoInc;
            if (v.fmLfoPhase >= juce::MathConstants<float>::twoPi)
                v.fmLfoPhase -= juce::MathConstants<float>::twoPi;

            v.amLfoPhase += juce::MathConstants<float>::twoPi * v.amLfoRate * speedMul * lfoInc;
            if (v.amLfoPhase >= juce::MathConstants<float>::twoPi)
                v.amLfoPhase -= juce::MathConstants<float>::twoPi;
        }

        // Constant-power crossfade (Acon-style: dry preserved at mix=0)
        const float dryGain = std::cos(mix * juce::MathConstants<float>::halfPi);
        const float wetGain = std::sin(mix * juce::MathConstants<float>::halfPi);

        buffer.getWritePointer(0)[i] = buffer.getReadPointer(0)[i] * dryGain + outL * wetGain;
        if (isStereo)
            buffer.getWritePointer(1)[i] = buffer.getReadPointer(1)[i] * dryGain + outR * wetGain;

        writePos = (writePos + 1) % kDelayBufferSize;
    }
}
