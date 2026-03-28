#pragma once
#include <array>
#include <cmath>
#include <mutex>

class GrainEnvelopeLUT
{
public:
    static constexpr int kTableSize = 1024;

    enum Shape { Hanning = 0, Gaussian, Triangle, Trapezoid, kNumShapes };

    static void initialize()
    {
        std::call_once(initFlag, []() {
            const float pi = 3.14159265358979f;
            for (int i = 0; i <= kTableSize; ++i)
            {
                const float phase = static_cast<float>(i) / static_cast<float>(kTableSize);
                tables[Hanning][i] = 0.5f * (1.0f - std::cos(2.0f * pi * phase));
                const float x = (phase - 0.5f) * 6.0f; // ±3σ for proper bell shape
                const float gRaw = std::exp(-0.5f * x * x);
                const float gEdge = std::exp(-4.5f); // value at phase=0 and phase=1 (x=±3)
                tables[Gaussian][i] = (gRaw - gEdge) / (1.0f - gEdge);
                tables[Triangle][i] = 1.0f - std::abs(2.0f * phase - 1.0f);
                constexpr float fadeLen = 0.2f;
                if (phase < fadeLen) tables[Trapezoid][i] = phase / fadeLen;
                else if (phase > (1.0f - fadeLen)) tables[Trapezoid][i] = (1.0f - phase) / fadeLen;
                else tables[Trapezoid][i] = 1.0f;
            }
        });
    }

    static float lookup(int shape, float phase)
    {
        const float pos = phase * static_cast<float>(kTableSize);
        const int idx = static_cast<int>(pos);
        const float frac = pos - static_cast<float>(idx);
        const int safe = (idx < kTableSize) ? idx : kTableSize - 1;
        return tables[shape][safe] * (1.0f - frac) + tables[shape][safe + 1] * frac;
    }

    static float lookupBlended(int shapeA, int shapeB, float blend, float phase)
    {
        return lookup(shapeA, phase) + blend * (lookup(shapeB, phase) - lookup(shapeA, phase));
    }

private:
    static inline std::array<std::array<float, kTableSize + 1>, kNumShapes> tables {};
    static inline std::once_flag initFlag;
};
