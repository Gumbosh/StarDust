#include "DestroyDrive.h"
#include "FastMath.h"
#include "HalfBandDecimator.h"

// Asymmetric saturation (D10): different positive/negative thresholds
// kPos=0.85 (softer on positive), kNeg=1.15 (harder on negative)
static constexpr float kSatKneePos = 0.6f;
static constexpr float kSatKneeNeg = 0.8f;
static constexpr float kInvTanhPos = 1.0f / 0.5370f; // ≈ 1/tanh(0.6)
static constexpr float kInvTanhNeg = 1.0f / 0.6640f; // ≈ 1/tanh(0.8)


void DestroyDrive::prepare(double newSampleRate, int /*samplesPerBlock*/)
{
    if (newSampleRate <= 0.0) return;
    sampleRate = newSampleRate;
    constexpr double rampTimeSec = 0.005;
    inputGainSmoothed.reset(sampleRate, rampTimeSec);
    outputGainSmoothed.reset(sampleRate, rampTimeSec);

    // DC blocker: 1-pole HP at ~8Hz (C4)
    dcBlockCoeff = std::exp(-(juce::MathConstants<float>::twoPi * 8.0f / static_cast<float>(sampleRate)));
    // DC tracking envelope: ~2Hz pole for even-harmonic compensation (C6)
    dcTrackerAlpha = static_cast<float>(1.0 - std::exp(-2.0 * juce::MathConstants<double>::pi * 2.0 / sampleRate));

    for (int ch = 0; ch < kMaxChannels; ++ch)
    {
        prevSample[ch] = 0.0f;
        prevPrevSample[ch] = 0.0f;
        // C5: LR4 crossover state
        xoverLPZ1a[ch] = 0.0f; xoverLPZ2a[ch] = 0.0f;
        xoverLPZ1b[ch] = 0.0f; xoverLPZ2b[ch] = 0.0f;
        xoverHPZ1a[ch] = 0.0f; xoverHPZ2a[ch] = 0.0f;
        xoverHPZ1b[ch] = 0.0f; xoverHPZ2b[ch] = 0.0f;
        dcBlockX1[ch] = 0.0f;
        dcBlockY1[ch] = 0.0f;
        dcTracker[ch] = 0.0f;
        for (int j = 0; j < 8; ++j)
        {
            hb1History[ch][j] = 0.0f;
            hb2History[ch][j] = 0.0f;
            hb3History[ch][j] = 0.0f;
        }
    }

    // C5: LR4 crossover at 500Hz — LP and HP paths (both cascaded 2nd-order Butterworth)
    {
        const float sr = static_cast<float>(sampleRate);
        const float fc = 500.0f;
        const float w0 = juce::MathConstants<float>::twoPi * fc / sr;
        const float cosW0 = std::cos(w0);
        const float sinW0 = std::sin(w0);
        const float alpha = sinW0 / (2.0f * 0.7071f);
        const float a0 = 1.0f + alpha;
        // LP
        xoverLP_B0 = ((1.0f - cosW0) * 0.5f) / a0;
        xoverLP_B1 = (1.0f - cosW0) / a0;
        xoverLP_B2 = xoverLP_B0;
        xoverLP_A1 = (-2.0f * cosW0) / a0;
        xoverLP_A2 = (1.0f - alpha) / a0;
        // HP
        xoverHP_B0 = ((1.0f + cosW0) * 0.5f) / a0;
        xoverHP_B1 = -(1.0f + cosW0) / a0;
        xoverHP_B2 = xoverHP_B0;
        xoverHP_A1 = (-2.0f * cosW0) / a0;
        xoverHP_A2 = (1.0f - alpha) / a0;
    }
}

void DestroyDrive::setInputDrive(float gainDb)
{
    if (currentInputDb != gainDb)
    {
        currentInputDb = gainDb;
        inputGainSmoothed.setTargetValue(dBToLinear(gainDb));
    }
}

void DestroyDrive::setOutputColor(float gainDb)
{
    if (currentOutputDb != gainDb)
    {
        currentOutputDb = gainDb;
        outputGainSmoothed.setTargetValue(dBToLinear(gainDb));
    }
}

void DestroyDrive::processInput(juce::AudioBuffer<float>& buffer)
{
    juce::ScopedNoDenormals noDenormals;
    const auto numChannels = std::min(buffer.getNumChannels(), kMaxChannels);
    const auto numSamples = buffer.getNumSamples();

    for (int i = 0; i < numSamples; ++i)
    {
        const float gain = inputGainSmoothed.getNextValue();

        for (int ch = 0; ch < numChannels; ++ch)
        {
            float& s = buffer.getWritePointer(ch)[i];
            s *= gain;

            // C5: LR4 crossover — cascaded Butterworth LP
            const float lp1 = xoverLP_B0 * s + xoverLPZ1a[ch];
            xoverLPZ1a[ch] = xoverLP_B1 * s - xoverLP_A1 * lp1 + xoverLPZ2a[ch];
            xoverLPZ2a[ch] = xoverLP_B2 * s - xoverLP_A2 * lp1;
            const float lp2 = xoverLP_B0 * lp1 + xoverLPZ1b[ch];
            xoverLPZ1b[ch] = xoverLP_B1 * lp1 - xoverLP_A1 * lp2 + xoverLPZ2b[ch];
            xoverLPZ2b[ch] = xoverLP_B2 * lp1 - xoverLP_A2 * lp2;
            const float bass = lp2;

            // C5: LR4 crossover — cascaded Butterworth HP
            const float hp1 = xoverHP_B0 * s + xoverHPZ1a[ch];
            xoverHPZ1a[ch] = xoverHP_B1 * s - xoverHP_A1 * hp1 + xoverHPZ2a[ch];
            xoverHPZ2a[ch] = xoverHP_B2 * s - xoverHP_A2 * hp1;
            const float hp2 = xoverHP_B0 * hp1 + xoverHPZ1b[ch];
            xoverHPZ1b[ch] = xoverHP_B1 * hp1 - xoverHP_A1 * hp2 + xoverHPZ2b[ch];
            xoverHPZ2b[ch] = xoverHP_B2 * hp1 - xoverHP_A2 * hp2;
            const float treble = hp2;

            // Catmull-Rom upsampling for 4× oversampled saturation (D11)
            const float p0 = prevPrevSample[ch];
            const float p1 = prevSample[ch];
            const float p2 = s;
            const float p3raw = 2.0f * p2 - p1; // linear extrapolation
            const float extMin = std::min(p1, p2) - 0.5f * std::abs(p2 - p1);
            const float extMax = std::max(p1, p2) + 0.5f * std::abs(p2 - p1);
            const float p3 = juce::jlimit(extMin, extMax, p3raw);

            const float ca = -0.5f*p0 + 1.5f*p1 - 1.5f*p2 + 0.5f*p3;
            const float cb =        p0 - 2.5f*p1 + 2.0f*p2 - 0.5f*p3;
            const float cc = -0.5f*p0             + 0.5f*p2;
            const float cd =  p1;

            float os[kOversample];
            for (int k = 0; k < kOversample; ++k)
            {
                const float t = (k + 1) * (1.0f / static_cast<float>(kOversample));
                const float interp = ((ca*t + cb)*t + cc)*t + cd;

                // Asymmetric saturation (D10): softer on positive, harder on negative
                // Frequency-dependent: bass gets softer saturation, treble gets harder (D9)
                // We approximate by biasing the knee based on the bass/treble ratio
                const float bassRatio = (std::abs(s) > 0.001f)
                    ? std::abs(bass) / (std::abs(bass) + std::abs(treble) + 1e-8f)
                    : 0.5f;
                // More bass content → softer knee (less harsh saturation)
                const float kneeBlend = kSatKneePos + (kSatKneeNeg - kSatKneePos) * (1.0f - bassRatio);
                const float invTanhBlend = kInvTanhPos + (kInvTanhNeg - kInvTanhPos) * (1.0f - bassRatio);

                if (interp >= 0.0f)
                    os[k] = FastMath::tanhFast(interp * kneeBlend) * invTanhBlend;
                else
                    os[k] = FastMath::tanhFast(interp * kSatKneeNeg) * kInvTanhNeg;
            }
            prevPrevSample[ch] = prevSample[ch];
            prevSample[ch] = s;

            // 2-stage half-band decimation: 4×→2×→1×
            const float s1a = HalfBand::decimateKaiser(os[0], os[1], hb1History[ch]);
            const float s1b = HalfBand::decimateKaiser(os[2], os[3], hb2History[ch]);
            float decimated = HalfBand::decimateKaiser(s1a, s1b, hb3History[ch]);

            // C6: DC tracking envelope — slow follower removes asymmetric saturation DC offset
            // Preserves sub-bass better than a simple DC blocker alone
            dcTracker[ch] += dcTrackerAlpha * (decimated - dcTracker[ch]);
            decimated -= dcTracker[ch];

            // C4: DC blocker — 1-pole HP at ~8Hz catches any residual DC
            const float dcOut = decimated - dcBlockX1[ch] + dcBlockCoeff * dcBlockY1[ch];
            dcBlockX1[ch] = decimated;
            dcBlockY1[ch] = dcOut;

            s = dcOut;
        }
    }
}

void DestroyDrive::processOutput(juce::AudioBuffer<float>& buffer)
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
}
