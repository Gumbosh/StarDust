#include "ButterworthFilter.h"

void ButterworthFilter::prepare(double newSampleRate, int /*samplesPerBlock*/)
{
    sampleRate = newSampleRate;
    const double rampTimeSec = 0.02;
    cutoffSmoothed.reset(sampleRate, rampTimeSec);
    resonanceSmoothed.reset(sampleRate, rampTimeSec);
    lfoRateSmoothed.reset(sampleRate, rampTimeSec);
    lfoDepthSmoothed.reset(sampleRate, rampTimeSec);

    for (int ch = 0; ch < kMaxChannels; ++ch)
        for (int s = 0; s < kNumSections; ++s)
            sections[ch][s].reset();

    needsRecalc = true;
    lastCalcCutoff = -1.0f;
    lastCalcResonance = -1.0f;
    lfoPhase = 0.0f;
}

void ButterworthFilter::setCutoff(float cutoffValue)
{
    if (currentCutoff != cutoffValue)
    {
        currentCutoff = cutoffValue;
        cutoffSmoothed.setTargetValue(cutoffValue);
    }
}

void ButterworthFilter::setResonance(float resonance)
{
    if (currentResonance != resonance)
    {
        currentResonance = resonance;
        resonanceSmoothed.setTargetValue(resonance);
    }
}

void ButterworthFilter::setLFO(float rate, float depth)
{
    if (currentLfoRate != rate) { currentLfoRate = rate; lfoRateSmoothed.setTargetValue(rate); }
    if (currentLfoDepth != depth) { currentLfoDepth = depth; lfoDepthSmoothed.setTargetValue(depth); }
}

void ButterworthFilter::setFilterType(int type)
{
    if (filterType != type)
    {
        filterType = type;
        needsRecalc = true;
        for (int ch = 0; ch < kMaxChannels; ++ch)
            for (int s = 0; s < kNumSections; ++s)
                sections[ch][s].reset();
    }
}

void ButterworthFilter::recalculateCoefficients(float cutoffVal, float resonanceVal)
{
    // Map 0-1 to frequency: 200 Hz to 20000 Hz (logarithmic curve)
    const float freq = 200.0f * std::exp2f(cutoffVal * 6.643856f); // log2(100) = 6.643856

    const float nyquist = static_cast<float>(sampleRate) * 0.5f;
    const float clampedFreq = std::min(freq, nyquist * 0.95f);

    // Bilinear transform pre-warp
    const float K = std::tan(juce::MathConstants<float>::pi * clampedFreq
                             / static_cast<float>(sampleRate));
    const float K2 = K * K;

    // D20: Cap maximum resonance Q at 6.0 for stability
    const float cappedResonance = std::min(resonanceVal, 0.75f); // maps to Q max ~6.0

    if (filterType == 0) // LP: 3 cascaded biquad sections
    {
        for (int s = 0; s < kNumSections; ++s)
        {
            // D17: distribute resonance across all pole pairs (proportional scaling)
            const float resScale = 1.0f + cappedResonance * 6.0f; // 1.0 → 5.5
            float Q = kButterworthQ[s] * resScale;

            const float norm = 1.0f / (1.0f + K / Q + K2);
            const float b0 = K2 * norm;
            const float b1 = 2.0f * b0;
            const float b2 = b0;
            const float a1 = 2.0f * (K2 - 1.0f) * norm;
            const float a2 = (1.0f - K / Q + K2) * norm;

            for (int ch = 0; ch < kMaxChannels; ++ch)
            {
                sections[ch][s].b0 = b0; sections[ch][s].b1 = b1; sections[ch][s].b2 = b2;
                sections[ch][s].a1 = a1; sections[ch][s].a2 = a2;
            }
        }
    }
    else if (filterType == 1) // HP: 3 cascaded biquad sections
    {
        for (int s = 0; s < kNumSections; ++s)
        {
            // D17: distribute resonance
            const float resScale = 1.0f + cappedResonance * 6.0f;
            float Q = kButterworthQ[s] * resScale;

            const float norm = 1.0f / (1.0f + K / Q + K2);
            const float b0 = norm;
            const float b1 = -2.0f * norm;
            const float b2 = norm;
            const float a1 = 2.0f * (K2 - 1.0f) * norm;
            const float a2 = (1.0f - K / Q + K2) * norm;

            for (int ch = 0; ch < kMaxChannels; ++ch)
            {
                sections[ch][s].b0 = b0; sections[ch][s].b1 = b1; sections[ch][s].b2 = b2;
                sections[ch][s].a1 = a1; sections[ch][s].a2 = a2;
            }
        }
    }
    else if (filterType == 2) // BP: 6th-order (D18: all 3 sections active)
    {
        for (int s = 0; s < kNumSections; ++s)
        {
            // D17 + D18: distribute Q across all 3 sections for 6th-order BP
            const float baseQ = 0.5f + cappedResonance * 6.0f;
            const float Q = kButterworthQ[s] * (baseQ / 0.7071f); // scale relative to Butterworth unity

            const float norm = 1.0f / (1.0f + K / Q + K2);
            const float b0 = (K / Q) * norm;
            const float b1 = 0.0f;
            const float b2 = -b0;
            const float a1 = 2.0f * (K2 - 1.0f) * norm;
            const float a2 = (1.0f - K / Q + K2) * norm;

            for (int ch = 0; ch < kMaxChannels; ++ch)
            {
                sections[ch][s].b0 = b0; sections[ch][s].b1 = b1; sections[ch][s].b2 = b2;
                sections[ch][s].a1 = a1; sections[ch][s].a2 = a2;
            }
        }
    }
    else // Notch: 6th-order (D18: all 3 sections active)
    {
        for (int s = 0; s < kNumSections; ++s)
        {
            const float baseQ = 0.5f + cappedResonance * 6.0f;
            const float Q = kButterworthQ[s] * (baseQ / 0.7071f);

            const float norm = 1.0f / (1.0f + K / Q + K2);
            const float b0 = (1.0f + K2) * norm;
            const float b1 = 2.0f * (K2 - 1.0f) * norm;
            const float b2 = b0;
            const float a1 = b1;
            const float a2 = (1.0f - K / Q + K2) * norm;

            for (int ch = 0; ch < kMaxChannels; ++ch)
            {
                sections[ch][s].b0 = b0; sections[ch][s].b1 = b1; sections[ch][s].b2 = b2;
                sections[ch][s].a1 = a1; sections[ch][s].a2 = a2;
            }
        }
    }
}

void ButterworthFilter::process(juce::AudioBuffer<float>& buffer)
{
    juce::ScopedNoDenormals noDenormals;
    const float cutoffVal = cutoffSmoothed.getCurrentValue();
    const float lfoDepth = lfoDepthSmoothed.getCurrentValue();

    // Bypass when LP cutoff is near max and no LFO (LP only — HP/BP/Notch always active)
    if (filterType == 0 && cutoffVal >= 0.985f && lfoDepth < 0.001f)
    {
        cutoffSmoothed.skip(buffer.getNumSamples());
        resonanceSmoothed.skip(buffer.getNumSamples());
        lfoRateSmoothed.skip(buffer.getNumSamples());
        lfoDepthSmoothed.skip(buffer.getNumSamples());
        return;
    }

    const auto numChannels = std::min(buffer.getNumChannels(), kMaxChannels);
    const auto numSamples = buffer.getNumSamples();

    // Always use sample-by-sample when LFO is active or parameters are smoothing
    const bool needsSampleBySample = cutoffSmoothed.isSmoothing()
        || resonanceSmoothed.isSmoothing()
        || needsRecalc
        || currentLfoDepth > 0.001f;

    if (needsSampleBySample)
    {
        for (int i = 0; i < numSamples; ++i)
        {
            const float c = cutoffSmoothed.getNextValue();
            const float r = resonanceSmoothed.getNextValue();
            const float rate = lfoRateSmoothed.getNextValue();
            const float depth = lfoDepthSmoothed.getNextValue();

            // Triangle LFO: 4 * |phase - 0.5| - 1 → range [-1, +1]
            float modulatedCutoff = c;
            if (depth > 0.001f)
            {
                const float lfoValue = 4.0f * std::abs(lfoPhase - 0.5f) - 1.0f;
                modulatedCutoff = juce::jlimit(0.0f, 1.0f, c + lfoValue * depth * 0.4f);
                lfoPhase += rate / static_cast<float>(sampleRate);
                if (lfoPhase >= 1.0f)
                    lfoPhase -= 1.0f;
            }

            // Recalculate only when values change meaningfully
            if (std::abs(modulatedCutoff - lastCalcCutoff) > 0.002f
                || std::abs(r - lastCalcResonance) > 0.001f)
            {
                lastCalcCutoff = modulatedCutoff;
                lastCalcResonance = r;
                recalculateCoefficients(modulatedCutoff, r);
            }

            for (int ch = 0; ch < numChannels; ++ch)
            {
                auto* data = buffer.getWritePointer(ch);
                float sample = data[i];
                for (int s = 0; s < kNumSections; ++s)
                    sample = sections[ch][s].process(sample);
                data[i] = sample;
            }
        }
        needsRecalc = false;
    }
    else
    {
        // Coefficients are stable, no LFO — process in bulk
        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto* data = buffer.getWritePointer(ch);
            for (int i = 0; i < numSamples; ++i)
            {
                float sample = data[i];
                for (int s = 0; s < kNumSections; ++s)
                    sample = sections[ch][s].process(sample);
                data[i] = sample;
            }
        }
    }
}
