#include "MultiplyEngine.h"

float MultiplyEngine::processAllpass(float input, AllpassState& state)
{
    const int readPos = (state.writePos - state.delaySamples + AllpassState::kAllpassBufSize) & AllpassState::kAllpassBufMask;
    const float delayed = state.buffer[readPos];
    const float output = delayed - kAllpassFeedback * input;
    state.buffer[state.writePos] = input + kAllpassFeedback * output;
    state.writePos = (state.writePos + 1) & AllpassState::kAllpassBufMask;
    return output;
}

float MultiplyEngine::readHermite(int channel, float delaySamples) const
{
    const float readPos = static_cast<float>(writePos) - delaySamples;
    const float readFloor = std::floor(readPos);
    const int idx1 = static_cast<int>(readFloor) & kDelayMask;
    const int idx0 = (idx1 - 1 + kDelayBufferSize) & kDelayMask;
    const int idx2 = (idx1 + 1) & kDelayMask;
    const int idx3 = (idx1 + 2) & kDelayMask;
    const float frac = readPos - readFloor;
    const auto& buf = delayBuffer[static_cast<size_t>(channel)];
    const float y0 = buf[static_cast<size_t>(idx0)];
    const float y1 = buf[static_cast<size_t>(idx1)];
    const float y2 = buf[static_cast<size_t>(idx2)];
    const float y3 = buf[static_cast<size_t>(idx3)];
    const float c1 = 0.5f * (y2 - y0);
    const float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
    const float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
    return ((c3 * frac + c2) * frac + c1) * frac + y1;
}

void MultiplyEngine::prepare(double newSampleRate, int /*samplesPerBlock*/)
{
    sampleRate = newSampleRate;
    mixSmoothed.reset(sampleRate, 0.02);
    speedSmoothed.reset(sampleRate, 0.02);

    for (auto& ch : delayBuffer)
        ch.fill(0.0f);
    writePos = 0;
    sampleCounter = 0;

    const float sr = static_cast<float>(sampleRate);

    // Acon Digital Multiply-style:
    //   4 voices, very short base delays and shallow FM for subtle detune/widening.
    //   Keep group delay low to avoid a chorusy/predelayed sensation.
    //   FM: delay = base + depth * (1 + sin(phase)), oscillates 0 -> 2*depth, never negative.
    //   No AM, no granular pitch shifting — detuning from FM delay modulation only.
    const float fmDepthMs = 0.25f;  // subtle detuning — Acon Multiply is very clean

    // Sub-2ms base delays keep the effect immediate and reduce perceived predelay.
    const float baseDelaysMs[kNumVoices] = { 0.85f, 1.25f, 1.05f, 1.55f };
    const float defaultPans[kNumVoices]  = { 0.0f, 1.0f, 0.33f, 0.67f };

    // Staggered low FM rates for movement without obvious chorus wobble.
    const float fmRates[kNumVoices] = { 0.43f, 0.56f, 0.71f, 0.89f };

    // Per-channel phase offsets: ch1 gets a small irrational offset from ch0
    // so L/R modulation never correlates perfectly
    const float channelPhaseOffset = 0.37f * IncrementalOscillator::kTwoPi; // ~133 degrees

    // Micro-allpass lengths (in samples): very short for transparent decorrelation.
    const int allpassDelays[kNumAllpasses] = {
        static_cast<int>(0.35f * 0.001f * sr),
        static_cast<int>(0.52f * 0.001f * sr),
        static_cast<int>(0.71f * 0.001f * sr),
    };

    for (int i = 0; i < kNumVoices; ++i)
    {
        auto& v = voices[static_cast<size_t>(i)];
        const float fi = static_cast<float>(i);
        const float phaseOffset = fi / static_cast<float>(kNumVoices)
                                  * IncrementalOscillator::kTwoPi;

        v.baseDelaySamples = baseDelaysMs[i] * 0.001f * sr;
        v.pan              = defaultPans[i];
        v.panTarget        = defaultPans[i];
        v.fmDepthSamples   = fmDepthMs * 0.001f * sr;
        v.fmBaseRate       = fmRates[i];

        // Initialize per-channel FM oscillators with different starting phases
        // Ch0 gets the base phase, Ch1 gets an irrational offset
        v.fmOsc[0].init(fmRates[i], sr);
        // Rotate ch0 to the voice's phase offset
        {
            const float c = std::cos(phaseOffset);
            const float s = std::sin(phaseOffset);
            v.fmOsc[0].cosVal = c;
            v.fmOsc[0].sinVal = s;
        }

        v.fmOsc[1].init(fmRates[i], sr);
        // Rotate ch1 to a different phase (base + channel offset)
        {
            const float totalPhase = phaseOffset + channelPhaseOffset;
            const float c = std::cos(totalPhase);
            const float s = std::sin(totalPhase);
            v.fmOsc[1].cosVal = c;
            v.fmOsc[1].sinVal = s;
        }

        // Reset ITD state
        v.itdDelaySamples = 0.0f;

        // Initialize allpass diffusion chains per voice per channel
        for (int ch = 0; ch < kMaxChannels; ++ch)
        {
            for (int ap = 0; ap < kNumAllpasses; ++ap)
            {
                auto& state = v.allpass[ch][ap];
                std::fill(std::begin(state.buffer), std::end(state.buffer), 0.0f);
                state.writePos = 0;
                state.delaySamples = juce::jmax(1, std::min(AllpassState::kAllpassBufSize - 1, allpassDelays[ap]));
            }
        }
    }

    // M4: Bass mono crossover — 2-pole Butterworth LP at 320Hz
    //     Higher crossover keeps low-mids dry/mono for a tighter, cleaner result.
    {
        const float fc   = 320.0f;
        const float w0   = juce::MathConstants<float>::twoPi * fc / sr;
        const float cosW = std::cos(w0);
        const float sinW = std::sin(w0);
        const float alpha = sinW / (2.0f * 0.7071f);
        const float a0   = 1.0f + alpha;
        bassCrossB0 = ((1.0f - cosW) * 0.5f) / a0;
        bassCrossB1 = (1.0f - cosW) / a0;
        bassCrossB2 = bassCrossB0;
        bassCrossA1 = (-2.0f * cosW) / a0;
        bassCrossA2 = (1.0f - alpha) / a0;
        for (int ch = 0; ch < kMaxChannels; ++ch)
        {
            bassCrossZ1[ch] = 0.0f;
            bassCrossZ2[ch] = 0.0f;
        }
    }
}

void MultiplyEngine::setMix(float mix)
{
    if (std::abs(currentMix - mix) > 1.0e-6f)
    {
        currentMix = mix;
        mixSmoothed.setTargetValue(mix);
    }
}

void MultiplyEngine::setSpeed(float speed)
{
    if (std::abs(currentSpeed - speed) > 1.0e-6f)
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

void MultiplyEngine::process(juce::AudioBuffer<float>& buffer)
{
    juce::ScopedNoDenormals noDenormals;
    const int  numChannels = buffer.getNumChannels();
    const int  numSamples  = buffer.getNumSamples();
    const bool isStereo    = numChannels > 1;

    for (int i = 0; i < numSamples; ++i)
    {
        const float mix      = mixSmoothed.getNextValue();
        const float speedMul = speedSmoothed.getNextValue();

        // M4: Bass crossover storage per channel (bass stays dry/mono)
        float bassStore[kMaxChannels] = {};

        // Write both channels to delay buffer (raw treble, no feedback)
        for (int ch = 0; ch < numChannels; ++ch)
        {
            const float input = buffer.getReadPointer(ch)[i];

            // M4: Bass crossover — extract bass (stays dry/mono)
            const float bassLP = bassCrossB0 * input + bassCrossZ1[ch];
            bassCrossZ1[ch] = bassCrossB1 * input - bassCrossA1 * bassLP + bassCrossZ2[ch];
            bassCrossZ2[ch] = bassCrossB2 * input - bassCrossA2 * bassLP;
            bassStore[ch] = bassLP;
            const float treble = input - bassLP;

            // Write raw treble directly to delay buffer (no feedback injection)
            delayBuffer[static_cast<size_t>(ch)][static_cast<size_t>(writePos)] = treble;
        }

        // Always advance oscillators so phase stays continuous even when mix is zero
        for (int vi = 0; vi < kNumVoices; ++vi)
        {
            auto& v = voices[static_cast<size_t>(vi)];
            v.fmOsc[0].advance(speedMul);
            v.fmOsc[1].advance(speedMul);
        }

        // Renormalize oscillators periodically to prevent drift
        if (++sampleCounter >= kNormalizeInterval)
        {
            sampleCounter = 0;
            for (int vi = 0; vi < kNumVoices; ++vi)
            {
                auto& v = voices[static_cast<size_t>(vi)];
                v.fmOsc[0].normalize();
                v.fmOsc[1].normalize();
            }
        }

        if (mix < 0.001f)
        {
            writePos = (writePos + 1) & kDelayMask;
            continue;
        }

        float outL = 0.0f, outR = 0.0f;

        for (int vi = 0; vi < kNumVoices; ++vi)
        {
            auto& v = voices[static_cast<size_t>(vi)];

            // Smooth pan (per-sample, no zipper noise)
            constexpr float kPanSlew = 0.001f;
            v.pan += kPanSlew * (v.panTarget - v.pan);

            // Per-channel FM modulated delays for independent L/R movement
            // FM: delay = base + depth * (1 + sin(phase)), oscillates 0 -> 2*depth
            const float fmLfoL = v.fmOsc[0].sinVal;
            const float fmLfoR = v.fmOsc[1].sinVal;
            float delaySmpL = v.baseDelaySamples + v.fmDepthSamples * (1.0f + fmLfoL);
            float delaySmpR = v.baseDelaySamples + v.fmDepthSamples * (1.0f + fmLfoR);

            // Guard: ensure delay >= 1 sample to prevent reading at or ahead of write pointer
            delaySmpL = std::max(1.0f, delaySmpL);
            delaySmpR = std::max(1.0f, delaySmpR);

            // ITD modeling — very small pan-dependent micro-delay for spatial imaging.
            // Keep conservative so widening does not translate into a delayed sound.
            const float itdMs = 0.28f * std::abs(v.pan - 0.5f);  // 0ms at center, 0.14ms at edges
            v.itdDelaySamples = itdMs * 0.001f * static_cast<float>(sampleRate);

            // Read L and R with per-channel modulation + ITD
            float sampleL, sampleR;
            if (v.pan < 0.5f)
            {
                // Panned left: L is near ear (no ITD), R is far ear (+ ITD)
                sampleL = readHermite(0, delaySmpL);
                sampleR = isStereo ? readHermite(1, delaySmpR + v.itdDelaySamples) : sampleL;
            }
            else
            {
                // Panned right: R is near ear (no ITD), L is far ear (+ ITD)
                sampleL = readHermite(0, delaySmpL + v.itdDelaySamples);
                sampleR = isStereo ? readHermite(1, delaySmpR) : sampleL;
            }

            // Allpass diffusion chain for transparent thickening
            for (int ap = 0; ap < kNumAllpasses; ++ap)
            {
                sampleL = processAllpass(sampleL, v.allpass[0][ap]);
                if (isStereo)
                    sampleR = processAllpass(sampleR, v.allpass[1][ap]);
            }

            // Constant-power pan law avoids level dips and keeps center solidity.
            const float pan = juce::jlimit(0.0f, 1.0f, v.pan);
            const float angle = pan * juce::MathConstants<float>::halfPi;
            const float gL = std::cos(angle);
            const float gR = std::sin(angle);
            outL += sampleL * gL;
            outR += sampleR * gR;
        }

        // Keep low-mid mix settings drier for "on top of dry" behavior like Multiply.
        const float wetMix = juce::jlimit(0.0f, 1.0f, mix * 0.72f);
        const float dryGain = std::cos(wetMix * juce::MathConstants<float>::halfPi);
        const float wetGain = std::sin(wetMix * juce::MathConstants<float>::halfPi);

        // M4: Recombine bass (dry, mono-safe) with processed treble
        const float dryTrebleL = buffer.getReadPointer(0)[i] - bassStore[0];
        buffer.getWritePointer(0)[i] = bassStore[0] + dryTrebleL * dryGain + outL * wetGain;
        if (isStereo)
        {
            const float dryTrebleR = buffer.getReadPointer(1)[i] - bassStore[1];
            buffer.getWritePointer(1)[i] = bassStore[1] + dryTrebleR * dryGain + outR * wetGain;
        }

        writePos = (writePos + 1) & kDelayMask;
    }
}
