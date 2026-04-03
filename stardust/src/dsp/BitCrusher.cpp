#include "BitCrusher.h"

void BitCrusher::prepare(double sampleRate, int /*samplesPerBlock*/)
{
    hostSampleRate = sampleRate;
    bitDepthSmoothed.reset(sampleRate, 0.005);
    targetRateSmoothed.reset(sampleRate, 0.005);

    for (int ch = 0; ch < kMaxChannels; ++ch)
    {
        holdCounter[ch] = 0.0f;
        holdSample[ch] = 0.0f;
        prevBassReduced[ch] = 0.0f;
        preAaZ1[ch] = 0.0f;
        preAaZ2[ch] = 0.0f;
        preAaZ1b[ch] = 0.0f;
        preAaZ2b[ch] = 0.0f;
        nyquistZ1[ch] = 0.0f;
        nyquistZ2[ch] = 0.0f;
        nyquistZ1b[ch] = 0.0f;
        nyquistZ2b[ch] = 0.0f;
        ditherState[ch]   = 0xDEAD0001u + static_cast<uint32_t>(ch);
        apertureState[ch] = 0xBEEF0001u + static_cast<uint32_t>(ch);
        dcBlockX1[ch] = 0.0f;
        dcBlockY1[ch] = 0.0f;
    }
    preAaB0 = 1.0f; preAaB1 = 0.0f; preAaB2 = 0.0f; preAaA1 = 0.0f; preAaA2 = 0.0f;
    preAaB0b = 1.0f; preAaB1b = 0.0f; preAaB2b = 0.0f; preAaA1b = 0.0f; preAaA2b = 0.0f;
    nyqB0b = 1.0f; nyqB1b = 0.0f; nyqB2b = 0.0f; nyqA1b = 0.0f; nyqA2b = 0.0f;
    oscRenormCount = 0;

    // DC blocker: 1-pole HP at ~5Hz. exp() form matches Saturation's dcCoeff calculation
    // and avoids the first-order approximation error of (1 - 2πf/sr) at low sample rates.
    dcBlockCoeff = std::exp(-(juce::MathConstants<float>::twoPi * 5.0f / static_cast<float>(sampleRate)));

    // Incremental jitter oscillators — no sin() per sample
    {
        const float twoPiOverSr = juce::MathConstants<float>::twoPi / static_cast<float>(sampleRate);
        const auto computeRot = [&](float freq, float& rc, float& rs) {
            const float angle = twoPiOverSr * freq;
            rc = std::cos(angle);
            rs = std::sin(angle);
        };
        computeRot(7.0f, jitterRotCosA, jitterRotSinA);   // 7Hz PSU-coupled
        computeRot(0.3f, jitterRotCosB, jitterRotSinB);   // 0.3Hz thermal drift
    }
    jitterCosA = 1.0f; jitterSinA = 0.0f;
    jitterCosB = 1.0f; jitterSinB = 0.0f;
    jitterWalkLP = 0.0f;
    jitterState = 12345u;

    // Frequency-dependent bit-crushing: low-shelf pair at 200Hz (C10 vintage character)
    // Pre-shelf: -3dB low shelf raises quantization noise floor in bass relative to signal.
    // Post-shelf: +3dB restores signal; noise elevation stays (intentional vintage texture).
    {
        const float pi = juce::MathConstants<float>::pi;
        const float sr = static_cast<float>(sampleRate);
        const float w0 = 2.0f * pi * 200.0f / sr;
        const float cosW0 = std::cos(w0);
        const float sinW0 = std::sin(w0);
        // Shelf slope S=1: alpha = sin(w0)/2 * sqrt(2)
        const float alphaSqrt2 = sinW0 * 0.7071f;
        {
            const float A = 0.8414f;  // 10^(-3/40) — -3dB shelf gain
            const float sqrtA = std::sqrt(A);
            const float a0r = 1.0f / ((A + 1.0f) + (A - 1.0f) * cosW0 + 2.0f * sqrtA * alphaSqrt2);
            bassPreB0 = A * ((A + 1.0f) - (A - 1.0f) * cosW0 + 2.0f * sqrtA * alphaSqrt2) * a0r;
            bassPreB1 = 2.0f * A * ((A - 1.0f) - (A + 1.0f) * cosW0) * a0r;
            bassPreB2 = A * ((A + 1.0f) - (A - 1.0f) * cosW0 - 2.0f * sqrtA * alphaSqrt2) * a0r;
            bassPreA1 = -2.0f * ((A - 1.0f) + (A + 1.0f) * cosW0) * a0r;
            bassPreA2 = ((A + 1.0f) + (A - 1.0f) * cosW0 - 2.0f * sqrtA * alphaSqrt2) * a0r;
        }
        {
            const float A = 1.1885f;  // 10^(+3/40) — +3dB shelf gain
            const float sqrtA = std::sqrt(A);
            const float a0r = 1.0f / ((A + 1.0f) + (A - 1.0f) * cosW0 + 2.0f * sqrtA * alphaSqrt2);
            bassPostB0 = A * ((A + 1.0f) - (A - 1.0f) * cosW0 + 2.0f * sqrtA * alphaSqrt2) * a0r;
            bassPostB1 = 2.0f * A * ((A - 1.0f) - (A + 1.0f) * cosW0) * a0r;
            bassPostB2 = A * ((A + 1.0f) - (A - 1.0f) * cosW0 - 2.0f * sqrtA * alphaSqrt2) * a0r;
            bassPostA1 = -2.0f * ((A - 1.0f) + (A + 1.0f) * cosW0) * a0r;
            bassPostA2 = ((A + 1.0f) + (A - 1.0f) * cosW0 - 2.0f * sqrtA * alphaSqrt2) * a0r;
        }
        for (int ch = 0; ch < kMaxChannels; ++ch)
            bassPreZ1[ch] = bassPreZ2[ch] = bassPostZ1[ch] = bassPostZ2[ch] = 0.0f;
    }
}

void BitCrusher::setBitDepth(float bits)
{
    bits = juce::jlimit(1.0f, 32.0f, bits);
    if (currentBitDepth != bits)
    {
        currentBitDepth = bits;
        bitDepthSmoothed.setTargetValue(bits);
    }
}

void BitCrusher::setSampleRate(float targetRate)
{
    if (currentTargetRate != targetRate)
    {
        currentTargetRate = targetRate;
        targetRateSmoothed.setTargetValue(targetRate);
    }
}


void BitCrusher::process(juce::AudioBuffer<float>& buffer)
{
    juce::ScopedNoDenormals noDenormals;
    const auto numChannels = std::min(buffer.getNumChannels(), kMaxChannels);
    const auto numSamples = buffer.getNumSamples();

    // Coefficient recomputation: unconditionally once per block using the current smoothed rate.
    // The old 1e-3 normalized-cutoff threshold caused 44Hz step artifacts when the sample-rate
    // fader swept slowly through low target rates (e.g. 250–2000 Hz), where a single step
    // represents a large fraction of the filter's passband. Block-rate trig calls are negligible.
    // 4th-order Butterworth requires two stages with staggered Q values:
    //   Q₁ = 1/(2·sin(π/8)) ≈ 0.5412  (wider, goes first)
    //   Q₂ = 1/(2·sin(3π/8)) ≈ 1.3066 (sharper knee near cutoff, goes second)
    {
        const float blockTargetRate = targetRateSmoothed.getCurrentValue();

        // Pre-S&H anti-alias: 4-pole Butterworth LP at targetRate/2
        {
            const float normalizedCutoff = juce::jlimit(0.001f, 0.49f,
                blockTargetRate * 0.5f / static_cast<float>(hostSampleRate));
            const float w0    = juce::MathConstants<float>::twoPi * normalizedCutoff;
            const float cosW0 = std::cos(w0);
            const float sinW0 = std::sin(w0);
            {   // Stage 1: Q = 0.5412
                const float alpha = sinW0 / (2.0f * 0.5412f);
                const float a0inv = 1.0f / (1.0f + alpha);
                preAaB0 = ((1.0f - cosW0) * 0.5f) * a0inv;
                preAaB1 = (1.0f - cosW0) * a0inv;
                preAaB2 = preAaB0;
                preAaA1 = (-2.0f * cosW0) * a0inv;
                preAaA2 = (1.0f - alpha) * a0inv;
            }
            {   // Stage 2: Q = 1.3066
                const float alpha = sinW0 / (2.0f * 1.3066f);
                const float a0inv = 1.0f / (1.0f + alpha);
                preAaB0b = ((1.0f - cosW0) * 0.5f) * a0inv;
                preAaB1b = (1.0f - cosW0) * a0inv;
                preAaB2b = preAaB0b;
                preAaA1b = (-2.0f * cosW0) * a0inv;
                preAaA2b = (1.0f - alpha) * a0inv;
            }
        }

        // Nyquist ceiling: 4-pole Butterworth LP at targetRate * 0.45
        {
            const float nyqNormalized = juce::jlimit(0.006f, 0.49f,
                blockTargetRate * 0.45f / static_cast<float>(hostSampleRate));
            const float w0    = juce::MathConstants<float>::twoPi * nyqNormalized;
            const float cosW0 = std::cos(w0);
            const float sinW0 = std::sin(w0);
            {   // Stage 1: Q = 0.5412
                const float alpha = sinW0 / (2.0f * 0.5412f);
                const float a0inv = 1.0f / (1.0f + alpha);
                nyqB0 = ((1.0f - cosW0) * 0.5f) * a0inv;
                nyqB1 = (1.0f - cosW0) * a0inv;
                nyqB2 = nyqB0;
                nyqA1 = (-2.0f * cosW0) * a0inv;
                nyqA2 = (1.0f - alpha) * a0inv;
            }
            {   // Stage 2: Q = 1.3066
                const float alpha = sinW0 / (2.0f * 1.3066f);
                const float a0inv = 1.0f / (1.0f + alpha);
                nyqB0b = ((1.0f - cosW0) * 0.5f) * a0inv;
                nyqB1b = (1.0f - cosW0) * a0inv;
                nyqB2b = nyqB0b;
                nyqA1b = (-2.0f * cosW0) * a0inv;
                nyqA2b = (1.0f - alpha) * a0inv;
            }
        }
    }

    // levels changes at most ~0.002/sample on a 5ms ramp — computing once per block
    // from getCurrentValue() is inaudible (< 0.2 LSB error at 12-bit) and avoids
    // an exp2f() call per sample. skip() advances the smoother by the full block so
    // the next block's getCurrentValue() lands at the correct ramp position.
    const float blockLevels = std::exp2f(bitDepthSmoothed.getCurrentValue());
    bitDepthSmoothed.skip(numSamples);

    for (int i = 0; i < numSamples; ++i)
    {
        const float targetRate = targetRateSmoothed.getNextValue();
        const float levels     = blockLevels;
        const float ratio      = targetRate / static_cast<float>(hostSampleRate);

        // ADC clock jitter — incremental unit-circle rotation (no sin() per sample).
        // Osc A (7Hz) + Osc B (0.3Hz) + LP-filtered random walk model crystal oscillator noise.
        float jitteredRatio = ratio;
        if (jitterAmount > 0.0f)
        {
            {
                const float c = jitterCosA * jitterRotCosA - jitterSinA * jitterRotSinA;
                const float s = jitterSinA * jitterRotCosA + jitterCosA * jitterRotSinA;
                jitterCosA = c; jitterSinA = s;
            }
            {
                const float c = jitterCosB * jitterRotCosB - jitterSinB * jitterRotSinB;
                const float s = jitterSinB * jitterRotCosB + jitterCosB * jitterRotSinB;
                jitterCosB = c; jitterSinB = s;
            }
            jitterState = jitterState * 1664525u + 1013904223u;
            const float noise = static_cast<float>(static_cast<int32_t>(jitterState))
                                * (1.0f / 2147483648.0f);
            jitterWalkLP += kJitterWalkAlpha * (noise - jitterWalkLP);

            const float jitterMod = jitterWalkLP * 0.6f + jitterSinA * 0.3f + jitterSinB * 0.1f;
            jitteredRatio = ratio * (1.0f + jitterMod * jitterAmount * 0.008f);

            // Renormalize unit-circle oscillators every 8192 samples to prevent radius drift
            if ((++oscRenormCount & 0x1FFF) == 0)
            {
                const float normA = 1.0f / std::sqrt(jitterCosA * jitterCosA + jitterSinA * jitterSinA);
                jitterCosA *= normA; jitterSinA *= normA;
                const float normB = 1.0f / std::sqrt(jitterCosB * jitterCosB + jitterSinB * jitterSinB);
                jitterCosB *= normB; jitterSinB *= normB;
            }
        }

        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto* data = buffer.getWritePointer(ch);
            const float sample = data[i];

            // 4th-order Butterworth pre-S&H anti-alias (2 cascaded biquad stages, ~24dB/oct)
            // Stage 1: Q=0.5412 — stage 2: Q=1.3066 (true staggered-pole Butterworth)
            const float aa1 = preAaB0 * sample + preAaZ1[ch];
            preAaZ1[ch] = preAaB1 * sample - preAaA1 * aa1 + preAaZ2[ch];
            preAaZ2[ch] = preAaB2 * sample - preAaA2 * aa1;
            const float filtered = preAaB0b * aa1 + preAaZ1b[ch];
            preAaZ1b[ch] = preAaB1b * aa1 - preAaA1b * filtered + preAaZ2b[ch];
            preAaZ2b[ch] = preAaB2b * aa1 - preAaA2b * filtered;

            // C10 bass shelf: -3dB pre-quantization.
            // Net effect: quantization noise in bass is elevated relative to signal level.
            const float bassReduced = bassPreB0 * filtered + bassPreZ1[ch];
            bassPreZ1[ch] = bassPreB1 * filtered - bassPreA1 * bassReduced + bassPreZ2[ch];
            bassPreZ2[ch] = bassPreB2 * filtered - bassPreA2 * bassReduced;

            // Sample-and-hold with linear interpolation at the sampling instant.
            // Linear (vs. Catmull-Rom) preserves the staircase character at low rate settings.
            // apertureState is independent from ditherState so the S&H timing jitter and the
            // TPDF quantisation dither are drawn from entirely separate LCG sequences.
            apertureState[ch] = apertureState[ch] * 1664525u + 1013904223u;
            const float apertureNoise = static_cast<float>(static_cast<int32_t>(apertureState[ch]))
                                        * (1.0f / 2147483648.0f);
            holdCounter[ch] += jitteredRatio + apertureNoise * jitterAmount * 0.05f;
            if (holdCounter[ch] >= 1.0f)
            {
                holdCounter[ch] -= 1.0f;
                const float t = holdCounter[ch];  // fractional overshoot = sub-sample position
                holdSample[ch] = prevBassReduced[ch] + t * (bassReduced - prevBassReduced[ch]);
            }
            prevBassReduced[ch] = bassReduced;

            // TPDF dither — one triangular noise pair per sample, no noise shaping.
            // Keeps quantization noise spectrally flat, matching vintage hardware character.
            const float halfStep = 0.5f / levels;
            ditherState[ch] = ditherState[ch] * 1664525u + 1013904223u;
            const float r1 = static_cast<float>(static_cast<int32_t>(ditherState[ch]))
                             * (1.0f / 2147483648.0f);
            ditherState[ch] = ditherState[ch] * 1664525u + 1013904223u;
            const float r2 = static_cast<float>(static_cast<int32_t>(ditherState[ch]))
                             * (1.0f / 2147483648.0f);
            const float dithered = holdSample[ch] + (r1 - r2) * halfStep;
            const float quantized = juce::jlimit(-1.0f, 1.0f,
                (levels <= 2.0f)
                    ? (dithered >= 0.0f ? 0.5f : -0.5f)
                    : std::floor(dithered * levels + 0.5f) / levels);

            // 4th-order Butterworth Nyquist ceiling (2 cascaded biquad stages)
            // Stage 1: Q=0.5412 — stage 2: Q=1.3066 (true staggered-pole Butterworth)
            const float out1 = nyqB0 * quantized + nyquistZ1[ch];
            nyquistZ1[ch] = nyqB1 * quantized - nyqA1 * out1 + nyquistZ2[ch];
            nyquistZ2[ch] = nyqB2 * quantized - nyqA2 * out1;
            const float out2 = nyqB0b * out1 + nyquistZ1b[ch];
            nyquistZ1b[ch] = nyqB1b * out1 - nyqA1b * out2 + nyquistZ2b[ch];
            nyquistZ2b[ch] = nyqB2b * out1 - nyqA2b * out2;

            // C10 bass restoration: +3dB post-quantization. Signal restored; noise stays elevated.
            const float postOut = bassPostB0 * out2 + bassPostZ1[ch];
            bassPostZ1[ch] = bassPostB1 * out2 - bassPostA1 * postOut + bassPostZ2[ch];
            bassPostZ2[ch] = bassPostB2 * out2 - bassPostA2 * postOut;

            // DC blocker (1-pole HP at ~5Hz) — prevents DC from accumulating at low bit depths
            const float dcOut = postOut - dcBlockX1[ch] + dcBlockCoeff * dcBlockY1[ch];
            dcBlockX1[ch] = postOut;
            dcBlockY1[ch] = dcOut;

            data[i] = dcOut;
        }
    }
}
