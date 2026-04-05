#include "Saturation.h"
#include "FastMath.h"
#include "HalfBandDecimator.h"

void Saturation::prepare(double newSampleRate, int /*samplesPerBlock*/)
{
    if (newSampleRate <= 0.0) return;

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
    prevMode = 0;
    isMorphing = false;

    // Vari-Mu envelope (D12): ~15ms attack, ~150ms release
    variMuAttack = static_cast<float>(1.0 - std::exp(-2.0 * juce::MathConstants<double>::pi * 70.0 / sampleRate));
    variMuRelease = static_cast<float>(1.0 - std::exp(-2.0 * juce::MathConstants<double>::pi * 7.0 / sampleRate));

    // S4: Program-dependent dynamics envelope (~50ms attack, ~500ms release)
    progAttack = static_cast<float>(1.0 - std::exp(-2.0 * 3.14159265 * 20.0 / sampleRate));
    progRelease = static_cast<float>(1.0 - std::exp(-2.0 * 3.14159265 * 2.0 / sampleRate));

    for (int ch = 0; ch < kMaxChannels; ++ch)
    {
        dcX1[ch] = 0.0f;
        dcY1[ch] = 0.0f;
        variMuEnv[ch] = 0.0f;
        progEnv[ch] = 0.0f;
        xfmrHPState[ch]  = {};
        xfmrLPState[ch]  = {};
        toneHPState[ch]  = {};
        toneLPState[ch]  = {};
        lr4LoStateLP[ch] = {};
        lr4LoStateHP[ch] = {};
        lr4HiStateLP[ch] = {};
        lr4HiStateHP[ch] = {};
        modeEQ1State[ch] = {};
        modeEQ2State[ch] = {};
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

    // S1: Linkwitz-Riley 4th-order crossover coefficients (2nd-order Butterworth LP, Q=0.7071)
    // Computed for both 1x and 8x sample rates
    {
        const float pi = juce::MathConstants<float>::pi;

        // Helper: compute 2nd-order Butterworth LP coefficients at given sample rate
        auto computeLR4LP = [&](float* coeffs, float freqHz, float sr) {
            const float w0 = 2.0f * pi * freqHz / sr;
            const float cosW0 = std::cos(w0);
            const float alpha = std::sin(w0) / (2.0f * 0.7071f);
            const float a0 = 1.0f + alpha;
            coeffs[0] = ((1.0f - cosW0) / 2.0f) / a0;
            coeffs[1] = (1.0f - cosW0) / a0;
            coeffs[2] = coeffs[0];
            coeffs[3] = (-2.0f * cosW0) / a0;
            coeffs[4] = (1.0f - alpha) / a0;
        };

        const float sr1x = static_cast<float>(sampleRate);
        const float sr8x = sr1x * static_cast<float>(kOversample);

        // 1x rate coefficients
        computeLR4LP(lr4Lo, 250.0f, sr1x);
        computeLR4LP(lr4Hi, 3000.0f, sr1x);

        // 8x rate coefficients (same target frequencies, higher sample rate)
        computeLR4LP(lr4Lo8x, 250.0f, sr8x);
        computeLR4LP(lr4Hi8x, 3000.0f, sr8x);
    }

    // Reset 8x LR4 filter states
    for (int ch = 0; ch < kMaxChannels; ++ch)
    {
        lr4LoStateLP8x[ch] = {};
        lr4LoStateHP8x[ch] = {};
        lr4HiStateLP8x[ch] = {};
        lr4HiStateHP8x[ch] = {};
    }

    computeTransformerCoeffs();
    computeModeEQ();
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
    // D14: curve morphing — save old mode, start crossfade
    prevMode = currentMode;
    pendingMode = clamped;
    isMorphing = true;
    modeRamp = 1.0f; // start fully on old mode
    // 10ms fade-out at current sample rate; if already mid-fade, start from current ramp level
    modeRampInc = -1.0f / std::max(1.0f, static_cast<float>(sampleRate) * 0.01f);
    // S2: update per-mode character EQ for the new mode
    // Note: EQ coefficients are updated immediately; the mode crossfade handles the transition
    const int savedMode = currentMode;
    currentMode = clamped;
    computeModeEQ();
    currentMode = savedMode;
}

void Saturation::computeModeEQ()
{
    const float sr = static_cast<float>(sampleRate);
    const float pi = juce::MathConstants<float>::pi;

    // Helper lambda: compute peaking EQ biquad coefficients
    auto computePeak = [&](float* coeffs, float freqHz, float gainDb, float Q) {
        const float A = std::pow(10.0f, gainDb / 40.0f);
        const float w0 = 2.0f * pi * freqHz / sr;
        const float cosW0 = std::cos(w0);
        const float alpha = std::sin(w0) / (2.0f * Q);
        const float a0 = 1.0f + alpha / A;
        coeffs[0] = (1.0f + alpha * A) / a0;
        coeffs[1] = (-2.0f * cosW0) / a0;
        coeffs[2] = (1.0f - alpha * A) / a0;
        coeffs[3] = (-2.0f * cosW0) / a0;
        coeffs[4] = (1.0f - alpha / A) / a0;
    };

    // Helper lambda: compute high shelf biquad coefficients
    auto computeHighShelf = [&](float* coeffs, float freqHz, float gainDb) {
        const float A = std::pow(10.0f, gainDb / 40.0f);
        const float w0 = 2.0f * pi * freqHz / sr;
        const float cosW0 = std::cos(w0);
        const float alpha = std::sin(w0) / (2.0f * 0.707f);
        const float twoSqrtAAlpha = 2.0f * std::sqrt(A) * alpha;
        const float a0 = (A + 1.0f) - (A - 1.0f) * cosW0 + twoSqrtAAlpha;
        coeffs[0] = (A * ((A + 1.0f) + (A - 1.0f) * cosW0 + twoSqrtAAlpha)) / a0;
        coeffs[1] = (-2.0f * A * ((A - 1.0f) + (A + 1.0f) * cosW0)) / a0;
        coeffs[2] = (A * ((A + 1.0f) + (A - 1.0f) * cosW0 - twoSqrtAAlpha)) / a0;
        coeffs[3] = (2.0f * ((A - 1.0f) - (A + 1.0f) * cosW0)) / a0;
        coeffs[4] = ((A + 1.0f) - (A - 1.0f) * cosW0 - twoSqrtAAlpha) / a0;
    };

    // Helper: bypass (flat)
    auto setBypass = [](float* coeffs) {
        coeffs[0] = 1.0f; coeffs[1] = 0.0f; coeffs[2] = 0.0f;
        coeffs[3] = 0.0f; coeffs[4] = 0.0f;
    };

    switch (currentMode)
    {
        case 0: // Soft: bypass
            setBypass(modeEQ1);
            setBypass(modeEQ2);
            break;
        case 1: // Tube: 80Hz peak +1.5dB Q=0.8, 12kHz high shelf -1.5dB
            computePeak(modeEQ1, 80.0f, 1.5f, 0.8f);
            computeHighShelf(modeEQ2, 12000.0f, -1.5f);
            break;
        case 2: // Hard: 2kHz peak +2dB Q=1.0, 8kHz high shelf -3dB
            computePeak(modeEQ1, 2000.0f, 2.0f, 1.0f);
            computeHighShelf(modeEQ2, 8000.0f, -3.0f);
            break;
        case 3: // Satin: 100Hz peak +1dB Q=0.7, 10kHz high shelf +1dB
            computePeak(modeEQ1, 100.0f, 1.0f, 0.7f);
            computeHighShelf(modeEQ2, 10000.0f, 1.0f);
            break;
        case 4: // Xfmr: 60Hz peak +2dB Q=0.6, 15kHz high shelf -2dB
            computePeak(modeEQ1, 60.0f, 2.0f, 0.6f);
            computeHighShelf(modeEQ2, 15000.0f, -2.0f);
            break;
        case 5: // Vari-Mu: 150Hz peak +1dB Q=0.7, 8kHz high shelf -1dB
            computePeak(modeEQ1, 150.0f, 1.0f, 0.7f);
            computeHighShelf(modeEQ2, 8000.0f, -1.0f);
            break;
        default:
            setBypass(modeEQ1);
            setBypass(modeEQ2);
            break;
    }
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

    // S2: Per-mode character EQ (applied post-saturation, pre-DC blocker)
    const auto numChannels = std::min(buffer.getNumChannels(), kMaxChannels);
    const auto numSamples = buffer.getNumSamples();
    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto* data = buffer.getWritePointer(ch);
        auto& eq1 = modeEQ1State[ch];
        auto& eq2 = modeEQ2State[ch];
        for (int i = 0; i < numSamples; ++i)
        {
            // EQ1: peak or low shelf
            const float eq1Out = modeEQ1[0] * data[i] + eq1.z1;
            eq1.z1 = modeEQ1[1] * data[i] - modeEQ1[3] * eq1Out + eq1.z2;
            eq1.z2 = modeEQ1[2] * data[i] - modeEQ1[4] * eq1Out;

            // EQ2: high shelf or peak
            const float eq2Out = modeEQ2[0] * eq1Out + eq2.z1;
            eq2.z1 = modeEQ2[1] * eq1Out - modeEQ2[3] * eq2Out + eq2.z2;
            eq2.z2 = modeEQ2[2] * eq1Out - modeEQ2[4] * eq2Out;

            data[i] = eq2Out;
        }
    }

    // DC blocker: y[n] = x[n] - x[n-1] + R * y[n-1], R = 0.995
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
    // Active in Tube (1), Xfmr (4), and Vari-Mu (5) — all model real transformer/tube hardware (S6).
    if (currentMode == 1 || currentMode == 4 || currentMode == 5)
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


void Saturation::applyGainAndSaturation(juce::AudioBuffer<float>& buffer,
                                         juce::SmoothedValue<float>& gainSmoothed,
                                         juce::SmoothedValue<float>& drvSmoothed)
{
    const auto numChannels = std::min(buffer.getNumChannels(), kMaxChannels);
    const auto numSamples = buffer.getNumSamples();

    // Loop order: samples outer, channels inner — SmoothedValues advance once per sample
    // so L/R gain-ramps stay in lock-step.
    //
    // Block-rate normalization: 1/tanh(scale) depends on drive which changes at most ~0.002
    // per sample on a 20ms ramp.  Computing once per block eliminates a std::tanh() call per
    // input sample with no audible consequence (< 0.1% level error within any single block).
    const float blockDrive = drvSmoothed.getCurrentValue();
    const float blockScale  = 1.0f + blockDrive * 4.0f;
    const float blockNormFull = 1.0f / std::tanh(blockScale);
    const float blockNorm = (blockDrive >= 0.01f)
        ? blockNormFull
        : 1.0f + (blockNormFull - 1.0f) * (blockDrive * 100.0f);

    // S1: LR4 biquad processing helper — applies one 2nd-order section of an LR4 filter
    auto applyBiquad = [](float input, const float* coeffs, float& z1a, float& z2a) -> float {
        const float out = coeffs[0] * input + z1a;
        z1a = coeffs[1] * input - coeffs[3] * out + z2a;
        z2a = coeffs[2] * input - coeffs[4] * out;
        return out;
    };

    // S1: LR4 filter (two cascaded Butterworth sections) helper
    auto applyLR4 = [&applyBiquad](float input, const float* coeffs, LR4State& state) -> float {
        const float stage1 = applyBiquad(input, coeffs, state.z1a, state.z2a);
        return applyBiquad(stage1, coeffs, state.z1b, state.z2b);
    };

    // Per-band drive scale factors
    static constexpr float kBandDriveScale[3] = { 1.15f, 1.0f, 0.75f };

    for (int i = 0; i < numSamples; ++i)
    {
        // Mode transition ramp: fade out -> switch mode -> fade in (10ms each).
        // modeRampInc is computed from sampleRate (not 8x), so advancing once per
        // input sample is correct.
        if (modeRampInc != 0.0f)
        {
            modeRamp += modeRampInc;
            if (modeRampInc < 0.0f && modeRamp <= 0.0f)
            {
                modeRamp = 0.0f;
                currentMode = pendingMode;
                pendingMode = -1;
                modeRampInc = -modeRampInc; // flip: now fade in
                computeModeEQ(); // S2: update EQ when mode actually switches
            }
            else if (modeRampInc > 0.0f && modeRamp >= 1.0f)
            {
                modeRamp = 1.0f;
                modeRampInc = 0.0f;
            }
        }

        // S7: Vari-Mu stereo linking — blend each channel's envelope toward the max
        if (numChannels > 1)
        {
            const float linked = std::max(variMuEnv[0], variMuEnv[1]);
            variMuEnv[0] = variMuEnv[0] * 0.5f + linked * 0.5f;
            variMuEnv[1] = variMuEnv[1] * 0.5f + linked * 0.5f;
        }

        const float gain  = gainSmoothed.getNextValue();
        const float drive = drvSmoothed.getNextValue();
        const float scale = 1.0f + drive * 4.0f;

        // Tube mode per-branch normalization (hoisted — depends only on scale)
        float tubeNormPos = 1.0f, tubeNormNeg = 1.0f;
        if (currentMode == 1)
        {
            tubeNormPos = 1.0f / FastMath::tanh(scale * 0.85f);
            tubeNormNeg = 1.0f / FastMath::tanh(scale * 1.15f);
        }

        // Soft-knee threshold: scales with drive (1x rate, slow follower)
        const float kneeThreshold = juce::jlimit(0.1f, 0.5f, 0.6f / scale);

        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto* data = buffer.getWritePointer(ch);
            const float sample = data[i] * gain;

            // S4: Program-dependent dynamics envelope (1x rate — slow follower)
            const float absIn = std::abs(sample);
            const float progAlpha = (absIn > progEnv[ch]) ? progAttack : progRelease;
            progEnv[ch] += progAlpha * (absIn - progEnv[ch]);

            float modKnee = kneeThreshold;
            if (currentMode == 0)
                modKnee *= (1.0f + progEnv[ch] * 0.08f);
            else if (currentMode == 1)
                modKnee *= (1.0f + progEnv[ch] * 0.15f);
            else if (currentMode == 3 || currentMode == 4)
                modKnee *= (1.0f + progEnv[ch] * 0.12f);

            // Soft-knee blend (1x rate — depends on smoothed drive, not sub-sample values)
            const float kneeBlend = juce::jlimit(0.0f, 1.0f, (absIn - modKnee) / modKnee);
            const float smoothBlend = kneeBlend * kneeBlend * (3.0f - 2.0f * kneeBlend);

            // Catmull-Rom (Hermite cubic) upsampling of raw input to 8x
            const float p0 = prevPrevSample[ch];
            const float p1 = prevSample[ch];
            const float p2 = sample;
            // D15: bounded linear extrapolation — prevents ringing on transients
            const float p3raw = 2.0f * p2 - p1;
            const float extMin = std::min(p1, p2) - 0.5f * std::abs(p2 - p1);
            const float extMax = std::max(p1, p2) + 0.5f * std::abs(p2 - p1);
            const float p3 = juce::jlimit(extMin, extMax, p3raw);

            // Catmull-Rom coefficients (interpolates between p1 and p2)
            const float crA = -0.5f*p0 + 1.5f*p1 - 1.5f*p2 + 0.5f*p3;
            const float crB =        p0 - 2.5f*p1 + 2.0f*p2 - 0.5f*p3;
            const float crC = -0.5f*p0             + 0.5f*p2;
            const float crD =  p1;

            float os[8];
            for (int k = 0; k < 8; ++k)
            {
                const float t = (k + 1) * 0.125f;
                const float subSample = ((crA*t + crB)*t + crC)*t + crD;

                // S1: 3-band multiband split at 8x rate (LR4 crossover)
                const float loLP = applyLR4(subSample, lr4Lo8x, lr4LoStateLP8x[ch]);
                const float loHP = subSample - applyLR4(subSample, lr4Lo8x, lr4LoStateHP8x[ch]);
                const float hiLP = applyLR4(loHP, lr4Hi8x, lr4HiStateLP8x[ch]);
                const float hiHP = loHP - applyLR4(loHP, lr4Hi8x, lr4HiStateHP8x[ch]);
                const float bands[3] = { loLP, hiLP, hiHP };

                float bandResults[3] = {};
                for (int band = 0; band < 3; ++band)
                {
                    // Per-band drive scaling: low=1.15x, mid=1.0x, high=0.75x
                    const float bandScale = scale * kBandDriveScale[band];
                    float s = bands[band];

                    if (currentMode == 4) // Xfmr: arctangent + iron core harmonic series
                    {
                        const float driven = s * bandScale * 1.5f;
                        const float arctan = (2.0f / juce::MathConstants<float>::pi) * FastMath::atan(driven);
                        const float arctan2 = arctan * arctan;
                        const float arctan3 = arctan2 * arctan;
                        const float h2 = 0.10f * arctan2;
                        const float h3 = 0.04f * arctan3;
                        const float h4 = 0.025f * arctan2 * arctan2;
                        const float h5 = 0.01f * arctan2 * arctan3;
                        const float h6 = 0.006f * arctan2 * arctan2 * arctan2;
                        s = (arctan + h2 + h3 + h4 + h5 + h6) * blockNorm * 0.85f;
                    }
                    else if (currentMode == 2) // Hard clip with tighter soft knee (S5)
                    {
                        float clipped = s * bandScale * blockNorm;
                        const float absClipped = std::abs(clipped);
                        if (absClipped > 0.985f)
                        {
                            const float sign = clipped >= 0.0f ? 1.0f : -1.0f;
                            if (absClipped >= 1.0f)
                                clipped = sign;
                            else
                            {
                                const float kt = (absClipped - 0.985f) * (1.0f / 0.015f);
                                clipped = sign * (0.985f + 0.015f * kt * (2.0f - kt));
                            }
                        }
                        s = clipped;
                    }
                    else if (currentMode == 5) // Vari-Mu: envelope-driven compressive saturation
                    {
                        const float x = s * bandScale;
                        const float absX = std::abs(x);
                        const float envAlpha = (absX > variMuEnv[ch]) ? variMuAttack : variMuRelease;
                        variMuEnv[ch] += envAlpha * (absX - variMuEnv[ch]);
                        const float mu = 1.0f / (1.0f + variMuEnv[ch] * 2.0f);
                        s = FastMath::tanh(x * mu) * blockNorm;
                    }
                    else if (currentMode == 1) // Tube: asymmetric knee + 2nd harmonic injection
                    {
                        const float scaledPos = s * bandScale * 0.85f;
                        const float scaledNeg = s * bandScale * 1.15f;
                        const float tubeOut   = s >= 0.0f
                            ? (FastMath::tanh(scaledPos) * tubeNormPos + 0.04f * scaledPos * scaledPos * tubeNormPos)
                            : FastMath::tanh(scaledNeg) * tubeNormNeg;
                        const float tubeNorm  = s >= 0.0f ? tubeNormPos : tubeNormNeg;
                        const float saturated = tubeOut;
                        const float linear    = s * tubeNorm;
                        s = linear + smoothBlend * (saturated - linear);
                    }
                    else if (currentMode == 3) // Satin: Langevin + HF-dependent saturation
                    {
                        // HF energy envelope for frequency-dependent saturation
                        const float hfContent = std::abs(bands[2]);
                        const float hfScale = 1.0f / (1.0f + hfContent * 3.0f);
                        const float satinScale = bandScale * (0.8f + 0.4f * hfScale);

                        const float driven = juce::jlimit(-3.5f, 3.5f, s * satinScale * 1.2f);
                        float langevin;
                        const float ax = std::abs(driven);
                        if (ax < 0.001f)
                            langevin = driven / 3.0f;
                        else
                            langevin = (driven > 0.0f ? 1.0f : -1.0f) * (1.0f / std::tanh(ax) - 1.0f / ax);
                        const float saturated = langevin * 3.0f * blockNorm;
                        const float linear = s * blockNorm;
                        s = linear + smoothBlend * (saturated - linear);
                    }
                    else // Soft (0): symmetric tanh
                    {
                        const float saturated = FastMath::tanh(s * bandScale) * blockNorm;
                        const float linear    = s * blockNorm;
                        s = linear + smoothBlend * (saturated - linear);
                    }

                    bandResults[band] = s;
                }

                // S1: recombine bands at 8x rate
                os[k] = bandResults[0] + bandResults[1] + bandResults[2];
            }

            prevPrevSample[ch] = prevSample[ch];
            prevSample[ch] = sample;

            // 3-stage half-band decimation: 8x -> 4x -> 2x -> 1x
            const float s1a = HalfBand::decimateKaiser(os[0], os[1], hb1History[ch]);
            const float s1b = HalfBand::decimateKaiser(os[2], os[3], hb2History[ch]);
            const float s1c = HalfBand::decimateKaiser(os[4], os[5], hb3History[ch]);
            const float s1d = HalfBand::decimateKaiser(os[6], os[7], hb4History[ch]);
            const float s2a = HalfBand::decimateKaiser(s1a, s1b, hb3aHistory[ch]);
            const float s2b = HalfBand::decimateKaiser(s1c, s1d, hb3bHistory[ch]);
            data[i] = HalfBand::decimateKaiser(s2a, s2b, hb4aHistory[ch]) * modeRamp;
        }
    }
}
