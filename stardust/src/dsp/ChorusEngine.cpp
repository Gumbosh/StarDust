#include "ChorusEngine.h"

void ChorusEngine::prepare(double newSampleRate, int /*samplesPerBlock*/)
{
    sampleRate = newSampleRate;
    mixSmoothed.reset(sampleRate, 0.02);

    delayBuffer0.fill(0.0f);
    delayBuffer1.fill(0.0f);
    writePos = 0;

    const float sr    = static_cast<float>(sampleRate);
    const float twoPi = juce::MathConstants<float>::twoPi;

    // MN3009 output reconstruction LP (~8 kHz)
    bbdLPCoeff = std::exp(-twoPi * 8000.0f / sr);

    // NE570 compander (M1): ~1ms attack, ~20ms release
    compAttack = 1.0f - std::exp(-twoPi * 1000.0f / sr);
    compRelease = 1.0f - std::exp(-twoPi * 50.0f / sr);
    compEnvState = 0.0f;
    expandEnvL = 0.0f;
    expandEnvR = 0.0f;

    // BBD leakage LP state (M2)
    for (int m = 0; m < kNumModes; ++m)
    {
        leakageLPStateL[m] = 0.0f;
        leakageLPStateR[m] = 0.0f;
    }

    // 4-pole Butterworth LP pre-filter at ~3.4 kHz (24dB/oct — CH2 upgrade)
    // Stage 1: Q = 0.5412 (4th-order Butterworth pole pair 1)
    {
        const float fc   = 3400.0f;
        const float w0   = twoPi * fc / sr;
        const float cosW = std::cos(w0);
        const float sinW = std::sin(w0);
        const float q    = 0.5412f;  // 4th-order Butterworth stage 1
        const float alph = sinW / (2.0f * q);
        const float a0   = 1.0f + alph;
        preB0 = ((1.0f - cosW) * 0.5f) / a0;
        preB1 = ( 1.0f - cosW)          / a0;
        preB2 = ((1.0f - cosW) * 0.5f) / a0;
        preA1 = (-2.0f * cosW)           / a0;
        preA2 = ( 1.0f - alph)           / a0;
    }
    hp1z1 = hp1z2 = 0.0f;

    // Stage 2: Q = 1.3066 (4th-order Butterworth pole pair 2)
    {
        const float fc   = 3400.0f;
        const float w0   = twoPi * fc / sr;
        const float cosW = std::cos(w0);
        const float sinW = std::sin(w0);
        const float q    = 1.3066f;  // 4th-order Butterworth stage 2
        const float alph = sinW / (2.0f * q);
        const float a0   = 1.0f + alph;
        pre2B0 = ((1.0f - cosW) * 0.5f) / a0;
        pre2B1 = ( 1.0f - cosW)          / a0;
        pre2B2 = ((1.0f - cosW) * 0.5f) / a0;
        pre2A1 = (-2.0f * cosW)           / a0;
        pre2A2 = ( 1.0f - alph)           / a0;
    }
    hp2z1 = hp2z2 = 0.0f;

    // BBD clock feedthrough oscillator (CH4): 50kHz typical BBD clock
    clockOsc.init(50000.0f, sr);
    clockRenormCount = 0;

    // BBD noise floor PRNG
    bbdNoisePrng = 98765u;

    // Hardware-accurate Juno-60 timing:
    //   Mode I:  0.513 Hz, ±1.85 ms depth, 3.5 ms centre (measured: 1.66–5.35 ms swing)
    //   Mode II: 0.863 Hz, ±2.5  ms depth, 3.5 ms centre
    const float lfoRates[kNumModes]  = { 0.513f, 0.863f };
    const float depthsMs[kNumModes]  = { 1.85f,  2.5f   };
    const float centreMs             = 3.5f;

    for (int m = 0; m < kNumModes; ++m)
    {
        auto& v = modes[static_cast<size_t>(m)];
        v.lfoRate      = lfoRates[m];
        v.depthSamples = depthsMs[m] * 0.001f * sr;
        v.baseSamples  = centreMs    * 0.001f * sr;
        // Both LFOs start at 0 — different rates (0.513 vs 0.863 Hz) naturally create
        // beating at ~0.35 Hz, producing I+II's characteristic complex wobble.
        // Anti-phase stagger was wrong: it caused depth cancellation in I+II.
        v.lfoPhase     = 0.0f;
        v.bbdLP[0]     = 0.0f;
        v.bbdLP[1]     = 0.0f;
        v.jitterL      = 0.0f;
        v.jitterR      = 0.0f;
        v.jitterPrng   = 12345u + static_cast<uint32_t>(m) * 6364136223846793005u;
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

void ChorusEngine::setMode(int mode)
{
    junoMode = juce::jlimit(0, 2, mode);
}

float ChorusEngine::readInterpolated(const float* buf, float delaySamples) const
{
    const float rp = static_cast<float>(writePos) - delaySamples;
    const int i1 = (static_cast<int>(std::floor(rp)) + kDelayBufferSize) & (kDelayBufferSize - 1);
    const int i0 = (i1 - 1 + kDelayBufferSize) & (kDelayBufferSize - 1);
    const int i2 = (i1 + 1) & (kDelayBufferSize - 1);
    const int i3 = (i1 + 2) & (kDelayBufferSize - 1);
    const float t = rp - std::floor(rp);

    const float p0 = buf[static_cast<size_t>(i0)];
    const float p1 = buf[static_cast<size_t>(i1)];
    const float p2 = buf[static_cast<size_t>(i2)];
    const float p3 = buf[static_cast<size_t>(i3)];

    const float a = 0.5f * (-p0 + 3.0f * p1 - 3.0f * p2 + p3);
    const float b = 0.5f * (2.0f * p0 - 5.0f * p1 + 4.0f * p2 - p3);
    const float c = 0.5f * (-p0 + p2);
    return ((a * t + b) * t + c) * t + p1;
}

void ChorusEngine::process(juce::AudioBuffer<float>& buffer)
{
    juce::ScopedNoDenormals noDenormals;
    const int  numChannels = buffer.getNumChannels();
    const int  numSamples  = buffer.getNumSamples();
    const bool isStereo    = numChannels > 1;
    const float lfoInc     = 1.0f / static_cast<float>(sampleRate);
    const float twoPi      = juce::MathConstants<float>::twoPi;

    const bool active[kNumModes] = { junoMode == 0 || junoMode == 2,
                                     junoMode == 1 || junoMode == 2 };
    // -3 dB when both modes active (hardware summing attenuation)
    const float wetNorm = (junoMode == 2) ? 0.7071f : 1.0f;

    for (int i = 0; i < numSamples; ++i)
    {
        const float mix = mixSmoothed.getNextValue();

        const float dryL = buffer.getReadPointer(0)[i];
        const float dryR = isStereo ? buffer.getReadPointer(1)[i] : dryL;

        // Mono-sum input (single BBD path, stereo comes from LFO inversion only)
        float monoIn = isStereo ? (dryL + dryR) * 0.5f : dryL;

        // 4-pole LP pre-filter (24dB/oct, ~3.4 kHz) — CH2: cascaded biquad stages
        // Stage 1 (Q = 0.5412)
        {
            const float y = preB0 * monoIn + hp1z1;
            hp1z1 = preB1 * monoIn - preA1 * y + hp1z2;
            hp1z2 = preB2 * monoIn - preA2 * y;
            monoIn = y;
        }
        // Stage 2 (Q = 1.3066)
        {
            const float y = pre2B0 * monoIn + hp2z1;
            hp2z1 = pre2B1 * monoIn - pre2A1 * y + hp2z2;
            hp2z2 = pre2B2 * monoIn - pre2A2 * y;
            monoIn = y;
        }

        // NE570 compander: compress before BBD (M1)
        // Envelope follower for compression
        {
            const float absIn = std::abs(monoIn);
            const float envAlpha = (absIn > compEnvState) ? compAttack : compRelease;
            compEnvState += envAlpha * (absIn - compEnvState);
            // 2:1 compression ratio (reduce dynamic range going into BBD)
            const float compGain = 1.0f / (1.0f + compEnvState * 0.5f);

            // CH6: NE570 VCA nonlinearity — 2nd-order distortion proportional to compression depth
            const float vcaDist = 0.001f * compGain * compGain;
            monoIn += vcaDist * monoIn * monoIn;

            monoIn *= compGain;
        }

        // BBD input OTA soft saturation (gentle, preserves unity small-signal gain)
        monoIn = fastTanh(monoIn * 1.15f) * 0.8697f;

        // Write to delay buffers (CH5: separate buffers for I+II mode)
        delayBuffer0[static_cast<size_t>(writePos)] = monoIn;
        if (junoMode == 2)
            delayBuffer1[static_cast<size_t>(writePos)] = monoIn;

        if (mix < 0.001f)
        {
            writePos = (writePos + 1) & (kDelayBufferSize - 1);
            // Advance clock oscillator even when bypassed (CH4)
            clockOsc.advance();
            if (++clockRenormCount >= 512)
            {
                clockRenormCount = 0;
                clockOsc.normalize();
            }
            continue;
        }

        float outL = 0.0f, outR = 0.0f;

        for (int m = 0; m < kNumModes; ++m)
        {
            if (!active[m]) continue;
            auto& v = modes[static_cast<size_t>(m)];

            // BBD clock jitter: LP-filtered random walk (correlated drift, not white noise).
            // Peak ±0.04 samples — real MN3009 clock variation is ~0.1% period, barely audible.
            // Filter alpha ~0.001 ≈ 7 Hz LP at 44.1kHz — slow drift like an analog RC oscillator.
            static constexpr float kJitterAlpha = 0.001f;
            static constexpr float kJitterScale = 0.04f / 2147483648.0f;
            v.jitterPrng = v.jitterPrng * 1664525u + 1013904223u;
            v.jitterL += kJitterAlpha * (static_cast<float>(static_cast<int32_t>(v.jitterPrng)) * kJitterScale - v.jitterL);
            v.jitterPrng = v.jitterPrng * 1664525u + 1013904223u;
            v.jitterR += kJitterAlpha * (static_cast<float>(static_cast<int32_t>(v.jitterPrng)) * kJitterScale - v.jitterR);
            const float jL = v.jitterL;
            const float jR = v.jitterR;

            const float lfo = triLFO(v.lfoPhase);

            // Stereo topology:
            //   Modes I and II standalone: R = inverted LFO (standard Juno stereo widening)
            //   I+II combined, Mode II: R = non-inverted (creates Leslie-style swirl)
            const bool modeIIinCombined = (junoMode == 2 && m == 1);
            float delayL = std::max(1.0f, v.baseSamples + lfo * v.depthSamples + jL);
            float delayR = std::max(1.0f, modeIIinCombined
                                          ? v.baseSamples + lfo * v.depthSamples + jR   // non-inverted
                                          : v.baseSamples - lfo * v.depthSamples + jR); // inverted

            // CH1: BBD bucket discretization — quantize delay to 512 discrete stages
            {
                const float bucketSize = (v.baseSamples + v.depthSamples) / static_cast<float>(kBBDBuckets);
                delayL = std::round(delayL / bucketSize) * bucketSize;
                delayR = std::round(delayR / bucketSize) * bucketSize;
            }

            // CH5: Select buffer based on mode — Mode II uses delayBuffer1 in I+II mode
            const float* readBuf = (junoMode == 2 && m == 1)
                                   ? delayBuffer1.data()
                                   : delayBuffer0.data();

            float sL = readInterpolated(readBuf, delayL);
            float sR = readInterpolated(readBuf, delayR);

            // CH3: BBD even-harmonic distortion — subtle 2nd harmonic from charge transfer
            static constexpr float kBbdDistortion = 0.005f;
            sL += kBbdDistortion * sL * sL;
            sR += kBbdDistortion * sR * sR;

            // BBD bucket leakage (M2): delay-dependent LP + per-bucket charge loss
            {
                const float avgDelay = (delayL + delayR) * 0.5f;
                // Number of active buckets (proportional to delay)
                const float activeBuckets = avgDelay / ((v.baseSamples + v.depthSamples) / static_cast<float>(kBBDBuckets));
                // Per-bucket loss: ~0.01% per stage, cumulative
                const float chargeLoss = std::exp(-activeBuckets * 0.0001f); // subtle cumulative attenuation
                sL *= chargeLoss;
                sR *= chargeLoss;

                // Original delay-dependent LP (cutoff decreases with longer delay)
                const float leakCutoff = 20000.0f - avgDelay * 25.0f;
                const float clampedCut = std::max(8000.0f, leakCutoff);
                const float leakCoeff = std::exp(-juce::MathConstants<float>::twoPi * clampedCut
                    / static_cast<float>(sampleRate));
                leakageLPStateL[m] = sL * (1.0f - leakCoeff) + leakageLPStateL[m] * leakCoeff;
                sL = leakageLPStateL[m];
                leakageLPStateR[m] = sR * (1.0f - leakCoeff) + leakageLPStateR[m] * leakCoeff;
                sR = leakageLPStateR[m];
            }

            // MN3009 output LP (~8 kHz capacitor rolloff)
            sL = sL * (1.0f - bbdLPCoeff) + v.bbdLP[0] * bbdLPCoeff;
            v.bbdLP[0] = sL;
            sR = sR * (1.0f - bbdLPCoeff) + v.bbdLP[1] * bbdLPCoeff;
            v.bbdLP[1] = sR;

            // CH4: BBD clock feedthrough — faint clock tone at instantaneous BBD rate
            {
                const float clockLevel = 0.001f; // -60dB
                const float clockNoise = clockOsc.sinVal * clockLevel;
                sL += clockNoise;
                sR += clockNoise;
            }

            // NE570 expander: expand after BBD to restore dynamics (M1)
            // Uses per-channel envelope tracking the wet signal
            {
                const float envAlphaL = (std::abs(sL) > expandEnvL) ? compAttack : compRelease;
                expandEnvL += envAlphaL * (std::abs(sL) - expandEnvL);
                sL *= (1.0f + expandEnvL * 0.5f); // inverse of 2:1 compression

                const float envAlphaR = (std::abs(sR) > expandEnvR) ? compAttack : compRelease;
                expandEnvR += envAlphaR * (std::abs(sR) - expandEnvR);
                sR *= (1.0f + expandEnvR * 0.5f);
            }

            // BBD noise floor: ~-70dB shaped noise (real MN3009 characteristic)
            {
                bbdNoisePrng = bbdNoisePrng * 1664525u + 1013904223u;
                const float noise = static_cast<float>(static_cast<int32_t>(bbdNoisePrng)) * (1.0f / 2147483648.0f);
                const float noiseLevel = 0.0003f; // ~-70dB
                sL += noise * noiseLevel;
                bbdNoisePrng = bbdNoisePrng * 1664525u + 1013904223u;
                const float noise2 = static_cast<float>(static_cast<int32_t>(bbdNoisePrng)) * (1.0f / 2147483648.0f);
                sR += noise2 * noiseLevel;
            }

            outL += sL;
            outR += sR;

            v.lfoPhase += twoPi * v.lfoRate * lfoInc;
            if (v.lfoPhase >= twoPi) v.lfoPhase -= twoPi;
        }

        // I+II normalisation (-3 dB)
        outL *= wetNorm;
        outR *= wetNorm;

        // Constant-power dry/wet crossfade
        const float dryGain = std::cos(mix * juce::MathConstants<float>::halfPi);
        const float wetGain = std::sin(mix * juce::MathConstants<float>::halfPi);

        buffer.getWritePointer(0)[i] = dryL * dryGain + outL * wetGain;
        if (isStereo)
            buffer.getWritePointer(1)[i] = dryR * dryGain + outR * wetGain;

        writePos = (writePos + 1) & (kDelayBufferSize - 1);

        // Advance clock oscillator once per sample (CH4)
        clockOsc.advance();
        if (++clockRenormCount >= 512)
        {
            clockRenormCount = 0;
            clockOsc.normalize();
        }
    }
}
