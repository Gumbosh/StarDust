#pragma once
#include <cmath>
#include <algorithm>
#include <cstring>
#include "Oscillator.h"

// Dattorro plate reverb (Griesinger topology)
// Reference: Jon Dattorro, "Effect Design Part 1", 1997
// 4 input diffusers → 2 crossed feedback tanks with modulated allpasses
// 14 output taps for stereo decorrelation
class DattorroReverb
{
public:
    DattorroReverb() = default;

    void prepare(double sampleRate);
    void setDecay(float decay);       // 0.0–0.99
    void setDamping(float damping);     // 0.0–1.0 (1=no HF damping, 0=heavy)
    void setDampingLow(float damping);  // 0.0–1.0 (1=no LF damping, 0=heavy)
    void setDiffusion(float amount);  // 0.0–1.0
    void setSize(float s);            // 0–1: small room → large hall (scales LFO rates + decay)
    void setPreDelay(float ms);       // 0–100ms
    void setFreeze(float amount);       // R6: 0.0–1.0 (0=normal, 1=infinite sustain)
    void processSample(float inL, float inR, float& outL, float& outR);
    void reset();

private:
    // Allpass filter helper
    struct Allpass
    {
        float* buffer = nullptr;
        int length = 0;
        int writePos = 0;
        float feedback = 0.0f;

        void init(float* buf, int len, float fb)
        {
            buffer = buf; length = len; feedback = fb; writePos = 0;
        }

        float process(float input)
        {
            const int readPos = (writePos - length + maxDelay) & kMask;
            const float delayed = buffer[readPos];
            const float output = delayed - feedback * input;
            buffer[writePos] = input + feedback * output;
            writePos = (writePos + 1) & kMask;
            return output;
        }

        // Modulated read for tank allpasses — cubic Hermite interpolation (R3)
        float processModulated(float input, float modOffset)
        {
            const float readPosF = static_cast<float>(writePos) - static_cast<float>(length) + modOffset;
            const float wrapped = readPosF + static_cast<float>(maxDelay);
            const int r1 = static_cast<int>(wrapped) & kMask;
            const float frac = wrapped - std::floor(wrapped);
            const int r0 = (r1 - 1 + maxDelay) & kMask;
            const int r2 = (r1 + 1) & kMask;
            const int r3 = (r1 + 2) & kMask;
            const float y0 = buffer[r0];
            const float y1 = buffer[r1];
            const float y2 = buffer[r2];
            const float y3 = buffer[r3];
            // Catmull-Rom cubic — smoother modulation, less metallic artifacts than linear
            const float c1 = 0.5f * (y2 - y0);
            const float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
            const float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
            const float delayed = ((c3 * frac + c2) * frac + c1) * frac + y1;

            const float output = delayed - feedback * input;
            buffer[writePos] = input + feedback * output;
            writePos = (writePos + 1) & kMask;
            return output;
        }

        static constexpr int maxDelay = 32768;
        static constexpr int kMask = maxDelay - 1;
    };

    // Delay line helper
    struct DelayLine
    {
        float* buffer = nullptr;
        int length = 0;
        int writePos = 0;

        void init(float* buf, int len)
        {
            buffer = buf; length = len; writePos = 0;
        }

        void write(float input)
        {
            buffer[writePos] = input;
            writePos = (writePos + 1) & kMask;
        }

        float read() const
        {
            return buffer[(writePos - length + maxDelay) & kMask];
        }

        float readAt(int offset) const
        {
            return buffer[(writePos - offset + maxDelay) & kMask];
        }

        static constexpr int maxDelay = 32768;
        static constexpr int kMask = maxDelay - 1;
    };

    bool prepared = false;
    double sr = 44100.0;
    float decayVal = 0.5f;
    float sizeVal = 0.5f;
    float dampingHFAlpha = 0.5f;  // R1: HF damping coefficient
    float dampingLFMult = 1.0f;   // R1: LF decay multiplier (1.0 = no LF damping)
    float freezeAmount = 0.0f;    // R6: 0.0 = normal, 1.0 = infinite sustain
    float inputDiffusion1 = 0.75f;
    float inputDiffusion2 = 0.625f;
    float modDepthSamples = 8.0f;

    // Pre-delay buffer (20ms, supports up to 192kHz)
    static constexpr int kPreDelaySize = 8192;
    float preDelayBuffer[kPreDelaySize] = {};
    float preDelaySideBuffer[kPreDelaySize] = {};
    int preDelayLength = 0;
    int preDelayWritePos = 0;

    // Input diffusers (4 series allpasses for mid, 2 for side)
    Allpass inDiffA, inDiffB, inDiffC, inDiffD;
    Allpass sideDiffA, sideDiffB, sideDiffC, sideDiffD;

    // Tank A: modulated allpass → delay → damper → allpass → delay
    Allpass tankApA1, tankApA2;
    DelayLine tankDelA1, tankDelA2;
    float tankDampA = 0.0f;
    float tankDampA2 = 0.0f;  // second stage of cascaded damping

    // Tank B: same structure
    Allpass tankApB1, tankApB2;
    DelayLine tankDelB1, tankDelB2;
    float tankDampB = 0.0f;
    float tankDampB2 = 0.0f;

    // Shared incremental oscillator (zero trig per sample)
    IncrementalOscillator lfo1, lfo2;
    int lfoCounter = 0;

    // Output tap offsets (scaled at prepare time)
    int tapA1 = 0, tapA2 = 0, tapA3 = 0, tapA4 = 0, tapA5 = 0, tapA6 = 0, tapA7 = 0;
    int tapB1 = 0, tapB2 = 0, tapB3 = 0, tapB4 = 0, tapB5 = 0, tapB6 = 0, tapB7 = 0;

    // Memory pools for delay buffers
    static constexpr int kPoolSize = Allpass::maxDelay;
    float poolInDiffA[kPoolSize] = {};
    float poolInDiffB[kPoolSize] = {};
    float poolInDiffC[kPoolSize] = {};
    float poolInDiffD[kPoolSize] = {};
    float poolTankApA1[kPoolSize] = {};
    float poolTankApA2[kPoolSize] = {};
    float poolTankDelA1[kPoolSize] = {};
    float poolTankDelA2[kPoolSize] = {};
    float poolTankApB1[kPoolSize] = {};
    float poolTankApB2[kPoolSize] = {};
    float poolTankDelB1[kPoolSize] = {};
    float poolTankDelB2[kPoolSize] = {};
    float poolSideDiffA[kPoolSize] = {};
    float poolSideDiffB[kPoolSize] = {};
    float poolSideDiffC[kPoolSize] = {};
    float poolSideDiffD[kPoolSize] = {};

    // R5: Early reflections: 16 Schroeder-style taps with per-tap LP filtering
    static constexpr int kERBufferSize = 32768; // covers largest tap at up to 192kHz with 1.5x size scale
    float erBuffer[kERBufferSize] = {};
    float erSideBuffer[kERBufferSize] = {};
    int erWritePos = 0;
    int erTaps[16] = {};
    float erTapLP[16] = {};        // per-tap 1-pole LP state
    float erTapLPCoeffs[16] = {};  // per-tap LP coefficient (further taps = darker)
    float erLevel = 0.25f; // updated by setDiffusion()

    // R7: Input pre-filtering — 1-pole HP + LP before diffusers
    float inputHPState = 0.0f;   // 1-pole HP state (~80Hz, removes bass rumble)
    float inputLPState = 0.0f;   // 1-pole LP state (~12kHz, removes sibilance)
    float inputHPCoeff = 0.0f;   // computed in prepare()
    float inputLPCoeff = 0.5f;   // computed in prepare()

    // Output DC blocker: y[n] = x[n] - x[n-1] + R * y[n-1]
    float dcBlockInL = 0.0f, dcBlockInR = 0.0f;   // x[n-1]
    float dcBlockOutL = 0.0f, dcBlockOutR = 0.0f;  // y[n-1]
    float dcBlockR = 0.9993f;

    // Tank DC blocker state (prevents DC accumulation in freeze mode)
    float tankADCState = 0.0f;
    float tankBDCState = 0.0f;
};
