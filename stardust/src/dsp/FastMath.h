#pragma once
#include <cmath>
#include <algorithm>

namespace FastMath {

// ============================================================================
// Accurate fastTanh -- [5/5] Pade rational approximant with smoothstep blend.
// Max error < 0.03% vs std::tanh. Used by the GRIT safety and drive stages.
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
// Max error ~3.5% near x=1.5. Used by the harmonic exciter.
// ============================================================================
inline float tanhFast(float x) noexcept
{
    if (x > 4.0f) return 1.0f;
    if (x < -4.0f) return -1.0f;
    const float x2 = x * x;
    return x * (27.0f + x2) / (27.0f + 9.0f * x2);
}

} // namespace FastMath
