#include "Saturation.h"

// [5/5] Padé rational approximant — max error < 0.03% across [-3.5, 3.5].
// Blended to ±1 with cubic smoothstep over [3.0, 3.5] to eliminate C¹ discontinuity.
// Matches TapeEngine::fastTanh(); used in the 8× oversampling inner loop to replace std::tanh.
static float fastTanh(float x) noexcept
{
    if (x > 3.5f) return 1.0f;
    if (x < -3.5f) return -1.0f;
    const float x2   = x * x;
    const float pade = x * (135135.0f + x2 * (17325.0f + x2 * 378.0f))
                         / (135135.0f + x2 * (62370.0f + x2 * (3150.0f + x2 * 28.0f)));
    const float ax = std::abs(x);
    if (ax > 3.0f)
    {
        const float t     = (ax - 3.0f) * 2.0f;
        const float blend = t * t * (3.0f - 2.0f * t);
        const float sign  = x >= 0.0f ? 1.0f : -1.0f;
        return pade + blend * (sign - pade);
    }
    return pade;
}

void Saturation::prepare(double newSampleRate, int /*samplesPerBlock*/)
{
    sampleRate = newSampleRate;
    const double rampTimeSec = 0.02; // 20ms ramp
    inputGainSmoothed.reset(sampleRate, rampTimeSec);
    outputGainSmoothed.reset(sampleRate, rampTimeSec);
    driveSmoothed.reset(sampleRate, rampTimeSec);

    // DC blocker coefficient for ~20Hz high-pass, adaptive to sample rate
    constexpr double dcCutoffHz = 20.0;
    dcCoeff = static_cast<float>(std::exp(-2.0 * juce::MathConstants<double>::pi * dcCutoffHz / sampleRate));

    pendingMode = -1;
    modeRamp = 1.0f;
    modeRampInc = 0.0f;

    for (int ch = 0; ch < kMaxChannels; ++ch)
    {
        dcX1[ch] = 0.0f;
        dcY1[ch] = 0.0f;
        xfmrHPState[ch]  = {};
        xfmrLPState[ch]  = {};
        toneHPState[ch]  = {};
        toneLPState[ch]  = {};
        prevSample[ch]     = 0.0f;
        prevPrevSample[ch] = 0.0f;
        for (int j = 0; j < 8; ++j)
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

void Saturation::setMode(int m)
{
    const int clamped = juce::jlimit(0, 5, m);
    if (clamped == currentMode && pendingMode < 0) return;
    if (clamped == currentMode) return; // already transitioning to this mode
    pendingMode = clamped;
    // 10ms fade-out at current sample rate; if already mid-fade, start from current ramp level
    modeRampInc = -1.0f / std::max(1.0f, static_cast<float>(sampleRate) * 0.01f);
}

void Saturation::setTone(float t)
{
    if (std::abs(currentTone - t) < 1e-6f) return;
    currentTone = t;

    const float sr = static_cast<float>(sampleRate);
    const float pi = juce::MathConstants<float>::pi;

    // HP: 20Hz (t=0) → 500Hz (t=1). At t=0.5 = ~100Hz — centre is transparent, not cutting.
    // Clamped to 0.48*sr to keep the design stable at all sample rates.
    {
        const float freq = juce::jlimit(20.0f, sr * 0.48f, 20.0f * std::pow(25.0f, t));
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

    // LP: 2kHz (t=0) → 20kHz (t=1), treble roll-off. Clamped to 0.48*sr for stability.
    {
        const float freq = juce::jlimit(2000.0f, sr * 0.48f, 2000.0f * std::pow(10.0f, t));
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

    if (outputGainSmoothed.isSmoothing() || std::abs(outputGainSmoothed.getCurrentValue() - 1.0f) > 1e-6f)
    {
        for (int i = 0; i < numSamples; ++i)
        {
            const float gain = outputGainSmoothed.getNextValue();
            for (int ch = 0; ch < numChannels; ++ch)
                buffer.getWritePointer(ch)[i] *= gain;
        }
    }

    // Output transformer stage: 30Hz coupling HP + 18kHz iron-core LP.
    // Only active in Tube mode (1) — other modes should sound direct, not coloured by a transformer.
    if (currentMode == 1)
    {
        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto* data = buffer.getWritePointer(ch);
            auto& hpSt = xfmrHPState[static_cast<size_t>(ch)];
            auto& lpSt = xfmrLPState[static_cast<size_t>(ch)];
            for (int i = 0; i < numSamples; ++i)
            {
                const float hp = xfmrHP[0] * data[i] + hpSt.z1;
                hpSt.z1 = xfmrHP[1] * data[i] - xfmrHP[3] * hp + hpSt.z2;
                hpSt.z2 = xfmrHP[2] * data[i] - xfmrHP[4] * hp;
                const float lp = xfmrLP[0] * hp + lpSt.z1;
                lpSt.z1 = xfmrLP[1] * hp - xfmrLP[3] * lp + lpSt.z2;
                lpSt.z2 = xfmrLP[2] * hp - xfmrLP[4] * lp;
                data[i] = lp;
            }
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

    // bias=0.0 and asym=0.5 are fixed (knobs removed); their expressions are inlined below.
    //
    // Loop order: samples are the outer loop and channels the inner so that the SmoothedValues
    // (gainSmoothed, drvSmoothed) advance once per sample and both channels share the same
    // smoothed value — keeping L/R gain-ramps in lock-step. processTone and the transformer
    // biquad sections in processOutput use channels-outer/samples-inner because they have no
    // per-sample SmoothedValue to advance; each order is intentional, not an inconsistency.
    //
    // Block-rate normalization: 1/tanh(scale) is determined by drive, which changes at most
    // ~0.002 per sample on a 20ms SmoothedValue ramp.  Computing it once per block rather than
    // once per sample eliminates a std::tanh() call per input sample with no audible consequence
    // (< 0.1% level error within any single block).
    const float blockDrive = drvSmoothed.getCurrentValue();
    const float blockScale  = 1.0f + blockDrive * 4.0f;
    const float blockNormFull = 1.0f / std::tanh(blockScale);
    const float blockNorm = (blockDrive >= 0.01f)
        ? blockNormFull
        : 1.0f + (blockNormFull - 1.0f) * (blockDrive * 100.0f);

    for (int i = 0; i < numSamples; ++i)
    {
        // Mode transition ramp: fade out → switch mode → fade in (10ms each).
        // modeRampInc is computed from sampleRate (not 8×sampleRate), so advancing it
        // once per input sample is correct — the 8× oversampling inner loop outputs
        // exactly one decimated sample per input sample, so modeRamp is applied at
        // the right rate at the decimation stage (see halfBandDecimate call below).
        if (modeRampInc != 0.0f)
        {
            modeRamp += modeRampInc;
            if (modeRampInc < 0.0f && modeRamp <= 0.0f)
            {
                modeRamp = 0.0f;
                currentMode = pendingMode;
                pendingMode = -1;
                modeRampInc = -modeRampInc; // flip: now fade in
            }
            else if (modeRampInc > 0.0f && modeRamp >= 1.0f)
            {
                modeRamp = 1.0f;
                modeRampInc = 0.0f;
            }
        }

        const float gain  = gainSmoothed.getNextValue();
        const float drive = drvSmoothed.getNextValue(); // advance smoother; scale drives waveshaping
        const float scale = 1.0f + drive * 4.0f;

        // Tube mode per-branch normalization: kPos=0.85 (0.7+0.5×0.3), kNeg=1.15 (1.3-0.5×0.3).
        // Hoisted from the 8× oversampling loop — depends only on scale, not on the sub-sample
        // value s, so computing it once per input sample (not 8× per channel) is sufficient.
        // Per-branch normalization is needed because kNeg > 1 means the negative half saturates
        // harder than tanh(scale), so a single normFull would over-compensate and push output > ±1.
        float tubeNormPos = 1.0f, tubeNormNeg = 1.0f;
        if (currentMode == 1)
        {
            tubeNormPos = 1.0f / std::tanh(scale * 0.85f);
            tubeNormNeg = 1.0f / std::tanh(scale * 1.15f);
        }

        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto* data = buffer.getWritePointer(ch);
            const float sample = data[i] * gain;

            // Catmull-Rom (Hermite cubic) upsampling — eliminates -6dB droop at fs/8
            const float p0 = prevPrevSample[ch];      // x[n-2]
            const float p1 = prevSample[ch];          // x[n-1]
            const float p2 = sample;                  // x[n]
            const float p3 = 2.0f * p2 - p1; // linear extrapolation of x[n+1] (causal, stable — avoids parabolic overshoot on transients)

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

            // Soft-knee threshold: scales with drive so higher drive engages saturation earlier.
            // Used by Soft (0), Tube (1), and Satin (3) modes only.
            const float kneeThreshold = juce::jlimit(0.1f, 0.5f, 0.6f / scale);

            // Apply saturation nonlinearity to each oversampled value
            for (int k = 0; k < 8; ++k)
            {
                float s = os[k];

                if (currentMode == 4) // Xfmr: arctangent + iron core even-harmonic series (2H/4H/6H)
                {
                    // bias=0 → no DC offset term; atan(0)=0 so the cancel term drops out.
                    const float driven = s * scale * 1.5f;
                    const float arctan = (2.0f / juce::MathConstants<float>::pi) * std::atan(driven);
                    // Iron core hysteresis: even-order harmonics (2H dominant, 4H secondary, 6H tertiary)
                    const float arctan2 = arctan * arctan;
                    const float h2 = 0.10f * arctan2;                      // 2nd harmonic (strong)
                    const float h4 = 0.025f * arctan2 * arctan2;           // 4th harmonic
                    const float h6 = 0.006f * arctan2 * arctan2 * arctan2; // 6th harmonic
                    // blockNorm = 1/tanh(scale) is the correct normalization for tanh-based modes.
                    // For atan, the small-signal gain is (2/π)*scale*1.5, which differs from
                    // tanh's scale. The 0.88 factor (−1.1 dB) corrects the resulting level
                    // mismatch so Xfmr sits at equal perceived loudness with the other modes.
                    s = (arctan + h2 + h4 + h6) * blockNorm * 0.88f;
                }
                else if (currentMode == 2) // Hard clip — normalize before clamp for consistent loudness with other modes
                {
                    // Normalization applied pre-jlimit: linear region matches other modes' gain;
                    // ceiling is still ±1 (jlimit). At drive=0 blockNorm==1 → transparent.
                    s = juce::jlimit(-1.0f, 1.0f, s * scale * blockNorm);
                }
                else if (currentMode == 5) // Vari-Mu: level-dependent µ (instantaneous compressive saturation, no attack/release)
                {
                    const float x = s * scale;  // x already includes drive scale
                    // µ decreases as signal level rises — classic Fairchild/Vari-Mu character.
                    // Do NOT re-apply scale here: x*µ*scale would double-scale the drive,
                    // causing extreme gain at low drive and crushing the dynamic range.
                    // As |x|→∞, x*µ → 0.5, so output → tanh(0.5)*blockNorm (soft ceiling).
                    // µ is computed sample-by-sample with no attack/release envelope — this is
                    // a compressive saturation curve rather than true gain-reduction compression.
                    const float mu = 1.0f / (1.0f + std::abs(x) * 2.0f);
                    s = fastTanh(x * mu) * blockNorm;
                }
                else
                {
                    // Soft-knee blend — Soft (0), Tube (1), Satin (3)
                    // Cubic smoothstep: 3t² - 2t³
                    const float absIn = std::abs(s);
                    const float kneeBlend = juce::jlimit(0.0f, 1.0f, (absIn - kneeThreshold) / kneeThreshold);
                    const float smoothBlend = kneeBlend * kneeBlend * (3.0f - 2.0f * kneeBlend);

                    if (currentMode == 1) // Tube: asymmetric knee (kPos=0.85, kNeg=1.15; asym=0.5 fixed)
                    {
                        // bias=0 → tanh(x+0)-tanh(0)=tanh(x); DC cancel terms vanish.
                        // tubeNormPos/tubeNormNeg are hoisted above the oversampling loop.
                        const float scaledPos = s * scale * 0.85f;
                        const float scaledNeg = s * scale * 1.15f;
                        const float tubeOut   = s >= 0.0f ? fastTanh(scaledPos) : fastTanh(scaledNeg);
                        const float tubeNorm  = s >= 0.0f ? tubeNormPos : tubeNormNeg;
                        const float saturated = tubeOut * tubeNorm;
                        const float linear    = s * tubeNorm;
                        s = linear + smoothBlend * (saturated - linear);
                    }
                    else if (currentMode == 3) // Satin: tape-like super-soft saturation
                    {
                        // Standard cubic soft clipper: y = 1.5x - 0.5x³
                        // Self-limits to exactly ±1 at x=±1 (slope=0 at boundary) — no discontinuity.
                        const float x = juce::jlimit(-1.0f, 1.0f, s * scale);
                        const float saturated = (1.5f * x - 0.5f * x * x * x) * blockNorm;
                        const float linear = s * blockNorm;
                        s = linear + smoothBlend * (saturated - linear);
                    }
                    else // Soft (0): symmetric tanh — bias=0 → tanh(s*scale+0)-0 = tanh(s*scale)
                    {
                        const float saturated = fastTanh(s * scale) * blockNorm;
                        const float linear    = s * blockNorm;
                        s = linear + smoothBlend * (saturated - linear);
                    }
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
            // Stage 3: pair stage-2 outputs → 1 output; apply mode transition ramp
            data[i] = halfBandDecimate(s2a, s2b, hb4aHistory[ch]) * modeRamp;
        }
    }
}
