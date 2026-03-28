#pragma once
#include <cmath>
#include <algorithm>

// Asymmetric peak envelope follower for grain amplitude modulation
// Fast attack (2ms) catches transients, slow release (50ms) creates smooth decay
class InputEnvelopeFollower
{
public:
    void prepare(double sampleRate)
    {
        const float twoPi = 2.0f * 3.14159265358979f;
        const float sr = static_cast<float>(sampleRate);
        attackAlpha = 1.0f - std::exp(-twoPi * 500.0f / sr);   // ~2ms attack
        releaseAlpha = 1.0f - std::exp(-twoPi * 20.0f / sr);   // ~50ms release
        envelope = 0.0f;
    }

    void processSample(float inputL, float inputR)
    {
        const float peak = std::max(std::abs(inputL), std::abs(inputR));
        const float alpha = (peak > envelope) ? attackAlpha : releaseAlpha;
        envelope += alpha * (peak - envelope);
    }

    float getEnvelope() const { return envelope; }

private:
    float envelope = 0.0f;
    float attackAlpha = 0.1f;
    float releaseAlpha = 0.01f;
};
