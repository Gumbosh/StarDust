#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <cmath>

class Saturation
{
public:
    Saturation() = default;

    void prepare(double sampleRate, int samplesPerBlock);

    void setInputGain(float gainDb);
    void setOutputGain(float gainDb);
    void setDrive(float driveAmount);
    void setMode(int m);     // 0=Soft (tanh), 1=Tube (asymmetric), 2=Hard (clip), 3=Satin, 4=Xfmr, 5=Vari-Mu
    void setTone(float t);   // [0,1]: HP 80–500Hz, LP 2kHz–20kHz (presence bandpass)

    void processInput(juce::AudioBuffer<float>& buffer);
    void processOutput(juce::AudioBuffer<float>& buffer);
    void processTone(juce::AudioBuffer<float>& buffer); // 2-pole HP + LP EQ

private:
    void applyGainAndSaturation(juce::AudioBuffer<float>& buffer,
                                 juce::SmoothedValue<float>& gainSmoothed,
                                 juce::SmoothedValue<float>& driveSmoothed);

    static float dBToLinear(float dB) { return std::pow(10.0f, dB / 20.0f); }

    juce::SmoothedValue<float> inputGainSmoothed { 1.0f };
    juce::SmoothedValue<float> outputGainSmoothed { 1.0f };
    juce::SmoothedValue<float> driveSmoothed { 0.0f };

    float currentInputGainDb = 0.0f;
    float currentOutputGainDb = 0.0f;
    float currentDrive = 0.0f;
    int currentMode = 0; // 0=Soft, 1=Tube, 2=Hard, 3=Satin, 4=Xfmr, 5=Vari-Mu
    int pendingMode = -1;        // -1 = no pending change
    float modeRamp = 1.0f;       // fade amplitude [0..1]
    float modeRampInc = 0.0f;    // per-sample ramp increment (negative=fade out, positive=fade in)
    float currentTone = -1.0f; // -1 forces initial coefficient calculation

    double sampleRate = 44100.0;

    // DC blocker state per channel
    static constexpr int kMaxChannels = 2;
    float dcX1[kMaxChannels] = {};
    float dcY1[kMaxChannels] = {};
    float dcCoeff = 0.995f; // recalculated in prepare() for ~20Hz HPF

    // Vari-Mu envelope follower (D12): attack/release for compressive saturation
    float variMuEnv[kMaxChannels] = {};
    float variMuAttack = 0.0f;   // ~15ms attack
    float variMuRelease = 0.0f;  // ~150ms release

    // S1: 3-band multiband crossover (Linkwitz-Riley 4th-order at 250Hz and 3kHz)
    struct LR4State {
        float z1a = 0, z2a = 0;  // first 2nd-order section
        float z1b = 0, z2b = 0;  // second 2nd-order section
    };
    float lr4Lo[5] = {1,0,0,0,0};   // 250Hz LP coefficients (1x rate)
    float lr4Hi[5] = {1,0,0,0,0};   // 3kHz LP coefficients (1x rate)
    float lr4Lo8x[5] = {1,0,0,0,0}; // 250Hz LP coefficients (8x rate)
    float lr4Hi8x[5] = {1,0,0,0,0}; // 3kHz LP coefficients (8x rate)
    // LR4 states for 8x-rate oversampled crossover (inside the oversampled loop)
    LR4State lr4LoStateLP8x[kMaxChannels] = {};
    LR4State lr4LoStateHP8x[kMaxChannels] = {};
    LR4State lr4HiStateLP8x[kMaxChannels] = {};
    LR4State lr4HiStateHP8x[kMaxChannels] = {};
    // Original 1x-rate states kept for any external use
    LR4State lr4LoStateLP[kMaxChannels] = {};
    LR4State lr4LoStateHP[kMaxChannels] = {};
    LR4State lr4HiStateLP[kMaxChannels] = {};
    LR4State lr4HiStateHP[kMaxChannels] = {};

    // S2: Per-mode character EQ (applied post-saturation)
    float modeEQ1[5] = {1,0,0,0,0};  // low shelf or peak
    float modeEQ2[5] = {1,0,0,0,0};  // high shelf or peak
    struct BiquadState2 { float z1 = 0.0f, z2 = 0.0f; };
    BiquadState2 modeEQ1State[kMaxChannels] = {};
    BiquadState2 modeEQ2State[kMaxChannels] = {};
    void computeModeEQ();

    // S4: Program-dependent dynamics envelope
    float progEnv[kMaxChannels] = {};
    float progAttack = 0.0f;   // ~50ms
    float progRelease = 0.0f;  // ~500ms

    // Mode transition: dual-mode curve morphing (D14)
    // When transitioning, run BOTH old and new modes and crossfade
    int prevMode = 0;            // mode before transition started
    bool isMorphing = false;     // true during crossfade period

    // 2-pole presence EQ (HP + LP biquad) for tone control
    struct BiquadState { float z1 = 0.0f, z2 = 0.0f; };
    std::array<BiquadState, kMaxChannels> toneHPState {}, toneLPState {};
    float toneHP[5] = { 1, 0, 0, 0, 0 }; // b0 b1 b2 a1 a2
    float toneLP[5] = { 1, 0, 0, 0, 0 };

    // Output transformer stage: 30Hz coupling HP + 18kHz iron-core LP
    void computeTransformerCoeffs();
    std::array<BiquadState, kMaxChannels> xfmrHPState {}, xfmrLPState {};
    float xfmrHP[5] = { 1, 0, 0, 0, 0 }; // b0 b1 b2 a1 a2
    float xfmrLP[5] = { 1, 0, 0, 0, 0 };

    // 8× oversampling for alias-free nonlinear processing
    static constexpr int kOversample = 8;
    float prevSample[kMaxChannels] = {};       // for Hermite cubic (Catmull-Rom) upsampling
    float prevPrevSample[kMaxChannels] = {};   // second history slot for Catmull-Rom interpolation
    // 15-tap Kaiser half-band (~90dB stopband) — 8-element even-polyphase history per stage
    float hb1History[kMaxChannels][8] = {};    // stage 1 branch A  (8x→4x, pair 0-1)
    float hb2History[kMaxChannels][8] = {};    // stage 1 branch B  (8x→4x, pair 2-3)
    float hb3History[kMaxChannels][8] = {};    // stage 1 branch C  (8x→4x, pair 4-5)
    float hb4History[kMaxChannels][8] = {};    // stage 1 branch D  (8x→4x, pair 6-7)
    float hb3aHistory[kMaxChannels][8] = {};   // stage 2 branch A  (4x→2x)
    float hb3bHistory[kMaxChannels][8] = {};   // stage 2 branch B  (4x→2x)
    float hb4aHistory[kMaxChannels][8] = {};   // stage 3           (2x→1x)
    static float halfBandDecimate(float a, float b, float* hist) noexcept;
};
