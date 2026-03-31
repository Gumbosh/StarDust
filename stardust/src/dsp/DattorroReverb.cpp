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

// Early reflection tap delays (at 29761 Hz reference, mutually prime for Schroeder spacing)
static constexpr float kERRef[8] = { 149.0f, 214.0f, 318.0f, 419.0f, 529.0f, 631.0f, 764.0f, 839.0f };

// Additional taps for stereo decorrelation
static constexpr float kERGains[8] = { 0.70f, -0.62f, 0.55f, -0.48f, 0.41f, -0.36f, 0.30f, -0.25f };
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

    // Input diffusers (mid: 4 series, side: 2 series for stereo content)
    inDiffA.init(poolInDiffA, scaleDelay(kInDiffA, sr), inputDiffusion1);
    inDiffB.init(poolInDiffB, scaleDelay(kInDiffB, sr), inputDiffusion1);
    inDiffC.init(poolInDiffC, scaleDelay(kInDiffC, sr), inputDiffusion2);
    inDiffD.init(poolInDiffD, scaleDelay(kInDiffD, sr), inputDiffusion2);
    sideDiffA.init(poolSideDiffA, scaleDelay(kInDiffC, sr), inputDiffusion2);
    sideDiffB.init(poolSideDiffB, scaleDelay(kInDiffD, sr), inputDiffusion2);

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

    // Incremental oscillators for tank modulation (zero trig per sample)
    lfo1.init(0.8f, static_cast<float>(sr));
    lfo2.init(1.1f, static_cast<float>(sr));
    // Offset lfo2 phase by ~90 degrees
    lfo2.cosVal = 0.0f; lfo2.sinVal = 1.0f;
    lfoCounter = 0;

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

    // Early reflection tap lengths (scaled to sample rate)
    for (int i = 0; i < 8; ++i)
        erTaps[i] = std::max(1, std::min(kERBufferSize - 1,
            static_cast<int>(kERRef[i] * static_cast<float>(sr) / kRefRate)));
    erWritePos = 0;

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

void DattorroReverb::setSize(float s)
{
    sizeVal = std::max(0.0f, std::min(1.0f, s));
    // Size only controls LFO speed and modulation depth — decay is independent
    lfo1.setFreq(0.4f + sizeVal * 1.2f, static_cast<float>(sr));
    lfo2.setFreq(0.6f + sizeVal * 1.4f, static_cast<float>(sr));
    modDepthSamples = (4.0f + sizeVal * 8.0f) * static_cast<float>(sr) / kRefRate;
}

void DattorroReverb::setPreDelay(float ms)
{
    ms = std::max(0.0f, std::min(100.0f, ms));
    preDelayLength = std::max(1, std::min(kPreDelaySize - 1,
        static_cast<int>(ms * 0.001f * static_cast<float>(sr))));
}

void DattorroReverb::setDiffusion(float amount)
{
    inputDiffusion1 = 0.3f + amount * 0.45f; // 0.3–0.75
    inputDiffusion2 = 0.2f + amount * 0.425f; // 0.2–0.625
    inDiffA.feedback = inputDiffusion1;
    inDiffB.feedback = inputDiffusion1;
    inDiffC.feedback = inputDiffusion2;
    inDiffD.feedback = inputDiffusion2;
    sideDiffA.feedback = inputDiffusion2;
    sideDiffB.feedback = inputDiffusion2;
    erLevel = 0.10f + amount * 0.25f; // 0.10–0.35 range
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
    std::memset(poolSideDiffA, 0, sizeof(poolSideDiffA));
    std::memset(poolSideDiffB, 0, sizeof(poolSideDiffB));
    tankDampA = 0.0f;
    tankDampB = 0.0f;
    std::memset(preDelayBuffer, 0, sizeof(preDelayBuffer));
    preDelayWritePos = 0;
    std::memset(erBuffer, 0, sizeof(erBuffer));
    erWritePos = 0;
}

void DattorroReverb::processSample(float inL, float inR, float& outL, float& outR)
{
    // True stereo: M/S decomposition, diffuse both mid and side before tanks
    const float inputMid = (inL + inR) * 0.5f;
    const float inputSide = (inL - inR) * 0.5f;

    preDelayBuffer[preDelayWritePos] = inputMid;
    const float preDelayed = preDelayBuffer[(preDelayWritePos - preDelayLength + kPreDelaySize) & (kPreDelaySize - 1)];
    preDelayWritePos = (preDelayWritePos + 1) & (kPreDelaySize - 1);

    // Early reflections: 8 Schroeder-style taps from pre-delayed mono mid
    // Even-indexed taps → L, odd-indexed taps → R (alternating signs for stereo spread)
    erBuffer[erWritePos] = preDelayed;
    float erL = 0.0f, erR = 0.0f;
    for (int i = 0; i < 8; ++i)
    {
        const float tap = erBuffer[(erWritePos - erTaps[i] + kERBufferSize) & (kERBufferSize - 1)];
        if (i & 1) erR += kERGains[i] * tap;
        else       erL += kERGains[i] * tap;
    }
    erWritePos = (erWritePos + 1) & (kERBufferSize - 1);

    // Diffuse mid (4 allpasses) and side (2 allpasses) independently
    float diffusedMid = inDiffA.process(preDelayed);
    diffusedMid = inDiffB.process(diffusedMid);
    diffusedMid = inDiffC.process(diffusedMid);
    diffusedMid = inDiffD.process(diffusedMid);
    float diffusedSide = sideDiffA.process(inputSide);
    diffusedSide = sideDiffB.process(diffusedSide);

    // LFO modulation via incremental oscillators (zero trig per sample)
    const float mod1 = lfo1.sinVal * modDepthSamples;
    const float mod2 = lfo2.sinVal * modDepthSamples;
    lfo1.advance();
    lfo2.advance();
    if (++lfoCounter >= 512) { lfoCounter = 0; lfo1.normalize(); lfo2.normalize(); }

    // Read from end of Tank B (crossed feedback into Tank A)
    const float feedbackA = tankDelB2.read();
    // Read from end of Tank A (crossed feedback into Tank B)
    const float feedbackB = tankDelA2.read();

    // True stereo: inject diffused mid + diffused side into separate tanks
    float tankA = diffusedMid + diffusedSide + feedbackA;
    tankA = tankApA1.processModulated(tankA, mod1);
    tankDelA1.write(tankA);
    tankA = tankDelA1.read();
    // Damping LP (1-pole)
    tankDampA += dampingAlpha * (tankA - tankDampA);
    tankA = tankDampA * decayVal;
    tankA = tankApA2.process(tankA);
    tankDelA2.write(tankA);

    // Tank B processing (mid - side = R-biased)
    float tankB = diffusedMid - diffusedSide + feedbackB;
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
         - tankApA2.buffer[(tankApA2.writePos - tapA3 + kPoolSize) & (kPoolSize - 1)]
         + tankDelA2.readAt(tapA4)
         - tankDelB1.readAt(tapB1)
         - tankApB2.buffer[(tankApB2.writePos - tapB3 + kPoolSize) & (kPoolSize - 1)]
         - tankDelB2.readAt(tapB4);

    // Right output: taps from Tank B + taps from Tank A (subtracted)
    outR = tankDelB1.readAt(tapB2)
         + tankDelB1.readAt(tapB5)
         - tankApB2.buffer[(tankApB2.writePos - tapB6 + kPoolSize) & (kPoolSize - 1)]
         + tankDelB2.readAt(tapB7)
         - tankDelA1.readAt(tapA5)
         - tankApA2.buffer[(tankApA2.writePos - tapA6 + kPoolSize) & (kPoolSize - 1)]
         - tankDelA2.readAt(tapA7);

    // Mix in early reflections then scale
    outL = (outL + erLevel * erL) * 0.5f;
    outR = (outR + erLevel * erR) * 0.5f;
}
