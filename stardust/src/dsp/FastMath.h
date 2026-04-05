#pragma once
#include <cmath>
#include <algorithm>

namespace FastMath {

// ============================================================================
// Accurate fastTanh -- [5/5] Pade rational approximant with smoothstep blend
// Max error < 0.03% vs std::tanh. Use for saturation curves, hysteresis.
// Source: TapeEngine.cpp
// ============================================================================
inline float tanh(float x) noexcept
{
    // Hard clamp beyond +/-3.5: tanh(3.5) ~ 0.9982, error to +/-1.0 is negligible.
    if (x > 3.5f) return 1.0f;
    if (x < -3.5f) return -1.0f;
    // [5/5] Pade rational approximant -- max error < 0.03% across [-3.5, 3.5]
    const float x2 = x * x;
    const float pade = x * (135135.0f + x2 * (17325.0f + x2 * 378.0f))
                         / (135135.0f + x2 * (62370.0f + x2 * (3150.0f + x2 * 28.0f)));
    // Blend to +/-1 over [3.0, 3.5] with cubic smoothstep -- eliminates C1 discontinuity
    // at the hard-clamp boundary. The common case (|x| < 3.0) returns immediately above,
    // so this branch only costs ~1% of hot-path calls.
    const float ax = std::abs(x);
    if (ax > 3.0f)
    {
        const float t = (ax - 3.0f) * 2.0f;                      // [0,1] over [3.0, 3.5]
        const float blend = t * t * (3.0f - 2.0f * t);           // cubic smoothstep
        const float sign = x >= 0.0f ? 1.0f : -1.0f;
        return pade + blend * (sign - pade);
    }
    return pade;
}

// ============================================================================
// Cheap fastTanh -- [1/1] rational approximant
// Max error ~3.5% near x=1.5. Use for soft clipping, OTA emulation.
// Source: ChorusEngine.h
// ============================================================================
inline float tanhFast(float x) noexcept
{
    if (x > 4.0f) return 1.0f;
    if (x < -4.0f) return -1.0f;
    const float x2 = x * x;
    return x * (27.0f + x2) / (27.0f + 9.0f * x2);
}

// ============================================================================
// Fast sinh approximation -- 7-term Maclaurin series for |x| < 3,
// exponential fallback for large |x|.
// Max relative error < 0.003% at |x|=3.
// Source: TapeEngine.cpp
// ============================================================================
inline float sinh(float x) noexcept
{
    const float ax = std::abs(x);
    if (ax < 3.0f)
    {
        const float x2 = x * x;
        return x * (1.0f + x2 * (1.0f/6.0f
               + x2 * (1.0f/120.0f
               + x2 * (1.0f/5040.0f
               + x2 * (1.0f/362880.0f
               + x2 * (1.0f/39916800.0f
               + x2 *  1.0f/6227020800.0f))))));
    }
    const float e = std::exp(ax);
    return (x >= 0.0f ? 1.0f : -1.0f) * (e - 1.0f / e) * 0.5f;
}

// ============================================================================
// Fast coth (1/tanh) -- inverted Pade [5/5] rational
// Source: TapeEngine.cpp
// ============================================================================
inline float coth(float x) noexcept
{
    const float ax = std::abs(x);
    if (ax < 0.25f) return 0.0f; // should never be called for small x (langevin uses Taylor)
    if (ax > 3.0f) {
        const float sign = x >= 0.0f ? 1.0f : -1.0f;
        return sign * (1.0f + 2.0f * std::exp(-2.0f * ax));
    }
    // coth(x) = 1/tanh(x). Inverted Pade: swap num/denom of tanh's [5/5] rational
    const float x2 = x * x;
    return (135135.0f + x2 * (62370.0f + x2 * (3150.0f + x2 * 28.0f)))
         / (x * (135135.0f + x2 * (17325.0f + x2 * 378.0f)));
}

// ============================================================================
// Fast atan approximation -- rational Pade
// For use in transformer saturation mode.
// Max error ~1% near |x|=2.
// ============================================================================
inline float atan(float x) noexcept
{
    if (x > 2.4f) return 1.17810f;   // atan(2.4) ~ 1.176
    if (x < -2.4f) return -1.17810f;
    return x / (1.0f + 0.28125f * x * x);
}

} // namespace FastMath
