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

// R5: Early reflection tap delays (at 29761 Hz reference, mutually prime for Schroeder spacing)
// Expanded to 16 taps for denser, smoother early reflections
static constexpr int kNumERTaps = 16;
static constexpr float kERRef[16] = {
    79.0f, 149.0f, 214.0f, 318.0f, 419.0f, 529.0f, 631.0f, 764.0f,
    839.0f, 953.0f, 1091.0f, 1207.0f, 1361.0f, 1493.0f, 1637.0f, 1783.0f
};

// Per-tap gains with alternating signs for stereo decorrelation (energy-decaying)
static constexpr float kERGains[16] = {
    0.82f, 0.70f, -0.62f, 0.55f, -0.48f, 0.41f, -0.36f, 0.30f,
    -0.25f, 0.21f, -0.18f, 0.15f, -0.13f, 0.11f, -0.09f, 0.08f
};
static constexpr float kTapDelA1_c = 353.0f;
static constexpr float kTapDelB1_c = 3627.0f;
static constexpr float kTapApA2_b = 1228.0f;
static constexpr float kTapApB2_b = 2673.0f;
static constexpr float kTapDelA2_b = 2111.0f;
static constexpr float kTapDelB2_b = 335.0f;

static int scaleDelay(float refDelay, double sampleRate)
{
    static constexpr int kMaxDelay = 32768; // matches Allpass::maxDelay
    return std::max(1, std::min(kMaxDelay - 1, static_cast<int>(refDelay * static_cast<float>(sampleRate) / kRefRate)));
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
    sideDiffC.init(poolSideDiffC, scaleDelay(kInDiffA, sr), inputDiffusion1);
    sideDiffD.init(poolSideDiffD, scaleDelay(kInDiffB, sr), inputDiffusion1);

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

    // R1: Damping filter coefficients
    dampingHFAlpha = 0.5f;
    dampingLFMult = 1.0f;
    tankDampA = 0.0f;
    tankDampB = 0.0f;

    // R7: Input pre-filter coefficients
    // HP ~80Hz: removes bass rumble before diffusion
    inputHPCoeff = static_cast<float>(std::exp(-2.0 * 3.14159265358979 * 80.0 / sampleRate));
    inputHPState = 0.0f;
    // LP ~12kHz: prevents sibilance in reverb tail
    const float lpW = 2.0f * 3.14159265358979f * 12000.0f / static_cast<float>(sampleRate);
    inputLPCoeff = lpW / (1.0f + lpW); // 1-pole LP approximation
    inputLPState = 0.0f;

    // Output DC blocker coefficient (~5Hz HP)
    dcBlockR = static_cast<float>(std::exp(-2.0 * 3.14159265358979 * 5.0 / sampleRate));
    dcBlockInL = 0.0f; dcBlockInR = 0.0f;
    dcBlockOutL = 0.0f; dcBlockOutR = 0.0f;

    // R5: Early reflection tap lengths (scaled to sample rate) with per-tap LP coefficients
    for (int i = 0; i < kNumERTaps; ++i) {
        erTaps[i] = std::max(1, std::min(kERBufferSize - 1,
            static_cast<int>(kERRef[i] * static_cast<float>(sr) / kRefRate)));
        // Per-tap LP: further taps are darker (simulates wall absorption)
        const float tapMs = kERRef[i] / kRefRate * 1000.0f;
        const float cutoff = std::max(2000.0f, 16000.0f - tapMs * 200.0f);
        erTapLPCoeffs[i] = static_cast<float>(std::exp(-2.0 * 3.14159265 * cutoff / sampleRate));
        erTapLP[i] = 0.0f;
    }
    erWritePos = 0;

    reset();
    prepared = true;
}

void DattorroReverb::setDecay(float decay)
{
    decayVal = std::max(0.0f, std::min(0.99f, decay));
}

void DattorroReverb::setDamping(float damping)
{
    // damping 1.0 = no damping (bright), 0.0 = heavy damping (dark)
    // LP coefficient: higher = more HF passed through
    dampingHFAlpha = 0.05f + damping * 0.9f;
}

void DattorroReverb::setDampingLow(float damping)
{
    // R1: 1.0 = no LF damping, 0.0 = heavy LF damping (-6dB per cycle)
    dampingLFMult = 0.4f + damping * 0.6f; // range: 0.4–1.0
}

void DattorroReverb::setFreeze(float amount)
{
    // R6: 0.0 = normal, 1.0 = infinite sustain
    freezeAmount = std::max(0.0f, std::min(1.0f, amount));
}

void DattorroReverb::setSize(float s)
{
    sizeVal = std::max(0.0f, std::min(1.0f, s));
    // Size only controls LFO speed and modulation depth — decay is independent
    lfo1.setFreq(0.4f + sizeVal * 1.2f, static_cast<float>(sr));
    lfo2.setFreq(0.6f + sizeVal * 1.4f, static_cast<float>(sr));
    modDepthSamples = (4.0f + sizeVal * 8.0f) * static_cast<float>(sr) / kRefRate;

    // Size-dependent early reflections: scale tap delays
    const float erScale = 0.5f + sizeVal * 1.0f; // 0.5x at min, 1.5x at max
    for (int i = 0; i < kNumERTaps; ++i) {
        erTaps[i] = std::max(1, std::min(kERBufferSize - 1,
            static_cast<int>(kERRef[i] * erScale * static_cast<float>(sr) / kRefRate)));
    }
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
    sideDiffC.feedback = inputDiffusion1;
    sideDiffD.feedback = inputDiffusion1;
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
    std::memset(poolSideDiffC, 0, sizeof(poolSideDiffC));
    std::memset(poolSideDiffD, 0, sizeof(poolSideDiffD));
    tankDampA = 0.0f;
    tankDampB = 0.0f;
    tankDampA2 = 0.0f;
    tankDampB2 = 0.0f;
    dcBlockInL = 0.0f; dcBlockInR = 0.0f;
    dcBlockOutL = 0.0f; dcBlockOutR = 0.0f;
    tankADCState = 0.0f;
    tankBDCState = 0.0f;
    std::memset(preDelayBuffer, 0, sizeof(preDelayBuffer));
    std::memset(preDelaySideBuffer, 0, sizeof(preDelaySideBuffer));
    preDelayWritePos = 0;
    std::memset(erBuffer, 0, sizeof(erBuffer));
    std::memset(erSideBuffer, 0, sizeof(erSideBuffer));
    erWritePos = 0;
}

void DattorroReverb::processSample(float inL, float inR, float& outL, float& outR)
{
    if (!prepared) { outL = inL; outR = inR; return; }
    // True stereo: M/S decomposition, diffuse both mid and side before tanks
    const float inputMid = (inL + inR) * 0.5f;
    const float inputSide = (inL - inR) * 0.5f;

    preDelayBuffer[preDelayWritePos] = inputMid;
    preDelaySideBuffer[preDelayWritePos] = inputSide;
    const int preDelayReadPos = (preDelayWritePos - preDelayLength + kPreDelaySize) & (kPreDelaySize - 1);
    const float preDelayed = preDelayBuffer[preDelayReadPos];
    const float preDelayedSide = preDelaySideBuffer[preDelayReadPos];
    preDelayWritePos = (preDelayWritePos + 1) & (kPreDelaySize - 1);

    // R5: Early reflections: 16 Schroeder-style taps with per-tap LP filtering
    // Even-indexed taps → L, odd-indexed taps → R (alternating signs for stereo spread)
    erBuffer[erWritePos] = preDelayed;
    erSideBuffer[erWritePos] = preDelayedSide;
    float erL = 0.0f, erR = 0.0f;
    for (int i = 0; i < kNumERTaps; ++i)
    {
        const int readIdx = (erWritePos - erTaps[i] + kERBufferSize) & (kERBufferSize - 1);
        const float tapMid  = erBuffer[readIdx];
        const float tapSide = erSideBuffer[readIdx];
        // Per-tap LP filter (further taps = darker, simulates wall absorption)
        erTapLP[i] = tapMid * (1.0f - erTapLPCoeffs[i]) + erTapLP[i] * erTapLPCoeffs[i];
        const float filteredMid = erTapLP[i];
        // Decode mid/side into L/R so stereo content appears in early reflections
        if (i & 1) erR += kERGains[i] * (filteredMid - tapSide);
        else       erL += kERGains[i] * (filteredMid + tapSide);
    }
    erWritePos = (erWritePos + 1) & (kERBufferSize - 1);

    // R7: Input pre-filtering — HP removes bass rumble, LP removes sibilance
    const float hpIn = preDelayed;
    const float hpOut = hpIn - inputHPState;
    inputHPState += (1.0f - inputHPCoeff) * (hpIn - inputHPState);
    inputLPState += inputLPCoeff * (hpOut - inputLPState);
    const float filteredInput = inputLPState;

    // Diffuse mid (4 allpasses) and side (2 allpasses) independently
    float diffusedMid = inDiffA.process(filteredInput);
    diffusedMid = inDiffB.process(diffusedMid);
    diffusedMid = inDiffC.process(diffusedMid);
    diffusedMid = inDiffD.process(diffusedMid);
    float diffusedSide = sideDiffC.process(preDelayedSide);
    diffusedSide = sideDiffD.process(diffusedSide);
    diffusedSide = sideDiffA.process(diffusedSide);
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

    // R6: Freeze modulation — blend input/decay/damping toward infinite sustain
    const float inputScale = 1.0f - freezeAmount;
    const float effectiveDecay = decayVal + (1.0f - decayVal) * freezeAmount;
    const float effectiveHFDamp = dampingHFAlpha + (1.0f - dampingHFAlpha) * freezeAmount;
    const float effectiveLFMult = dampingLFMult + (1.0f - dampingLFMult) * freezeAmount;

    // True stereo: inject diffused mid + diffused side into separate tanks
    // R6: Scale new input by inputScale (frozen = no new input, feedback circulates)
    float tankA = (diffusedMid + diffusedSide) * inputScale + feedbackA;
    tankA = tankApA1.processModulated(tankA, mod1);
    tankDelA1.write(tankA);
    tankA = tankDelA1.read();
    // R1: HF damping LP (2-pole cascaded for steeper -12dB/oct absorption)
    tankDampA += effectiveHFDamp * (tankA - tankDampA);
    tankDampA2 += effectiveHFDamp * (tankDampA - tankDampA2);
    // R1: LF damping — attenuate low frequencies independently
    const float lfContentA = tankA - tankDampA2;
    tankA = tankDampA2 + lfContentA * effectiveLFMult;
    // DC blocker: remove DC accumulation in tank (critical for freeze mode)
    tankADCState += 0.00001f * (tankA - tankADCState);
    tankA -= tankADCState;
    // R2: Gentle feedback saturation — prevents explosion at high decay, adds density
    // ~1% THD at unity, self-limiting. Models EMT 140 plate driver nonlinearity.
    {
        const float driven = tankA * 1.05f;
        const float x2 = driven * driven;
        tankA = (driven * (27.0f + x2) / (27.0f + 9.0f * x2)) * (1.0f / 1.05f);
    }
    tankA *= effectiveDecay;
    tankA = tankApA2.processModulated(tankA, lfo2.cosVal * modDepthSamples * 0.5f);
    tankDelA2.write(tankA);

    // Tank B processing (mid - side = R-biased)
    float tankB = (diffusedMid - diffusedSide) * inputScale + feedbackB;
    tankB = tankApB1.processModulated(tankB, mod2);
    tankDelB1.write(tankB);
    tankB = tankDelB1.read();
    // R1: HF damping LP (2-pole cascaded for steeper -12dB/oct absorption)
    tankDampB += effectiveHFDamp * (tankB - tankDampB);
    tankDampB2 += effectiveHFDamp * (tankDampB - tankDampB2);
    // R1: LF damping — attenuate low frequencies independently
    const float lfContentB = tankB - tankDampB2;
    tankB = tankDampB2 + lfContentB * effectiveLFMult;
    // DC blocker: remove DC accumulation in tank (critical for freeze mode)
    tankBDCState += 0.00001f * (tankB - tankBDCState);
    tankB -= tankBDCState;
    // R2: Feedback saturation (Tank B)
    {
        const float driven = tankB * 1.05f;
        const float x2 = driven * driven;
        tankB = (driven * (27.0f + x2) / (27.0f + 9.0f * x2)) * (1.0f / 1.05f);
    }
    tankB *= effectiveDecay;
    tankB = tankApB2.processModulated(tankB, lfo1.cosVal * modDepthSamples * 0.5f);
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

    // DC blocker: y[n] = x[n] - x[n-1] + R * y[n-1]
    {
        const float newL = outL - dcBlockInL + dcBlockR * dcBlockOutL;
        dcBlockInL = outL;
        dcBlockOutL = newL;
        outL = newL;

        const float newR = outR - dcBlockInR + dcBlockR * dcBlockOutR;
        dcBlockInR = outR;
        dcBlockOutR = newR;
        outR = newR;
    }
}
