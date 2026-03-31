#include "BitCrusher.h"

// Soft-knee ADC input saturation constants
// At ±1.0 input: tanh(2.0) ≈ 0.964 — graceful rollover, not a hard brick-wall
// Normalization preserves unity gain through the linear region (below ±0.5)
static constexpr float kAdcSatDrive = 2.0f;
static constexpr float kAdcSatNorm  = 0.9640f;  // 1.0f / std::tanh(kAdcSatDrive) precomputed

void BitCrusher::prepare(double sampleRate, int /*samplesPerBlock*/)
{
    hostSampleRate = sampleRate;
    bitDepthSmoothed.reset(sampleRate, 0.005);
    targetRateSmoothed.reset(sampleRate, 0.005);

    for (int ch = 0; ch < kMaxChannels; ++ch)
    {
        holdCounter[ch] = 0.0f;
        holdSample[ch] = 0.0f;
        preAaZ1[ch] = 0.0f;
        preAaZ2[ch] = 0.0f;
        nyquistZ1[ch] = 0.0f;
        nyquistZ2[ch] = 0.0f;
        nyquistZ1b[ch] = 0.0f;
        nyquistZ2b[ch] = 0.0f;
        ditherState[ch] = 0xDEAD0001u + static_cast<uint32_t>(ch);
        ditherError[ch] = 0.0f;
        ditherErrorPrev[ch] = 0.0f;
        upsampleHistory[ch][0] = 0.0f;
        upsampleHistory[ch][1] = 0.0f;
        upsampleHistory[ch][2] = 0.0f;
    }
    preAaLastCutoff = -1.0f;
    preAaB0 = 1.0f; preAaB1 = 0.0f; preAaB2 = 0.0f; preAaA1 = 0.0f; preAaA2 = 0.0f;

    jitterPhase = 0.0f;
    jitterPhaseCapstan = 0.0f;
    jitterPhaseReel    = 0.0f;
    jitterState = 12345u;

    // Nyquist ceiling coefficients are computed dynamically in process() based on targetRate.
    // Reset the cache so the first block triggers a fresh coefficient computation.
    nyqLastTargetRate = -1.0f;

    // Frequency-dependent bit-crushing: low-shelf pair at 200Hz (C10)
    // Pre: -3dB to simulate bass accuracy loss; Post: +3dB to restore level (noise stays)
    {
        const float pi = juce::MathConstants<float>::pi;
        const float sr = static_cast<float>(sampleRate);
        const float w0 = 2.0f * pi * 200.0f / sr;
        const float cosW0 = std::cos(w0);
        const float sinW0 = std::sin(w0);
        // Pre-shelf: -3dB low shelf, A = 10^(-3/40) ≈ 0.8414
        {
            const float A = 0.8414f;
            const float alpha = sinW0 / 2.0f * std::sqrt((A + 1.0f / A) * (1.0f / 1.0f - 1.0f) + 2.0f);
            const float sqrtA = std::sqrt(A);
            const float a0r = 1.0f / ((A + 1.0f) + (A - 1.0f) * cosW0 + 2.0f * sqrtA * alpha);
            bassPreB0 = A * ((A + 1.0f) - (A - 1.0f) * cosW0 + 2.0f * sqrtA * alpha) * a0r;
            bassPreB1 = 2.0f * A * ((A - 1.0f) - (A + 1.0f) * cosW0) * a0r;
            bassPreB2 = A * ((A + 1.0f) - (A - 1.0f) * cosW0 - 2.0f * sqrtA * alpha) * a0r;
            bassPreA1 = -2.0f * ((A - 1.0f) + (A + 1.0f) * cosW0) * a0r;
            bassPreA2 = ((A + 1.0f) + (A - 1.0f) * cosW0 - 2.0f * sqrtA * alpha) * a0r;
        }
        // Post-shelf: +3dB low shelf, A = 10^(+3/40) ≈ 1.1885
        {
            const float A = 1.1885f;
            const float alpha = sinW0 / 2.0f * std::sqrt((A + 1.0f / A) * (1.0f / 1.0f - 1.0f) + 2.0f);
            const float sqrtA = std::sqrt(A);
            const float a0r = 1.0f / ((A + 1.0f) + (A - 1.0f) * cosW0 + 2.0f * sqrtA * alpha);
            bassPostB0 = A * ((A + 1.0f) - (A - 1.0f) * cosW0 + 2.0f * sqrtA * alpha) * a0r;
            bassPostB1 = 2.0f * A * ((A - 1.0f) - (A + 1.0f) * cosW0) * a0r;
            bassPostB2 = A * ((A + 1.0f) - (A - 1.0f) * cosW0 - 2.0f * sqrtA * alpha) * a0r;
            bassPostA1 = -2.0f * ((A - 1.0f) + (A + 1.0f) * cosW0) * a0r;
            bassPostA2 = ((A + 1.0f) + (A - 1.0f) * cosW0 - 2.0f * sqrtA * alpha) * a0r;
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

void BitCrusher::setHardwareProfile(int profile)
{
    // Measured hardware specs: {sampleRate Hz, bits, filter cutoff Hz}
    struct HWSpec { float sampleRate; float bits; };
    static const HWSpec kProfiles[] = {
        { 0.0f,     0.0f  },  // 0 = Custom — no override
        { 26040.0f, 12.0f },  // 1 = SP-1200 (hip-hop standard)
        { 40000.0f, 16.0f },  // 2 = MPC-60 (clean 16-bit)
        { 45000.0f, 12.0f },  // 3 = S950 (bright 12-bit)
        { 28000.0f,  8.0f },  // 4 = SP-12 (gritty 8-bit)
    };
    if (profile < 1 || profile > 4) return;
    const auto& p = kProfiles[profile];
    setSampleRate(p.sampleRate);
    setBitDepth(p.bits);
}

void BitCrusher::process(juce::AudioBuffer<float>& buffer)
{
    juce::ScopedNoDenormals noDenormals;
    const auto numChannels = std::min(buffer.getNumChannels(), kMaxChannels);
    const auto numSamples = buffer.getNumSamples();

    for (int i = 0; i < numSamples; ++i)
    {
        const float bitDepth = bitDepthSmoothed.getNextValue();
        const float targetRate = targetRateSmoothed.getNextValue();
        const float levels = std::exp2f(bitDepth);
        const float ratio = targetRate / static_cast<float>(hostSampleRate);

        // 2-pole Butterworth LP at targetRate/2 — recompute only when rate changes
        const float normalizedCutoff = targetRate * 0.5f / static_cast<float>(hostSampleRate);
        if (std::abs(normalizedCutoff - preAaLastCutoff) > 1e-3f)
        {
            preAaLastCutoff = normalizedCutoff;
            // Bilinear transform Butterworth design at normalized freq [0, 0.5]
            const float w0 = juce::MathConstants<float>::pi * normalizedCutoff * 2.0f;  // rad/sample
            const float cosW0 = std::cos(w0);
            const float sinW0 = std::sin(w0);
            const float alpha = sinW0 / (2.0f * 0.7071f);  // Q = 1/sqrt(2) for Butterworth
            const float a0inv = 1.0f / (1.0f + alpha);
            preAaB0 = ((1.0f - cosW0) * 0.5f) * a0inv;
            preAaB1 = (1.0f - cosW0) * a0inv;
            preAaB2 = preAaB0;
            preAaA1 = (-2.0f * cosW0) * a0inv;
            preAaA2 = (1.0f - alpha) * a0inv;
        }

        // Clock jitter: 3-frequency stack (motor 90Hz + capstan 200Hz + reel 0.3Hz) + noise
        float jitteredRatio = ratio;
        if (jitterAmount > 0.0f)
        {
            const float twoPiOverSr = juce::MathConstants<float>::twoPi / static_cast<float>(hostSampleRate);
            jitterPhase += twoPiOverSr * jitterFreq;
            if (jitterPhase > juce::MathConstants<float>::twoPi)
                jitterPhase -= juce::MathConstants<float>::twoPi;
            jitterPhaseCapstan += twoPiOverSr * 200.0f;
            if (jitterPhaseCapstan > juce::MathConstants<float>::twoPi)
                jitterPhaseCapstan -= juce::MathConstants<float>::twoPi;
            jitterPhaseReel += twoPiOverSr * 0.3f;
            if (jitterPhaseReel > juce::MathConstants<float>::twoPi)
                jitterPhaseReel -= juce::MathConstants<float>::twoPi;
            jitterState = jitterState * 1664525u + 1013904223u;
            const float noise   = static_cast<float>(static_cast<int32_t>(jitterState))
                                  * (1.0f / 2147483648.0f);
            const float motor   = std::sin(jitterPhase);
            const float capstan = std::sin(jitterPhaseCapstan) * 0.4f;
            const float reel    = std::sin(jitterPhaseReel) * 0.8f;
            jitteredRatio = ratio * (1.0f + (motor * 0.5f + capstan * 0.2f + reel * 0.2f + noise * 0.1f)
                                           * jitterAmount * 0.4f);
        }

        // Dynamic Nyquist ceiling: tracks targetRate * 0.45 (45% of target Nyquist)
        if (std::abs(targetRate - nyqLastTargetRate) > 10.0f)
        {
            nyqLastTargetRate = targetRate;
            const float nyqCutoff = juce::jlimit(500.0f, 22000.0f, targetRate * 0.45f);
            const float sr = static_cast<float>(hostSampleRate);
            const float w0 = juce::MathConstants<float>::twoPi * nyqCutoff / sr;
            const float cosW0 = std::cos(w0);
            const float alpha = std::sin(w0) / (2.0f * 0.7071f);
            const float a0inv = 1.0f / (1.0f + alpha);
            nyqB0 = ((1.0f - cosW0) * 0.5f) * a0inv;
            nyqB1 = (1.0f - cosW0) * a0inv;
            nyqB2 = nyqB0;
            nyqA1 = (-2.0f * cosW0) * a0inv;
            nyqA2 = (1.0f - alpha) * a0inv;
        }

        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto* data = buffer.getWritePointer(ch);
            float sample = data[i];

            // Soft-knee ADC input saturation (replaces hard clip)
            // Near-perfect linearity below ±0.5, gentle compression 0.5–0.8,
            // soft rolloff above 0.8 — asymptotically approaches ±1.0, never exceeds it
            sample = std::tanh(sample * kAdcSatDrive) * kAdcSatNorm;

            // 2-pole Butterworth pre-S&H anti-alias
            const float filtered = preAaB0 * sample + preAaZ1[ch];
            preAaZ1[ch] = preAaB1 * sample - preAaA1 * filtered + preAaZ2[ch];
            preAaZ2[ch] = preAaB2 * sample - preAaA2 * filtered;
            sample = filtered;

            // Frequency-dependent bass attenuation before quantization (C10)
            // Simulates ADC bass accuracy loss: -3dB shelf at 200Hz
            const float bassReduced = bassPreB0 * filtered + bassPreZ1[ch];
            bassPreZ1[ch] = bassPreB1 * filtered - bassPreA1 * bassReduced + bassPreZ2[ch];
            bassPreZ2[ch] = bassPreB2 * filtered - bassPreA2 * bassReduced;

            // Catmull-Rom (Hermite) S&H intake smoothing
            // When the hold fires we use an interpolated sample rather than the raw step,
            // reducing the spectral splatter caused by the S&H discontinuity.
            // History: upsampleHistory[ch] = { p0(oldest), p1, p2 }, p3 = bass-reduced (current)
            const float p0 = upsampleHistory[ch][0];
            const float p1 = upsampleHistory[ch][1];
            const float p2 = upsampleHistory[ch][2];
            const float p3 = bassReduced;

            // Shift history (oldest dropped, current bass-reduced sample becomes newest)
            upsampleHistory[ch][0] = upsampleHistory[ch][1];
            upsampleHistory[ch][1] = upsampleHistory[ch][2];
            upsampleHistory[ch][2] = bassReduced;

            // Sample rate reduction (sample-and-hold) — BEFORE quantization
            // Aperture jitter: ~1-5µs ADC uncertainty distinct from clock jitter
            ditherState[ch] = ditherState[ch] * 1664525u + 1013904223u;
            const float apertureNoise = static_cast<float>(static_cast<int32_t>(ditherState[ch])) * (1.0f / 2147483648.0f);
            holdCounter[ch] += jitteredRatio + apertureNoise * jitterAmount * 0.05f;
            if (holdCounter[ch] >= 1.0f)
            {
                holdCounter[ch] -= 1.0f;
                // Catmull-Rom at variable t matching the actual sampling instant (B14 fix)
                const float t = holdCounter[ch]; // fractional overshoot = position in interval
                const float crA = -0.5f*p0 + 1.5f*p1 - 1.5f*p2 + 0.5f*p3;
                const float crB =        p0 - 2.5f*p1 + 2.0f*p2 - 0.5f*p3;
                const float crC = -0.5f*p0             + 0.5f*p2;
                const float crD =  p1;
                holdSample[ch] = ((crA*t + crB)*t + crC)*t + crD;
            }

            // 2nd-order noise-shaped dither with 8× averaging oversampling (~9dB noise floor reduction)
            // 8 independent TPDF-dithered quantizations of holdSample are averaged per host sample.
            // Error feedback (2nd-order noise shaping) applies only on the first sub-sample.
            const float halfStep = 0.5f / levels;
            const float feedbackTerm = 2.0f * ditherError[ch] - ditherErrorPrev[ch];
            float qSum = 0.0f;
            for (int k = 0; k < kQuantOS; ++k)
            {
                ditherState[ch] = ditherState[ch] * 1664525u + 1013904223u;
                const float r1 = static_cast<float>(static_cast<int32_t>(ditherState[ch])) * (1.0f / 2147483648.0f);
                ditherState[ch] = ditherState[ch] * 1664525u + 1013904223u;
                const float r2 = static_cast<float>(static_cast<int32_t>(ditherState[ch])) * (1.0f / 2147483648.0f);
                const float tpdf = (r1 - r2) * halfStep;
                const float fb = (k == 0) ? feedbackTerm : 0.0f;
                const float dithered = holdSample[ch] + tpdf - fb;
                qSum += (levels <= 2.0f)
                    ? (dithered >= 0.0f ? 0.5f : -0.5f)
                    : std::floor(dithered * levels + 0.5f) / levels;
            }
            const float quantized = qSum * kQuantOSInv;
            ditherErrorPrev[ch] = ditherError[ch];
            ditherError[ch] = quantized - holdSample[ch];

            // Nyquist ceiling: 4-pole Butterworth LP at targetRate * 0.45 (2 cascaded stages)
            // Stage 1
            const float out1 = nyqB0 * quantized + nyquistZ1[ch];
            nyquistZ1[ch] = nyqB1 * quantized - nyqA1 * out1 + nyquistZ2[ch];
            nyquistZ2[ch] = nyqB2 * quantized - nyqA2 * out1;
            // Stage 2
            const float out2 = nyqB0 * out1 + nyquistZ1b[ch];
            nyquistZ1b[ch] = nyqB1 * out1 - nyqA1 * out2 + nyquistZ2b[ch];
            nyquistZ2b[ch] = nyqB2 * out1 - nyqA2 * out2;

            // Post-quantization bass restoration (C10): +3dB shelf at 200Hz
            // Restores level; quantization noise in bass stays uncompensated (intentional)
            const float postOut = bassPostB0 * out2 + bassPostZ1[ch];
            bassPostZ1[ch] = bassPostB1 * out2 - bassPostA1 * postOut + bassPostZ2[ch];
            bassPostZ2[ch] = bassPostB2 * out2 - bassPostA2 * postOut;
            data[i] = postOut;
        }
    }
}
