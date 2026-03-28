#pragma once
#include "Oscillator.h"
#include <cmath>
#include <cstdint>

// Modulation targets — indices into the target parameter array
enum class ModTarget : int
{
    GrainCloud = 0, GrainScatter, GrainReverse, GrainPosition,
    GrainSpace, GrainMorph, GrainMix,
    TapeWow, TapeFlutter, TapeDrive, TapeBias, TapeMix,
    kNumTargets
};

class ModulationMatrix
{
public:
    static constexpr int kNumSlots = 4;
    static constexpr int kNumLFOs = 2;
    static constexpr int kNumTargets = static_cast<int>(ModTarget::kNumTargets);

    // Waveforms: 0=sine, 1=triangle, 2=saw, 3=random (S&H)
    struct LFOState
    {
        IncrementalOscillator osc;
        float rate = 1.0f;
        float depth = 0.0f;
        int waveform = 0;
        bool tempoSync = false;
        float output = 0.0f;
        // S&H state
        float shValue = 0.0f;
        int shCounter = 0;
        int shInterval = 4410; // samples between S&H triggers
        float sawPhase = 0.0f;
        float triPhase = 0.0f;
        uint32_t noiseState = 99991;
    };

    struct Slot
    {
        int sourceIndex = -1;   // -1=none, 0=LFO1, 1=LFO2, 2=EnvFollower
        int targetIndex = -1;   // ModTarget enum value
        float amount = 0.0f;    // bipolar: -1 to +1
    };

    void prepare(double sampleRate)
    {
        sr = sampleRate;
        for (int i = 0; i < kNumLFOs; ++i)
        {
            lfos[i].osc.init(lfos[i].rate, static_cast<float>(sr));
            lfos[i].shInterval = std::max(1, static_cast<int>(sr / std::max(0.01f, lfos[i].rate)));
        }
        normCounter = 0;
    }

    void setLFO(int index, float rate, float depth, int waveform, bool sync)
    {
        if (index < 0 || index >= kNumLFOs) return;
        auto& lfo = lfos[index];
        if (lfo.rate != rate)
        {
            lfo.rate = rate;
            lfo.osc.setFreq(rate, static_cast<float>(sr));
            lfo.shInterval = std::max(1, static_cast<int>(sr / std::max(0.01f, rate)));
        }
        if (lfo.waveform != waveform)
        {
            const float phase = std::atan2(lfo.osc.sinVal, lfo.osc.cosVal) / 3.14159265f;
            if (waveform == 1) lfo.triPhase = phase;
            if (waveform == 2) lfo.sawPhase = phase;
        }
        lfo.depth = depth;
        lfo.waveform = waveform;
        lfo.tempoSync = sync;
    }

    void setSlot(int slotIdx, int source, int target, float amount)
    {
        if (slotIdx < 0 || slotIdx >= kNumSlots) return;
        slots[slotIdx] = { source, target, amount };
    }

    void setEnvFollower(float value) { envFollowerValue = value; }

    // Advance all LFOs by one sample
    void processSample()
    {
        for (int i = 0; i < kNumLFOs; ++i)
        {
            auto& lfo = lfos[i];
            lfo.osc.advance();

            switch (lfo.waveform)
            {
                case 0: // Sine
                    lfo.output = lfo.osc.sinVal;
                    break;
                case 1: // Triangle (phase accumulator — zero trig per sample)
                {
                    lfo.triPhase += lfo.rate / static_cast<float>(sr);
                    if (lfo.triPhase >= 1.0f) lfo.triPhase -= 2.0f;
                    lfo.output = 1.0f - 2.0f * std::abs(lfo.triPhase);
                    break;
                }
                case 2: // Saw (phase accumulator — zero trig per sample)
                {
                    lfo.sawPhase += 2.0f * lfo.rate / static_cast<float>(sr);
                    if (lfo.sawPhase > 1.0f) lfo.sawPhase -= 2.0f;
                    lfo.output = lfo.sawPhase;
                    break;
                }
                case 3: // Random (S&H)
                    if (++lfo.shCounter >= lfo.shInterval)
                    {
                        lfo.shCounter = 0;
                        lfo.noiseState ^= lfo.noiseState << 13;
                        lfo.noiseState ^= lfo.noiseState >> 17;
                        lfo.noiseState ^= lfo.noiseState << 5;
                        lfo.shValue = static_cast<float>(lfo.noiseState) / static_cast<float>(UINT32_MAX) * 2.0f - 1.0f;
                    }
                    lfo.output = lfo.shValue;
                    break;
                default:
                    lfo.output = lfo.osc.sinVal;
            }
            lfo.output *= lfo.depth;
        }

        // Normalize oscillators periodically
        if (++normCounter >= 512)
        {
            normCounter = 0;
            for (int i = 0; i < kNumLFOs; ++i)
                lfos[i].osc.normalize();
        }
    }

    // Get total modulation offset for a target parameter
    float getModOffset(int targetIdx) const
    {
        float total = 0.0f;
        for (int s = 0; s < kNumSlots; ++s)
        {
            const auto& slot = slots[s];
            if (slot.targetIndex != targetIdx || slot.sourceIndex < 0) continue;

            float sourceVal = 0.0f;
            if (slot.sourceIndex < kNumLFOs)
                sourceVal = lfos[slot.sourceIndex].output;
            else if (slot.sourceIndex == kNumLFOs) // envelope follower
                sourceVal = envFollowerValue * 2.0f - 1.0f; // 0-1 → -1 to +1

            total += sourceVal * slot.amount;
        }
        return total;
    }

    // Apply modulation to a base value, clamped to [0, 1]
    float modulate(ModTarget target, float baseValue) const
    {
        const float offset = getModOffset(static_cast<int>(target));
        return std::max(0.0f, std::min(1.0f, baseValue + offset));
    }

private:
    double sr = 44100.0;
    LFOState lfos[kNumLFOs];
    Slot slots[kNumSlots];
    float envFollowerValue = 0.0f;
    int normCounter = 0;
};
