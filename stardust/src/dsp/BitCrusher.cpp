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
        zohShelfZ1[ch] = 0.0f;
        ditherState[ch]   = 0xDEAD0001u + static_cast<uint32_t>(ch);
        apertureState[ch] = 0xBEEF0001u + static_cast<uint32_t>(ch);
        dcBlockX1[ch] = 0.0f;
        dcBlockY1[ch] = 0.0f;
        noiseShapeE1[ch] = 0.0f;
        noiseShapeE2[ch] = 0.0f;
        noiseShapeE3[ch] = 0.0f;
    }
    preAaB0 = 1.0f; preAaB1 = 0.0f; preAaB2 = 0.0f; preAaA1 = 0.0f; preAaA2 = 0.0f;
    preAaB0b = 1.0f; preAaB1b = 0.0f; preAaB2b = 0.0f; preAaA1b = 0.0f; preAaA2b = 0.0f;
    nyqB0b = 1.0f; nyqB1b = 0.0f; nyqB2b = 0.0f; nyqA1b = 0.0f; nyqA2b = 0.0f;
    oscRenormCount = 0;
    lastPreAaCutoff = -1.0f;
    lastNyqCutoff = -1.0f;
    jitterPinkState = 0.0f;

    // DC blocker: 1-pole HP at ~5Hz
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

    // C3: Per-channel jitter state for stereo decorrelation
    jitterCosA_ch1 = 1.0f; jitterSinA_ch1 = 0.0f;
    jitterCosB_ch1 = 1.0f; jitterSinB_ch1 = 0.0f;
    jitterWalkLP_ch1 = 0.0f;
    jitterState_ch1 = 54321u;
    jitterPinkState_ch1 = 0.0f;

    // C2: 4th-order Bessel reconstruction filter at ~16kHz
    {
        const float fc = 16000.0f;
        const float normCut = juce::jlimit(0.001f, 0.49f, fc / static_cast<float>(sampleRate));
        const float w0 = juce::MathConstants<float>::twoPi * normCut;
        const float cosW = std::cos(w0);
        const float sinW = std::sin(w0);
        // Stage 1: Bessel Q = 0.5219
        {
            const float alpha = sinW / (2.0f * 0.5219f);
            const float a0inv = 1.0f / (1.0f + alpha);
            bessB0a = ((1.0f - cosW) * 0.5f) * a0inv;
            bessB1a = (1.0f - cosW) * a0inv;
            bessB2a = bessB0a;
            bessA1a = (-2.0f * cosW) * a0inv;
            bessA2a = (1.0f - alpha) * a0inv;
        }
        // Stage 2: Bessel Q = 0.8055
        {
            const float alpha = sinW / (2.0f * 0.8055f);
            const float a0inv = 1.0f / (1.0f + alpha);
            bessB0b = ((1.0f - cosW) * 0.5f) * a0inv;
            bessB1b = (1.0f - cosW) * a0inv;
            bessB2b = bessB0b;
            bessA1b = (-2.0f * cosW) * a0inv;
            bessA2b = (1.0f - alpha) * a0inv;
        }
        for (int ch = 0; ch < kMaxChannels; ++ch) {
            besselZ1a[ch] = besselZ2a[ch] = 0.0f;
            besselZ1b[ch] = besselZ2b[ch] = 0.0f;
        }
    }

    // Frequency-dependent bit-crushing: low-shelf pair at 200Hz (C10 vintage character)
    {
        const float pi = juce::MathConstants<float>::pi;
        const float sr = static_cast<float>(sampleRate);
        const float w0 = 2.0f * pi * 200.0f / sr;
        const float cosW0 = std::cos(w0);
        const float sinW0 = std::sin(w0);
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

// Helper: compute 4-pole Butterworth LP biquad coefficients for a given normalized cutoff
static void computeButterworthLP4(float normalizedCutoff,
                                   float& b0a, float& b1a, float& b2a, float& a1a, float& a2a,
                                   float& b0b, float& b1b, float& b2b, float& a1b, float& a2b)
{
    const float w0    = juce::MathConstants<float>::twoPi * normalizedCutoff;
    const float cosW0 = std::cos(w0);
    const float sinW0 = std::sin(w0);
    {   // Stage 1: Q = 0.5412
        const float alpha = sinW0 / (2.0f * 0.5412f);
        const float a0inv = 1.0f / (1.0f + alpha);
        b0a = ((1.0f - cosW0) * 0.5f) * a0inv;
        b1a = (1.0f - cosW0) * a0inv;
        b2a = b0a;
        a1a = (-2.0f * cosW0) * a0inv;
        a2a = (1.0f - alpha) * a0inv;
    }
    {   // Stage 2: Q = 1.3066
        const float alpha = sinW0 / (2.0f * 1.3066f);
        const float a0inv = 1.0f / (1.0f + alpha);
        b0b = ((1.0f - cosW0) * 0.5f) * a0inv;
        b1b = (1.0f - cosW0) * a0inv;
        b2b = b0b;
        a1b = (-2.0f * cosW0) * a0inv;
        a2b = (1.0f - alpha) * a0inv;
    }
}

void BitCrusher::process(juce::AudioBuffer<float>& buffer)
{
    juce::ScopedNoDenormals noDenormals;
    const auto numChannels = std::min(buffer.getNumChannels(), kMaxChannels);
    const auto numSamples = buffer.getNumSamples();
    const float sr = static_cast<float>(hostSampleRate);

    // Per-16-sample cached bit depth (exp2 is expensive — amortize over 16 samples)
    float cachedLevels = std::exp2f(bitDepthSmoothed.getCurrentValue());
    int bitUpdateCounter = 0;

    // Determine if target rate is being modulated (D6: per-sample coeffs when sweeping)
    const bool rateIsSmoothing = targetRateSmoothed.isSmoothing();

    // Block-rate coefficient computation when rate is stable
    if (!rateIsSmoothing)
    {
        const float blockTargetRate = targetRateSmoothed.getCurrentValue();

        // Pre-S&H anti-alias
        const float preAaCut = juce::jlimit(0.001f, 0.49f, blockTargetRate * 0.5f / sr);
        if (std::abs(preAaCut - lastPreAaCutoff) > 0.0001f)
        {
            lastPreAaCutoff = preAaCut;
            computeButterworthLP4(preAaCut, preAaB0, preAaB1, preAaB2, preAaA1, preAaA2,
                                  preAaB0b, preAaB1b, preAaB2b, preAaA1b, preAaA2b);
        }

        // Nyquist ceiling
        const float nyqCut = juce::jlimit(0.006f, 0.49f, blockTargetRate * 0.45f / sr);
        if (std::abs(nyqCut - lastNyqCutoff) > 0.0001f)
        {
            lastNyqCutoff = nyqCut;
            computeButterworthLP4(nyqCut, nyqB0, nyqB1, nyqB2, nyqA1, nyqA2,
                                  nyqB0b, nyqB1b, nyqB2b, nyqA1b, nyqA2b);
        }

        // ZOH droop compensation: 1st-order high shelf at targetRate/2 with ~+3.9dB (D2)
        // sinc(0.5) = 2/pi ≈ -3.9dB; compensate with a shelf
        {
            const float shelfFreq = blockTargetRate * 0.5f;
            const float wShelf = juce::MathConstants<float>::twoPi * shelfFreq / sr;
            // 1st-order allpass-based shelf: gain A at HF, unity at DC
            const float A = 1.247f; // ~+1.9dB (half the droop — full compensation sounds too bright)
            const float t = std::tan(wShelf * 0.5f);
            const float norm = 1.0f / (1.0f + t);
            zohShelfB0 = (A + t) * norm;
            zohShelfB1 = (t - A) * norm;
            zohShelfA1 = (t - 1.0f) * norm;
        }
    }

    for (int i = 0; i < numSamples; ++i)
    {
        const float targetRate = targetRateSmoothed.getNextValue();
        const float currentBits = bitDepthSmoothed.getNextValue();
        if (++bitUpdateCounter >= 16) {
            bitUpdateCounter = 0;
            cachedLevels = std::exp2f(currentBits);
        }
        const float levels     = cachedLevels;
        const float ratio      = targetRate / sr;

        // Per-sample coefficient update when rate is modulated (D6)
        if (rateIsSmoothing)
        {
            const float preAaCut = juce::jlimit(0.001f, 0.49f, targetRate * 0.5f / sr);
            computeButterworthLP4(preAaCut, preAaB0, preAaB1, preAaB2, preAaA1, preAaA2,
                                  preAaB0b, preAaB1b, preAaB2b, preAaA1b, preAaA2b);

            const float nyqCut = juce::jlimit(0.006f, 0.49f, targetRate * 0.45f / sr);
            computeButterworthLP4(nyqCut, nyqB0, nyqB1, nyqB2, nyqA1, nyqA2,
                                  nyqB0b, nyqB1b, nyqB2b, nyqA1b, nyqA2b);
        }

        // ADC clock jitter — incremental unit-circle rotation + 1/f pink noise component (D5)
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

            // Pink noise component (1/f) via Paul Kellet 1-pole (D5)
            jitterPinkState = 0.99765f * jitterPinkState + noise * 0.09905f;
            const float pinkNoise = (jitterPinkState + noise * 0.0555f) * 0.65f;

            const float jitterMod = jitterWalkLP * 0.4f + jitterSinA * 0.2f
                                  + jitterSinB * 0.1f + pinkNoise * 0.3f;
            jitteredRatio = ratio * (1.0f + jitterMod * jitterAmount * 0.008f);

            // Renormalize every 1024 samples (D7: was 8192, too infrequent)
            if ((++oscRenormCount & 0x3FF) == 0)
            {
                const float normA = 1.0f / std::sqrt(jitterCosA * jitterCosA + jitterSinA * jitterSinA);
                jitterCosA *= normA; jitterSinA *= normA;
                const float normB = 1.0f / std::sqrt(jitterCosB * jitterCosB + jitterSinB * jitterSinB);
                jitterCosB *= normB; jitterSinB *= normB;
                // C3: Renormalize ch1 oscillators too
                const float normA1 = 1.0f / std::sqrt(jitterCosA_ch1 * jitterCosA_ch1 + jitterSinA_ch1 * jitterSinA_ch1);
                jitterCosA_ch1 *= normA1; jitterSinA_ch1 *= normA1;
                const float normB1 = 1.0f / std::sqrt(jitterCosB_ch1 * jitterCosB_ch1 + jitterSinB_ch1 * jitterSinB_ch1);
                jitterCosB_ch1 *= normB1; jitterSinB_ch1 *= normB1;
            }
        }

        // C3: Per-channel jitter for stereo decorrelation (ch1 independent state)
        float jitteredRatio_ch1 = ratio;
        if (jitterAmount > 0.0f)
        {
            {
                const float c = jitterCosA_ch1 * jitterRotCosA - jitterSinA_ch1 * jitterRotSinA;
                const float s2 = jitterSinA_ch1 * jitterRotCosA + jitterCosA_ch1 * jitterRotSinA;
                jitterCosA_ch1 = c; jitterSinA_ch1 = s2;
            }
            {
                const float c = jitterCosB_ch1 * jitterRotCosB - jitterSinB_ch1 * jitterRotSinB;
                const float s2 = jitterSinB_ch1 * jitterRotCosB + jitterCosB_ch1 * jitterRotSinB;
                jitterCosB_ch1 = c; jitterSinB_ch1 = s2;
            }
            jitterState_ch1 = jitterState_ch1 * 1664525u + 1013904223u;
            const float noise1 = static_cast<float>(static_cast<int32_t>(jitterState_ch1)) * (1.0f / 2147483648.0f);
            jitterWalkLP_ch1 += kJitterWalkAlpha * (noise1 - jitterWalkLP_ch1);
            jitterPinkState_ch1 = 0.99765f * jitterPinkState_ch1 + noise1 * 0.09905f;
            const float pink1 = (jitterPinkState_ch1 + noise1 * 0.0555f) * 0.65f;
            const float jitterMod1 = jitterWalkLP_ch1 * 0.4f + jitterSinA_ch1 * 0.2f
                                   + jitterSinB_ch1 * 0.1f + pink1 * 0.3f;
            jitteredRatio_ch1 = ratio * (1.0f + jitterMod1 * jitterAmount * 0.008f);
        }

        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto* data = buffer.getWritePointer(ch);
            const float sample = data[i];

            // 4th-order Butterworth pre-S&H anti-alias
            const float aa1 = preAaB0 * sample + preAaZ1[ch];
            preAaZ1[ch] = preAaB1 * sample - preAaA1 * aa1 + preAaZ2[ch];
            preAaZ2[ch] = preAaB2 * sample - preAaA2 * aa1;
            const float filtered = preAaB0b * aa1 + preAaZ1b[ch];
            preAaZ1b[ch] = preAaB1b * aa1 - preAaA1b * filtered + preAaZ2b[ch];
            preAaZ2b[ch] = preAaB2b * aa1 - preAaA2b * filtered;

            // C10 bass shelf: -3dB pre-quantization
            const float bassReduced = bassPreB0 * filtered + bassPreZ1[ch];
            bassPreZ1[ch] = bassPreB1 * filtered - bassPreA1 * bassReduced + bassPreZ2[ch];
            bassPreZ2[ch] = bassPreB2 * filtered - bassPreA2 * bassReduced;

            // C3: Select per-channel jitter ratio for stereo decorrelation
            const float chJitteredRatio = (ch == 0) ? jitteredRatio : jitteredRatio_ch1;

            // Sample-and-hold with aperture guard (D8: clamp holdCounter to prevent double-triggers)
            apertureState[ch] = apertureState[ch] * 1664525u + 1013904223u;
            const float apertureNoise = static_cast<float>(static_cast<int32_t>(apertureState[ch]))
                                        * (1.0f / 2147483648.0f);
            holdCounter[ch] += chJitteredRatio + apertureNoise * jitterAmount * 0.05f;
            holdCounter[ch] = juce::jlimit(0.0f, 2.0f, holdCounter[ch]); // D8: guard
            if (holdCounter[ch] >= 1.0f)
            {
                holdCounter[ch] -= 1.0f;
                const float t = holdCounter[ch];
                holdSample[ch] = prevBassReduced[ch] + t * (bassReduced - prevBassReduced[ch]);
            }
            prevBassReduced[ch] = bassReduced;

            // 3rd-order noise shaping (C1): Wannamaker F-weighted error feedback
            // Coefficients optimized for 44.1kHz: pushes 12-15dB more quantization noise above 10kHz
            const float shapedInput = holdSample[ch]
                - (noiseShapeE1[ch] * 1.623f + noiseShapeE2[ch] * -0.982f + noiseShapeE3[ch] * 0.176f);

            // TPDF dither scaled by signal level for perceptual consistency (D4)
            const float halfStep = 0.5f / levels;
            // Scale dither with signal RMS envelope (ensures audibility at high bit depths)
            const float ditherScale = std::max(halfStep, std::abs(shapedInput) * 0.001f);
            ditherState[ch] = ditherState[ch] * 1664525u + 1013904223u;
            const float r1 = static_cast<float>(static_cast<int32_t>(ditherState[ch]))
                             * (1.0f / 2147483648.0f);
            ditherState[ch] = ditherState[ch] * 1664525u + 1013904223u;
            const float r2 = static_cast<float>(static_cast<int32_t>(ditherState[ch]))
                             * (1.0f / 2147483648.0f);
            const float dithered = shapedInput + (r1 - r2) * ditherScale;
            const float quantized = juce::jlimit(-1.0f, 1.0f,
                (levels <= 2.0f)
                    ? (dithered >= 0.0f ? 0.5f : -0.5f)
                    : std::floor(dithered * levels + 0.5f) / levels);

            // Update 3rd-order noise shaping error history (C1)
            noiseShapeE3[ch] = noiseShapeE2[ch];
            noiseShapeE2[ch] = noiseShapeE1[ch];
            noiseShapeE1[ch] = quantized - holdSample[ch];

            // 4th-order Butterworth Nyquist ceiling
            const float out1 = nyqB0 * quantized + nyquistZ1[ch];
            nyquistZ1[ch] = nyqB1 * quantized - nyqA1 * out1 + nyquistZ2[ch];
            nyquistZ2[ch] = nyqB2 * quantized - nyqA2 * out1;
            const float out2 = nyqB0b * out1 + nyquistZ1b[ch];
            nyquistZ1b[ch] = nyqB1b * out1 - nyqA1b * out2 + nyquistZ2b[ch];
            nyquistZ2b[ch] = nyqB2b * out1 - nyqA2b * out2;

            // ZOH droop compensation (D2): 1st-order high shelf restores HF lost to sinc envelope
            const float zohOut = zohShelfB0 * out2 + zohShelfB1 * zohShelfZ1[ch];
            const float zohFinal = zohOut - zohShelfA1 * zohShelfZ1[ch];
            // Correct 1st-order shelf: y[n] = b0*x[n] + b1*x[n-1] - a1*y[n-1]
            const float shelfResult = zohShelfB0 * out2 + zohShelfZ1[ch];
            zohShelfZ1[ch] = zohShelfB1 * out2 - zohShelfA1 * shelfResult;
            const float postZoh = shelfResult;

            // C2: Bessel reconstruction — models SP-1200 DAC output filter
            const float bess1 = bessB0a * postZoh + besselZ1a[ch];
            besselZ1a[ch] = bessB1a * postZoh - bessA1a * bess1 + besselZ2a[ch];
            besselZ2a[ch] = bessB2a * postZoh - bessA2a * bess1;
            const float besselOut = bessB0b * bess1 + besselZ1b[ch];
            besselZ1b[ch] = bessB1b * bess1 - bessA1b * besselOut + besselZ2b[ch];
            besselZ2b[ch] = bessB2b * bess1 - bessA2b * besselOut;

            // C10 bass restoration: +3dB post-quantization
            const float postOut = bassPostB0 * besselOut + bassPostZ1[ch];
            bassPostZ1[ch] = bassPostB1 * besselOut - bassPostA1 * postOut + bassPostZ2[ch];
            bassPostZ2[ch] = bassPostB2 * besselOut - bassPostA2 * postOut;

            // DC blocker (1-pole HP at ~5Hz)
            const float dcOut = postOut - dcBlockX1[ch] + dcBlockCoeff * dcBlockY1[ch];
            dcBlockX1[ch] = postOut;
            dcBlockY1[ch] = dcOut;

            data[i] = dcOut;
        }
    }
}
