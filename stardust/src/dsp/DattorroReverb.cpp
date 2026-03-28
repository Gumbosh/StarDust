#include "DattorroReverb.h"

// Reference delay lengths from Dattorro (1997) at 29761 Hz sample rate
// All delays are scaled proportionally to actual sample rate
static constexpr float kRefRate = 29761.0f;

// Input diffuser delays
static constexpr float kInDiffA = 142.0f;
static constexpr float kInDiffB = 107.0f;
static constexpr float kInDiffC = 379.0f;
static constexpr float kInDiffD = 277.0f;

// Tank A delays
static constexpr float kTankApA1Delay = 672.0f;
static constexpr float kTankDelA1Delay = 4453.0f;
static constexpr float kTankApA2Delay = 1800.0f;
static constexpr float kTankDelA2Delay = 3720.0f;

// Tank B delays
static constexpr float kTankApB1Delay = 908.0f;
static constexpr float kTankDelB1Delay = 4217.0f;
static constexpr float kTankApB2Delay = 2656.0f;
static constexpr float kTankDelB2Delay = 3163.0f;

// Output tap positions (Dattorro Table 1)
static constexpr float kTapDelA1_a = 266.0f;
static constexpr float kTapDelA1_b = 2974.0f;
static constexpr float kTapApA2_a = 1913.0f;
static constexpr float kTapDelA2_a = 1996.0f;
static constexpr float kTapDelB1_a = 1990.0f;
static constexpr float kTapDelB1_b = 187.0f;
static constexpr float kTapApB2_a = 1066.0f;
static constexpr float kTapDelB2_a = 1913.0f;

// Additional taps for stereo decorrelation
static constexpr float kTapDelA1_c = 353.0f;
static constexpr float kTapDelB1_c = 3627.0f;
static constexpr float kTapApA2_b = 1228.0f;
static constexpr float kTapApB2_b = 2673.0f;
static constexpr float kTapDelA2_b = 2111.0f;
static constexpr float kTapDelB2_b = 335.0f;

static int scaleDelay(float refDelay, double sampleRate)
{
    return std::max(1, std::min(32000, static_cast<int>(refDelay * static_cast<float>(sampleRate) / kRefRate)));
}

void DattorroReverb::prepare(double sampleRate)
{
    sr = sampleRate;
    const float twoPi = 2.0f * 3.14159265358979f;

    // Input diffusers
    inDiffA.init(poolInDiffA, scaleDelay(kInDiffA, sr), inputDiffusion1);
    inDiffB.init(poolInDiffB, scaleDelay(kInDiffB, sr), inputDiffusion1);
    inDiffC.init(poolInDiffC, scaleDelay(kInDiffC, sr), inputDiffusion2);
    inDiffD.init(poolInDiffD, scaleDelay(kInDiffD, sr), inputDiffusion2);

    // Tank A
    tankApA1.init(poolTankApA1, scaleDelay(kTankApA1Delay, sr), -0.7f);
    tankDelA1.init(poolTankDelA1, scaleDelay(kTankDelA1Delay, sr));
    tankApA2.init(poolTankApA2, scaleDelay(kTankApA2Delay, sr), 0.5f);
    tankDelA2.init(poolTankDelA2, scaleDelay(kTankDelA2Delay, sr));

    // Tank B
    tankApB1.init(poolTankApB1, scaleDelay(kTankApB1Delay, sr), -0.7f);
    tankDelB1.init(poolTankDelB1, scaleDelay(kTankDelB1Delay, sr));
    tankApB2.init(poolTankApB2, scaleDelay(kTankApB2Delay, sr), 0.5f);
    tankDelB2.init(poolTankDelB2, scaleDelay(kTankDelB2Delay, sr));

    // LFOs for tank modulation
    lfoPhase1 = 0.0f;
    lfoInc1 = twoPi * 0.8f / static_cast<float>(sr);
    lfoPhase2 = 1.57f;
    lfoInc2 = twoPi * 1.1f / static_cast<float>(sr);

    // Scale modulation depth with sample rate (Dattorro reference: 29761 Hz)
    modDepthSamples = 8.0f * static_cast<float>(sampleRate) / 29761.0f;

    // Pre-delay (20ms)
    preDelayLength = std::max(1, static_cast<int>(0.020 * sampleRate));
    preDelayWritePos = 0;

    // Output taps (scaled to sample rate)
    tapA1 = scaleDelay(kTapDelA1_a, sr);
    tapA2 = scaleDelay(kTapDelA1_b, sr);
    tapA3 = scaleDelay(kTapApA2_a, sr);
    tapA4 = scaleDelay(kTapDelA2_a, sr);
    tapA5 = scaleDelay(kTapDelA1_c, sr);
    tapA6 = scaleDelay(kTapApA2_b, sr);
    tapA7 = scaleDelay(kTapDelA2_b, sr);

    tapB1 = scaleDelay(kTapDelB1_a, sr);
    tapB2 = scaleDelay(kTapDelB1_b, sr);
    tapB3 = scaleDelay(kTapApB2_a, sr);
    tapB4 = scaleDelay(kTapDelB2_a, sr);
    tapB5 = scaleDelay(kTapDelB1_c, sr);
    tapB6 = scaleDelay(kTapApB2_b, sr);
    tapB7 = scaleDelay(kTapDelB2_b, sr);

    // Damping filter coefficient
    dampingAlpha = 0.5f;
    tankDampA = 0.0f;
    tankDampB = 0.0f;

    reset();
}

void DattorroReverb::setDecay(float decay)
{
    decayVal = std::max(0.0f, std::min(0.99f, decay));
}

void DattorroReverb::setDamping(float damping)
{
    // damping 1.0 = no damping (bright), 0.0 = heavy damping (dark)
    // LP coefficient: higher = more HF passed through
    dampingAlpha = 0.05f + damping * 0.9f;
}

void DattorroReverb::setDiffusion(float amount)
{
    inputDiffusion1 = 0.3f + amount * 0.45f; // 0.3–0.75
    inputDiffusion2 = 0.2f + amount * 0.425f; // 0.2–0.625
    inDiffA.feedback = inputDiffusion1;
    inDiffB.feedback = inputDiffusion1;
    inDiffC.feedback = inputDiffusion2;
    inDiffD.feedback = inputDiffusion2;
}

void DattorroReverb::reset()
{
    std::memset(poolInDiffA, 0, sizeof(poolInDiffA));
    std::memset(poolInDiffB, 0, sizeof(poolInDiffB));
    std::memset(poolInDiffC, 0, sizeof(poolInDiffC));
    std::memset(poolInDiffD, 0, sizeof(poolInDiffD));
    std::memset(poolTankApA1, 0, sizeof(poolTankApA1));
    std::memset(poolTankApA2, 0, sizeof(poolTankApA2));
    std::memset(poolTankDelA1, 0, sizeof(poolTankDelA1));
    std::memset(poolTankDelA2, 0, sizeof(poolTankDelA2));
    std::memset(poolTankApB1, 0, sizeof(poolTankApB1));
    std::memset(poolTankApB2, 0, sizeof(poolTankApB2));
    std::memset(poolTankDelB1, 0, sizeof(poolTankDelB1));
    std::memset(poolTankDelB2, 0, sizeof(poolTankDelB2));
    tankDampA = 0.0f;
    tankDampB = 0.0f;
    std::memset(preDelayBuffer, 0, sizeof(preDelayBuffer));
    preDelayWritePos = 0;
}

void DattorroReverb::processSample(float inL, float inR, float& outL, float& outR)
{
    // Mix input to mono
    const float inputMono = (inL + inR) * 0.5f;

    // Pre-delay (20ms) for clarity
    preDelayBuffer[preDelayWritePos] = inputMono;
    const float preDelayed = preDelayBuffer[(preDelayWritePos - preDelayLength + 8192) % kPreDelaySize];
    preDelayWritePos = (preDelayWritePos + 1) % kPreDelaySize;

    // Input diffuser chain (4 series allpasses)
    float diffused = inDiffA.process(preDelayed);
    diffused = inDiffB.process(diffused);
    diffused = inDiffC.process(diffused);
    diffused = inDiffD.process(diffused);

    // LFO modulation for tank allpasses
    const float mod1 = std::sin(lfoPhase1) * modDepthSamples;
    const float mod2 = std::sin(lfoPhase2) * modDepthSamples;
    lfoPhase1 += lfoInc1;
    if (lfoPhase1 >= 6.28318530718f) lfoPhase1 -= 6.28318530718f;
    lfoPhase2 += lfoInc2;
    if (lfoPhase2 >= 6.28318530718f) lfoPhase2 -= 6.28318530718f;

    // Read from end of Tank B (crossed feedback into Tank A)
    const float feedbackA = tankDelB2.read();
    // Read from end of Tank A (crossed feedback into Tank B)
    const float feedbackB = tankDelA2.read();

    // Tank A processing
    float tankA = diffused + feedbackA;
    tankA = tankApA1.processModulated(tankA, mod1);
    tankDelA1.write(tankA);
    tankA = tankDelA1.read();
    // Damping LP (1-pole)
    tankDampA += dampingAlpha * (tankA - tankDampA);
    tankA = tankDampA * decayVal;
    tankA = tankApA2.process(tankA);
    tankDelA2.write(tankA);

    // Tank B processing
    float tankB = diffused + feedbackB;
    tankB = tankApB1.processModulated(tankB, mod2);
    tankDelB1.write(tankB);
    tankB = tankDelB1.read();
    // Damping LP
    tankDampB += dampingAlpha * (tankB - tankDampB);
    tankB = tankDampB * decayVal;
    tankB = tankApB2.process(tankB);
    tankDelB2.write(tankB);

    // Output: sum of 14 taps with alternating signs (Dattorro Table 1)
    // Left output: taps from Tank A delay lines + taps from Tank B (subtracted)
    outL = tankDelA1.readAt(tapA1)
         + tankDelA1.readAt(tapA2)
         - tankApA2.buffer[(tankApA2.writePos - tapA3 + kPoolSize) % kPoolSize]
         + tankDelA2.readAt(tapA4)
         - tankDelB1.readAt(tapB1)
         - tankApB2.buffer[(tankApB2.writePos - tapB3 + kPoolSize) % kPoolSize]
         - tankDelB2.readAt(tapB4);

    // Right output: taps from Tank B + taps from Tank A (subtracted)
    outR = tankDelB1.readAt(tapB2)
         + tankDelB1.readAt(tapB5)
         - tankApB2.buffer[(tankApB2.writePos - tapB6 + kPoolSize) % kPoolSize]
         + tankDelB2.readAt(tapB7)
         - tankDelA1.readAt(tapA5)
         - tankApA2.buffer[(tankApA2.writePos - tapA6 + kPoolSize) % kPoolSize]
         - tankDelA2.readAt(tapA7);

    // Scale output
    outL *= 0.35f;
    outR *= 0.35f;
}
