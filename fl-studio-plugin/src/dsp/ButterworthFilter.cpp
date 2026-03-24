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

void ButterworthFilter::recalculateCoefficients(float cutoffVal, float resonanceVal)
{
    // Map 0-99 to frequency: 200 Hz to 20000 Hz (squared curve)
    const float normalized = cutoffVal / 99.0f;
    const float freq = 200.0f + normalized * normalized * 19800.0f;

    // Clamp frequency to Nyquist - margin
    const float nyquist = static_cast<float>(sampleRate) * 0.5f;
    const float clampedFreq = std::min(freq, nyquist * 0.95f);

    // Bilinear transform pre-warp
    const float wc = 2.0f * static_cast<float>(sampleRate)
        * std::tan(juce::MathConstants<float>::pi * clampedFreq
                   / static_cast<float>(sampleRate));

    for (int s = 0; s < kNumSections; ++s)
    {
        float Q = kButterworthQ[s];
        if (s == kNumSections - 1)
            Q = 0.707f + resonanceVal * 8.0f;

        const float K = wc / (2.0f * static_cast<float>(sampleRate));
        const float K2 = K * K;
        const float norm = 1.0f / (1.0f + K / Q + K2);

        const float b0 = K2 * norm;
        const float b1 = 2.0f * b0;
        const float b2 = b0;
        const float a1 = 2.0f * (K2 - 1.0f) * norm;
        const float a2 = (1.0f - K / Q + K2) * norm;

        for (int ch = 0; ch < kMaxChannels; ++ch)
        {
            sections[ch][s].b0 = b0;
            sections[ch][s].b1 = b1;
            sections[ch][s].b2 = b2;
            sections[ch][s].a1 = a1;
            sections[ch][s].a2 = a2;
        }
    }
}

void ButterworthFilter::process(juce::AudioBuffer<float>& buffer)
{
    const float cutoffVal = cutoffSmoothed.getCurrentValue();
    const float lfoDepth = lfoDepthSmoothed.getCurrentValue();

    // Bypass when cutoff is at max (99) and no LFO
    if (cutoffVal >= 98.5f && lfoDepth < 0.001f)
    {
        cutoffSmoothed.skip(buffer.getNumSamples());
        resonanceSmoothed.skip(buffer.getNumSamples());
        lfoRateSmoothed.skip(buffer.getNumSamples());
        lfoDepthSmoothed.skip(buffer.getNumSamples());
        return;
    }

    const auto numChannels = buffer.getNumChannels();
    const auto numSamples = buffer.getNumSamples();

    // Always use sample-by-sample when LFO is active or parameters are smoothing
    const bool needsSampleBySmple = cutoffSmoothed.isSmoothing()
        || resonanceSmoothed.isSmoothing()
        || needsRecalc
        || currentLfoDepth > 0.001f;

    if (needsSampleBySmple)
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
                modulatedCutoff = juce::jlimit(0.0f, 99.0f, c + lfoValue * depth * 40.0f);
                lfoPhase += rate / static_cast<float>(sampleRate);
                if (lfoPhase >= 1.0f)
                    lfoPhase -= 1.0f;
            }

            // Recalculate only when values change meaningfully
            if (std::abs(modulatedCutoff - lastCalcCutoff) > 0.01f
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
