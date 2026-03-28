#pragma once
#include <cmath>

// Shared incremental oscillator: complex-multiply rotation (zero trig per sample)
// Used by TapeEngine, GranularEngine, and DattorroReverb
struct IncrementalOscillator
{
    static constexpr float kTwoPi = 6.28318530718f;

    float cosVal = 1.0f, sinVal = 0.0f;
    float cosInc = 1.0f, sinInc = 0.0f;

    void init(float freq, float sr)
    {
        const float w = kTwoPi * freq / sr;
        cosInc = std::cos(w); sinInc = std::sin(w);
        cosVal = 1.0f; sinVal = 0.0f;
    }

    // Update frequency without resetting phase (use on speed change)
    void setFreq(float freq, float sr)
    {
        const float w = kTwoPi * freq / sr;
        cosInc = std::cos(w); sinInc = std::sin(w);
    }

    void advance()
    {
        const float c = cosVal * cosInc - sinVal * sinInc;
        const float s = sinVal * cosInc + cosVal * sinInc;
        cosVal = c; sinVal = s;
    }

    // Rate-scaled advance: small-angle approximation for scaled rotation (fix 2.17)
    // Uses cos(r*w) ≈ 1 - r²(1-cos(w)), sin(r*w) ≈ r*sin(w)
    // Both components scaled symmetrically to maintain near-unit magnitude
    void advance(float rateScale)
    {
        const float scaledCos = 1.0f - rateScale * rateScale * (1.0f - cosInc);
        const float scaledSin = rateScale * sinInc;
        const float c = cosVal * scaledCos - sinVal * scaledSin;
        const float s = sinVal * scaledCos + cosVal * scaledSin;
        cosVal = c; sinVal = s;
    }

    // Fast renormalization (Newton-Raphson approx near mag=1, no sqrt)
    void normalize()
    {
        const float mag = cosVal * cosVal + sinVal * sinVal;
        const float correction = (3.0f - mag) * 0.5f;
        cosVal *= correction; sinVal *= correction;
    }
};
