#include "StutterEngine.h"

void StutterEngine::prepare(double sr, int /*samplesPerBlock*/)
{
    sampleRate = sr;
    constexpr double kRampSec = 0.02;
    attackSmoothed.reset(sr, kRampSec);
    decaySmoothed.reset(sr, kRampSec);
    sustainSmoothed.reset(sr, kRampSec);
    releaseSmoothed.reset(sr, kRampSec);
    swingSmoothed.reset(sr, kRampSec);
    mixSmoothed.reset(sr, kRampSec);

    currentBpm = 120.0;
    chunks = 16;
    resolutionIndex = 2;
    patternEnabledMask = 0xAAAAAAAAAAAAAAAAull;
    patternTieMask = 0ull;

    attackSmoothed.setCurrentAndTargetValue(0.05f);
    decaySmoothed.setCurrentAndTargetValue(0.20f);
    sustainSmoothed.setCurrentAndTargetValue(1.0f);
    releaseSmoothed.setCurrentAndTargetValue(0.10f);
    swingSmoothed.setCurrentAndTargetValue(0.0f);
    mixSmoothed.setCurrentAndTargetValue(1.0f);

    resetSequence();
}

void StutterEngine::setBPM(double bpm)
{
    currentBpm = juce::jlimit(20.0, 300.0, bpm);
}

void StutterEngine::setChunks(int chunkCount)
{
    chunks = juce::jlimit(2, kPatternSteps, chunkCount);
}

void StutterEngine::setResolution(int resolutionIx)
{
    resolutionIndex = juce::jlimit(0, kResolutionCount - 1, resolutionIx);
}

void StutterEngine::setSwing(float swingNorm)
{
    swingSmoothed.setTargetValue(clampNorm(swingNorm));
}

void StutterEngine::setAttack(float attackNorm)
{
    attackSmoothed.setTargetValue(clampNorm(attackNorm));
}

void StutterEngine::setDecay(float decayNorm)
{
    decaySmoothed.setTargetValue(clampNorm(decayNorm));
}

void StutterEngine::setSustain(float sustainNorm)
{
    sustainSmoothed.setTargetValue(clampNorm(sustainNorm));
}

void StutterEngine::setRelease(float releaseNorm)
{
    releaseSmoothed.setTargetValue(clampNorm(releaseNorm));
}

void StutterEngine::setPattern(uint64_t enabledMask, uint64_t tieMask)
{
    patternEnabledMask = enabledMask;
    // Tie is only meaningful for enabled source steps.
    patternTieMask = tieMask & enabledMask;
}

void StutterEngine::setMix(float mix)
{
    mixSmoothed.setTargetValue(clampNorm(mix));
}

void StutterEngine::resetSequence()
{
    currentStep = 0;
    currentStepDurationSamples = 1;
    stepSamplesRemaining = 1.0;
    sequencePrimed = false;
    gateOpen = false;
    envelopeStage = EnvelopeStage::Idle;
    envelopeValue = 0.0f;
    releaseStep = 0.0f;
}

float StutterEngine::clampNorm(float value)
{
    return juce::jlimit(0.0f, 1.0f, value);
}

bool StutterEngine::isStepEnabled(int step) const
{
    if (step < 0 || step >= chunks)
        return false;

    return (patternEnabledMask & (uint64_t(1) << static_cast<uint32_t>(step))) != 0u;
}

bool StutterEngine::isStepTied(int step) const
{
    if (step < 0 || step >= (chunks - 1))
        return false;

    return (patternTieMask & (uint64_t(1) << static_cast<uint32_t>(step))) != 0u;
}

int StutterEngine::resolveStepDurationSamples(int step, float swingNorm) const
{
    const int safeStep = juce::jlimit(0, juce::jmax(0, chunks - 1), step);
    const size_t resolutionIx = static_cast<size_t>(juce::jlimit(0, 5, resolutionIndex));
    const double stepSeconds = static_cast<double>(kResolutionBeats[resolutionIx]) * 60.0 / currentBpm;

    // Swing offsets every second step while keeping pair duration stable.
    double swingFactor = 1.0;
    if (chunks > 1 && swingNorm > 0.0001f)
        swingFactor = (safeStep & 1) == 0 ? (1.0 - static_cast<double>(swingNorm) * 0.5)
                                          : (1.0 + static_cast<double>(swingNorm) * 0.5);

    const int samples = static_cast<int>(std::round(stepSeconds * swingFactor * sampleRate));
    return juce::jlimit(1, kMaxStepSamples, samples);
}

void StutterEngine::startGateSegment(float attackNorm, float decayNorm, float sustainNorm)
{
    gateOpen = true;

    if (attackNorm <= 0.0001f)
    {
        envelopeValue = 1.0f;
        envelopeStage = decayNorm <= 0.0001f ? EnvelopeStage::Sustain : EnvelopeStage::Decay;
        if (envelopeStage == EnvelopeStage::Sustain)
            envelopeValue = sustainNorm;
    }
    else
    {
        envelopeStage = EnvelopeStage::Attack;
    }
}

void StutterEngine::endGateSegment(float releaseNorm, int stepSamples)
{
    gateOpen = false;

    if (envelopeStage == EnvelopeStage::Idle)
        return;

    if (releaseNorm <= 0.0001f || envelopeValue <= 0.0001f)
    {
        envelopeValue = 0.0f;
        envelopeStage = EnvelopeStage::Idle;
        releaseStep = 0.0f;
        return;
    }

    const int releaseSamples = juce::jlimit(1, kMaxStepSamples,
        static_cast<int>(std::round(static_cast<float>(stepSamples) * releaseNorm)));

    releaseStep = envelopeValue / static_cast<float>(releaseSamples);
    envelopeStage = EnvelopeStage::Release;
}

void StutterEngine::advanceSequenceStep(float attackNorm, float decayNorm,
                                        float sustainNorm, float releaseNorm,
                                        float swingNorm)
{
    const bool hadPreviousStep = sequencePrimed;
    int previousStep = currentStep;

    if (!sequencePrimed)
    {
        currentStep = 0;
        sequencePrimed = true;
    }
    else
    {
        previousStep = currentStep;
        currentStep = (currentStep + 1) % chunks;
    }

    const bool currentEnabled = isStepEnabled(currentStep);

    bool tiedFromPrevious = false;
    if (hadPreviousStep)
    {
        const bool wrapped = (previousStep == chunks - 1) && (currentStep == 0);
        if (!wrapped)
            tiedFromPrevious = isStepEnabled(previousStep) && isStepTied(previousStep) && currentEnabled;
    }

    if (currentEnabled)
    {
        if (!tiedFromPrevious)
            startGateSegment(attackNorm, decayNorm, sustainNorm);
    }
    else
    {
        endGateSegment(releaseNorm, juce::jmax(1, currentStepDurationSamples));
    }

    currentStepDurationSamples = resolveStepDurationSamples(currentStep, swingNorm);
    stepSamplesRemaining += static_cast<double>(currentStepDurationSamples);
}

float StutterEngine::renderEnvelopeSample(float attackNorm, float decayNorm,
                                          float sustainNorm, float releaseNorm)
{
    const int stepSamples = juce::jmax(1, currentStepDurationSamples);
    const int attackSamples = juce::jlimit(1, kMaxStepSamples,
        static_cast<int>(std::round(static_cast<float>(stepSamples) * attackNorm)));
    const int decaySamples = juce::jlimit(1, kMaxStepSamples,
        static_cast<int>(std::round(static_cast<float>(stepSamples) * decayNorm)));
    const int releaseSamples = juce::jlimit(1, kMaxStepSamples,
        static_cast<int>(std::round(static_cast<float>(stepSamples) * releaseNorm)));

    switch (envelopeStage)
    {
        case EnvelopeStage::Idle:
            envelopeValue = 0.0f;
            break;

        case EnvelopeStage::Attack:
        {
            const float attackStep = 1.0f / static_cast<float>(attackSamples);
            envelopeValue = juce::jmin(1.0f, envelopeValue + attackStep);
            if (envelopeValue >= 0.9999f)
                envelopeStage = decayNorm <= 0.0001f ? EnvelopeStage::Sustain : EnvelopeStage::Decay;
            break;
        }

        case EnvelopeStage::Decay:
        {
            const float delta = 1.0f - sustainNorm;
            const float decayStep = delta / static_cast<float>(decaySamples);
            envelopeValue = juce::jmax(sustainNorm, envelopeValue - decayStep);
            if (envelopeValue <= sustainNorm + 0.0001f)
                envelopeStage = EnvelopeStage::Sustain;
            break;
        }

        case EnvelopeStage::Sustain:
            envelopeValue = sustainNorm;
            break;

        case EnvelopeStage::Release:
        {
            // Recompute when release time changes while releasing.
            if (releaseNorm > 0.0001f)
                releaseStep = envelopeValue / static_cast<float>(releaseSamples);

            envelopeValue = juce::jmax(0.0f, envelopeValue - juce::jmax(1.0e-6f, releaseStep));
            if (envelopeValue <= 0.0001f)
            {
                envelopeValue = 0.0f;
                envelopeStage = EnvelopeStage::Idle;
                releaseStep = 0.0f;
            }
            break;
        }
    }

    return juce::jlimit(0.0f, 1.0f, envelopeValue);
}

void StutterEngine::process(juce::AudioBuffer<float>& buffer)
{
    juce::ScopedNoDenormals noDenormals;
    const auto numChannels = std::min(buffer.getNumChannels(), kMaxChannels);
    const auto numSamples = buffer.getNumSamples();

    for (int i = 0; i < numSamples; ++i)
    {
        const float attack = attackSmoothed.getNextValue();
        const float decay = decaySmoothed.getNextValue();
        const float sustain = sustainSmoothed.getNextValue();
        const float release = releaseSmoothed.getNextValue();
        const float swing = swingSmoothed.getNextValue();
        const float mix = mixSmoothed.getNextValue();

        if (!sequencePrimed)
            advanceSequenceStep(attack, decay, sustain, release, swing);

        while (stepSamplesRemaining <= 0.0)
            advanceSequenceStep(attack, decay, sustain, release, swing);

        stepSamplesRemaining -= 1.0;

        const float env = renderEnvelopeSample(attack, decay, sustain, release);

        const float dryGain = std::cos(mix * juce::MathConstants<float>::halfPi);
        const float wetGain = std::sin(mix * juce::MathConstants<float>::halfPi);

        for (int ch = 0; ch < numChannels; ++ch)
        {
            const float dry = buffer.getSample(ch, i);
            const float wet = dry * env;
            buffer.setSample(ch, i, dry * dryGain + wet * wetGain);
        }
    }
}
