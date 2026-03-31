#include "Saturation.h"

void Saturation::prepare(double newSampleRate, int /*samplesPerBlock*/)
{
    sampleRate = newSampleRate;
    const double rampTimeSec = 0.02; // 20ms ramp
    inputGainSmoothed.reset(sampleRate, rampTimeSec);
    outputGainSmoothed.reset(sampleRate, rampTimeSec);
    driveSmoothed.reset(sampleRate, rampTimeSec);
    biasSmoothed.reset(sampleRate, rampTimeSec);
    asymSmoothed.reset(sampleRate, rampTimeSec);

    // DC blocker coefficient for ~20Hz high-pass, adaptive to sample rate
    constexpr double dcCutoffHz = 20.0;
    dcCoeff = static_cast<float>(std::exp(-2.0 * juce::MathConstants<double>::pi * dcCutoffHz / sampleRate));

    for (int ch = 0; ch < kMaxChannels; ++ch)
    {
        dcX1[ch] = 0.0f;
        dcY1[ch] = 0.0f;
        xfmrHPState[ch] = {};
        xfmrLPState[ch] = {};
        prevSample[ch] = 0.0f;
        prevPrevSample[ch] = 0.0f;
        for (int j = 0; j < 4; ++j)
        {
            hb1History[ch][j]  = 0.0f;
            hb2History[ch][j]  = 0.0f;
            hb3History[ch][j]  = 0.0f;
            hb4History[ch][j]  = 0.0f;
            hb3aHistory[ch][j] = 0.0f;
            hb3bHistory[ch][j] = 0.0f;
            hb4aHistory[ch][j] = 0.0f;
        }
    }
    computeTransformerCoeffs();
}

void Saturation::computeTransformerCoeffs()
{
    const float sr = static_cast<float>(sampleRate);
    const float pi = juce::MathConstants<float>::pi;

    // HP: 30Hz coupling capacitor simulation (iron core LF coupling)
    {
        const float f = 30.0f;
        const float w = 2.0f * pi * f / sr;
        const float c = std::cos(w);
        const float a = std::sin(w) / (2.0f * 0.5f); // Q=0.5 for gentle slope
        const float a0 = 1.0f + a;
        xfmrHP[0] = (1.0f + c) / 2.0f / a0;
        xfmrHP[1] = -(1.0f + c) / a0;
        xfmrHP[2] = xfmrHP[0];
        xfmrHP[3] = -2.0f * c / a0;
        xfmrHP[4] = (1.0f - a) / a0;
    }
    // LP: 18kHz iron core HF rolloff
    {
        const float f = 18000.0f;
        const float w = 2.0f * pi * f / sr;
        const float c = std::cos(w);
        const float a = std::sin(w) / (2.0f * 0.707f);
        const float a0 = 1.0f + a;
        xfmrLP[0] = (1.0f - c) / 2.0f / a0;
        xfmrLP[1] = (1.0f - c) / a0;
        xfmrLP[2] = xfmrLP[0];
        xfmrLP[3] = -2.0f * c / a0;
        xfmrLP[4] = (1.0f - a) / a0;
    }
}

void Saturation::setInputGain(float gainDb)
{
    if (currentInputGainDb != gainDb)
    {
        currentInputGainDb = gainDb;
        inputGainSmoothed.setTargetValue(dBToLinear(gainDb));
    }
}

void Saturation::setOutputGain(float gainDb)
{
    if (currentOutputGainDb != gainDb)
    {
        currentOutputGainDb = gainDb;
        outputGainSmoothed.setTargetValue(dBToLinear(gainDb));
    }
}

void Saturation::setDrive(float driveAmount)
{
    if (currentDrive != driveAmount)
    {
        currentDrive = driveAmount;
        driveSmoothed.setTargetValue(driveAmount);
    }
}

void Saturation::setBias(float b)
{
    if (currentBias != b)
    {
        currentBias = b;
        biasSmoothed.setTargetValue(b);
    }
}

void Saturation::setMode(int m)
{
    currentMode = juce::jlimit(0, 5, m);
}

void Saturation::setAsym(float a)
{
    if (currentAsym != a)
    {
        currentAsym = a;
        asymSmoothed.setTargetValue(a);
    }
}

void Saturation::setTone(float t)
{
    if (currentTone == t) return;
    currentTone = t;

    const float sr = static_cast<float>(sampleRate);
    const float pi = juce::MathConstants<float>::pi;

    // HP: 80Hz (t=0) → 500Hz (t=1), removes muddiness
    {
        const float freq = 80.0f * std::pow(6.25f, t); // 80*6.25^t Hz
        const float w0 = 2.0f * pi * freq / sr;
        const float cosW0 = std::cos(w0);
        const float alpha = std::sin(w0) / (2.0f * 0.707f);
        const float a0 = 1.0f + alpha;
        toneHP[0] = ((1.0f + cosW0) / 2.0f) / a0; // b0
        toneHP[1] = -(1.0f + cosW0) / a0;          // b1
        toneHP[2] = ((1.0f + cosW0) / 2.0f) / a0;  // b2
        toneHP[3] = (-2.0f * cosW0) / a0;           // a1
        toneHP[4] = (1.0f - alpha) / a0;            // a2
    }

    // LP: 2kHz (t=0) → 20kHz (t=1), treble roll-off
    {
        const float freq = 2000.0f * std::pow(10.0f, t); // 2000*10^t Hz
        const float w0 = 2.0f * pi * freq / sr;
        const float cosW0 = std::cos(w0);
        const float alpha = std::sin(w0) / (2.0f * 0.707f);
        const float a0 = 1.0f + alpha;
        toneLP[0] = ((1.0f - cosW0) / 2.0f) / a0; // b0
        toneLP[1] = (1.0f - cosW0) / a0;           // b1
        toneLP[2] = ((1.0f - cosW0) / 2.0f) / a0;  // b2
        toneLP[3] = (-2.0f * cosW0) / a0;           // a1
        toneLP[4] = (1.0f - alpha) / a0;            // a2
    }
}

void Saturation::processTone(juce::AudioBuffer<float>& buffer)
{
    juce::ScopedNoDenormals noDenormals;
    const auto numChannels = std::min(buffer.getNumChannels(), kMaxChannels);
    const auto numSamples = buffer.getNumSamples();

    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto* data = buffer.getWritePointer(ch);
        auto& hp = toneHPState[static_cast<size_t>(ch)];
        auto& lp = toneLPState[static_cast<size_t>(ch)];

        for (int i = 0; i < numSamples; ++i)
        {
            // HP biquad
            const float hpOut = toneHP[0] * data[i] + hp.z1;
            hp.z1 = toneHP[1] * data[i] - toneHP[3] * hpOut + hp.z2;
            hp.z2 = toneHP[2] * data[i] - toneHP[4] * hpOut;

            // LP biquad
            const float lpOut = toneLP[0] * hpOut + lp.z1;
            lp.z1 = toneLP[1] * hpOut - toneLP[3] * lpOut + lp.z2;
            lp.z2 = toneLP[2] * hpOut - toneLP[4] * lpOut;

            data[i] = lpOut;
        }
    }
}

void Saturation::processInput(juce::AudioBuffer<float>& buffer)
{
    juce::ScopedNoDenormals noDenormals;
    applyGainAndSaturation(buffer, inputGainSmoothed, driveSmoothed);

    // DC blocker: y[n] = x[n] - x[n-1] + R * y[n-1], R = 0.995
    const auto numChannels = std::min(buffer.getNumChannels(), kMaxChannels);
    const auto numSamples = buffer.getNumSamples();
    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto* data = buffer.getWritePointer(ch);
        for (int i = 0; i < numSamples; ++i)
        {
            const float x = data[i];
            const float y = x - dcX1[ch] + dcCoeff * dcY1[ch];
            dcX1[ch] = x;
            dcY1[ch] = y;
            data[i] = y;
        }
    }
}

void Saturation::processOutput(juce::AudioBuffer<float>& buffer)
{
    juce::ScopedNoDenormals noDenormals;
    const auto numChannels = std::min(buffer.getNumChannels(), kMaxChannels);
    const auto numSamples = buffer.getNumSamples();

    for (int i = 0; i < numSamples; ++i)
    {
        const float gain = outputGainSmoothed.getNextValue();
        for (int ch = 0; ch < numChannels; ++ch)
            buffer.getWritePointer(ch)[i] *= gain;
    }

    // Output transformer stage: 30Hz coupling HP + 18kHz iron-core LP (always active)
    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto* data = buffer.getWritePointer(ch);
        auto& hpSt = xfmrHPState[static_cast<size_t>(ch)];
        auto& lpSt = xfmrLPState[static_cast<size_t>(ch)];
        for (int i = 0; i < numSamples; ++i)
        {
            // HP coupling capacitor
            const float hp = xfmrHP[0] * data[i] + hpSt.z1;
            hpSt.z1 = xfmrHP[1] * data[i] - xfmrHP[3] * hp + hpSt.z2;
            hpSt.z2 = xfmrHP[2] * data[i] - xfmrHP[4] * hp;
            // LP iron-core rolloff
            const float lp = xfmrLP[0] * hp + lpSt.z1;
            lpSt.z1 = xfmrLP[1] * hp - xfmrLP[3] * lp + lpSt.z2;
            lpSt.z2 = xfmrLP[2] * hp - xfmrLP[4] * lp;
            data[i] = lp;
        }
    }
}

// 15-tap Kaiser half-band polyphase decimation helper (2:1 decimation, ~90dB stopband rejection).
// Even polyphase branch uses 8-sample history; odd branch is center tap × 0.5.
float Saturation::halfBandDecimate(const float in0, const float in1, float* history) noexcept
{
    history[0] = history[1];
    history[1] = history[2];
    history[2] = history[3];
    history[3] = history[4];
    history[4] = history[5];
    history[5] = history[6];
    history[6] = history[7];
    history[7] = in0;
    // 15-tap Kaiser half-band even polyphase (~90dB stopband rejection)
    const float even = -0.001586f * (history[0] + history[7])
                     +  0.011750f * (history[1] + history[6])
                     + -0.052800f * (history[2] + history[5])
                     +  0.295636f * (history[3] + history[4]);
    return even + 0.5f * in1;
}

void Saturation::applyGainAndSaturation(juce::AudioBuffer<float>& buffer,
                                         juce::SmoothedValue<float>& gainSmoothed,
                                         juce::SmoothedValue<float>& drvSmoothed)
{
    const auto numChannels = std::min(buffer.getNumChannels(), kMaxChannels);
    const auto numSamples = buffer.getNumSamples();

    for (int i = 0; i < numSamples; ++i)
    {
        const float gain = gainSmoothed.getNextValue();
        const float drive = drvSmoothed.getNextValue();
        const float bias = biasSmoothed.getNextValue();
        const float asym = asymSmoothed.getNextValue();
        const float scale = 1.0f + drive * 4.0f;
        const float normalization = (scale > 0.01f) ? 1.0f / std::tanh(scale) : 1.0f;
        const float biasCancel = std::tanh(bias);

        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto* data = buffer.getWritePointer(ch);
            const float sample = data[i] * gain;

            // Catmull-Rom (Hermite cubic) upsampling — eliminates -6dB droop at fs/8
            const float p0 = prevPrevSample[ch];      // x[n-2]
            const float p1 = prevSample[ch];          // x[n-1]
            const float p2 = sample;                  // x[n]
            const float p3 = 3.0f * p2 - 3.0f * p1 + p0; // parabolic (2nd-order) extrapolation of x[n+1] (causal)

            // Catmull-Rom coefficients (interpolates between p1 and p2)
            const float a = -0.5f*p0 + 1.5f*p1 - 1.5f*p2 + 0.5f*p3;
            const float b =        p0 - 2.5f*p1 + 2.0f*p2 - 0.5f*p3;
            const float c = -0.5f*p0             + 0.5f*p2;
            const float d =  p1;

            float os[8];
            for (int k = 0; k < 8; ++k)
            {
                const float t = (k + 1) * 0.125f;  // 0.125, 0.25, 0.375, 0.50, 0.625, 0.75, 0.875, 1.00
                os[k] = ((a*t + b)*t + c)*t + d;
            }
            prevPrevSample[ch] = prevSample[ch];
            prevSample[ch] = sample;

            // Soft-knee threshold: scales with drive so higher drive engages saturation earlier
            const float kneeThreshold = juce::jlimit(0.1f, 0.5f, 0.6f / scale);

            // Apply saturation nonlinearity to each oversampled value
            for (int k = 0; k < 8; ++k)
            {
                float s = os[k];

                // Soft-knee blend weight (used by modes 0, 1, 4)
                const float absIn = std::abs(s);
                const float kneeBlend = juce::jlimit(0.0f, 1.0f, (absIn - kneeThreshold) / kneeThreshold);
                // Cubic smoothstep: 3t² - 2t³
                const float smoothBlend = kneeBlend * kneeBlend * (3.0f - 2.0f * kneeBlend);

                if (currentMode == 3) // Transformer: arctangent + iron core even-harmonic series (2H/4H/6H)
                {
                    const float driven = s * scale * 1.5f + bias;
                    const float arctan = (2.0f / juce::MathConstants<float>::pi) * std::atan(driven)
                                       - (2.0f / juce::MathConstants<float>::pi) * std::atan(bias);
                    // Iron core hysteresis: even-order harmonics (2H dominant, 4H secondary, 6H tertiary)
                    const float arctan2 = arctan * arctan;
                    const float h2 = 0.10f * arctan2;                      // 2nd harmonic (strong)
                    const float h4 = 0.025f * arctan2 * arctan2;           // 4th harmonic
                    const float h6 = 0.006f * arctan2 * arctan2 * arctan2; // 6th harmonic
                    s = (arctan + h2 + h4 + h6) * normalization * 0.88f;   // -1.1dB makeup
                }
                else if (currentMode == 2) // Hard clip — no soft-knee, abrupt onset is intentional
                {
                    s = juce::jlimit(-1.0f, 1.0f, s * scale * 4.0f) * normalization * 0.25f;
                }
                else if (currentMode == 1) // Tube: asymmetric knee, softness controlled by asym
                {
                    const float kPos = 0.7f + asym * 0.3f;  // 0.7 (hard class-A) → 1.0 (symmetric)
                    const float kNeg = 1.3f - asym * 0.3f;  // 1.3 (hard) → 1.0 (symmetric)
                    const float scaledPos = s * scale * kPos;
                    const float scaledNeg = s * scale * kNeg;
                    const float tubeOut = s >= 0.0f
                        ? std::tanh(scaledPos + bias) - std::tanh(bias * kPos)
                        : std::tanh(scaledNeg + bias) - std::tanh(bias * kNeg);
                    const float saturated = tubeOut * normalization;
                    const float linear = s * normalization;
                    s = linear + smoothBlend * (saturated - linear);
                }
                else if (currentMode == 4) // Satin: super-soft cubic knee (tape-like, stays linear longest)
                {
                    const float x = s * scale;
                    // Cubic soft-knee: x - x³/3 in [-1,1], hard clamp outside
                    const float saturated = (std::abs(x) <= 1.0f)
                        ? (x - x * x * x / 3.0f) * 1.5f * normalization
                        : (x > 0.0f ? 1.0f : -1.0f) * normalization;
                    const float linear = s * normalization;
                    s = linear + smoothBlend * (saturated - linear);
                }
                else if (currentMode == 5) // Vari-Mu: level-dependent µ (variable-mu tube compression+sat)
                {
                    const float x = s * scale;
                    // µ decreases as signal level rises — classic Fairchild/Vari-Mu character
                    const float mu = 1.0f / (1.0f + std::abs(x) * 2.0f);
                    s = std::tanh(x * mu * 2.0f) * normalization;
                }
                else // Soft (0): symmetric tanh with bias
                {
                    const float saturated = (std::tanh(s * scale + bias) - biasCancel) * normalization;
                    const float linear = s * normalization;
                    s = linear + smoothBlend * (saturated - linear);
                }

                os[k] = s;
            }

            // 3-stage half-band decimation: 8x → 4x → 2x → 1x
            // Stage 1: pair adjacent samples → 4 outputs
            const float s1a = halfBandDecimate(os[0], os[1], hb1History[ch]);
            const float s1b = halfBandDecimate(os[2], os[3], hb2History[ch]);
            const float s1c = halfBandDecimate(os[4], os[5], hb3History[ch]);
            const float s1d = halfBandDecimate(os[6], os[7], hb4History[ch]);
            // Stage 2: pair stage-1 outputs → 2 outputs
            const float s2a = halfBandDecimate(s1a, s1b, hb3aHistory[ch]);
            const float s2b = halfBandDecimate(s1c, s1d, hb3bHistory[ch]);
            // Stage 3: pair stage-2 outputs → 1 output
            data[i] = halfBandDecimate(s2a, s2b, hb4aHistory[ch]);
        }
    }
}
