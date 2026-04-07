#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "presets/FactoryPresets.h"
#include "dsp/FastMath.h"

StardustProcessor::StardustProcessor()
    : AudioProcessor(BusesProperties()
          .withInput("Input", juce::AudioChannelSet::stereo(), true)
          .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, &undoManager, "Parameters", createParameterLayout())
{
    initFactoryPresets();
    refreshPresets();
    loadPreset(0);
}

juce::AudioProcessorValueTreeState::ParameterLayout StardustProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("destroyIn", 1), "Destroy In",
        juce::NormalisableRange<float>(0.0f, 24.0f, 0.1f), 0.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("destroyOut", 1), "Destroy Out",
        juce::NormalisableRange<float>(-12.0f, 12.0f, 0.1f), 0.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("destroyMix", 1), "Destroy Mix",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 1.0f));

    {
        juce::NormalisableRange<float> rateRange(250.0f, 96000.0f, 1.0f);
        rateRange.setSkewForCentre(3000.0f); // 3kHz at knob centre, log feel
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("destroyFader", 1), "Destroy Fader", rateRange, 22082.0f));
    }

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("destroyBits", 1), "Destroy Bits",
        juce::NormalisableRange<float>(3.0f, 24.0f, 0.001f), 12.0f));  // 12-bit SP-950 authentic

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("destroyJitter", 1), "Destroy Jitter",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.10f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("chorusMix", 1), "Chorus Mix",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 1.0f));

    // Juno-60 chorus mode: 0=I, 1=II, 2=I+II
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID("junoMode", 1), "Chorus Mode",
        juce::StringArray{"I", "II", "I+II"}, 0));

    // Modulation LFOs (2 LFOs x 4 params = 8 params)
    for (int lfo = 1; lfo <= 2; ++lfo)
    {
        const auto prefix = juce::String("modLfo") + juce::String(lfo);
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID(prefix + "Rate", 1), "LFO " + juce::String(lfo) + " Rate",
            juce::NormalisableRange<float>(0.01f, 20.0f, 0.01f, 0.4f), 1.0f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID(prefix + "Depth", 1), "LFO " + juce::String(lfo) + " Depth",
            juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.0f));
        params.push_back(std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID(prefix + "Wave", 1), "LFO " + juce::String(lfo) + " Wave",
            juce::StringArray{"Sine", "Triangle", "Saw", "Random"}, 0));
        params.push_back(std::make_unique<juce::AudioParameterBool>(
            juce::ParameterID(prefix + "Sync", 1), "LFO " + juce::String(lfo) + " Sync", false));
    }

    // Modulation routing slots (4 slots x 3 params = 12 params)
    const juce::StringArray sourceNames {"Off", "LFO 1", "LFO 2", "Env Follower"};
    const juce::StringArray targetNames {"Off",
                                         "Tape Drive", "Tape Wear", "Tape Mix"};
    for (int slot = 1; slot <= 4; ++slot)
    {
        const auto prefix = juce::String("modSlot") + juce::String(slot);
        params.push_back(std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID(prefix + "Src", 1), "Slot " + juce::String(slot) + " Source",
            sourceNames, 0));
        params.push_back(std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID(prefix + "Tgt", 1), "Slot " + juce::String(slot) + " Target",
            targetNames, 0));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID(prefix + "Amt", 1), "Slot " + juce::String(slot) + " Amount",
            juce::NormalisableRange<float>(-1.0f, 1.0f, 0.01f), 0.0f));
    }

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("tapeDrive", 1), "Tape Drive",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.17f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("tapeInput", 1), "Tape Input",
        juce::NormalisableRange<float>(-12.0f, 12.0f, 0.1f), 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("tapeGlue", 1), "Tape Glue",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.48f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("tapeNoise", 1), "Tape Noise",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.07f));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID("tapeNoiseSpeed", 1), "Tape Noise Speed",
        juce::StringArray{"7.5 ips", "15 ips", "30 ips"}, 1));

    // Tape formulation hardcoded to Ampex 456 (index 0) — no user selection
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID("tapeFormulation", 1), "Tape Formulation",
        juce::StringArray{"456"}, 0));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("tapeMix", 1), "Tape Mix",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 1.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("tapeOutput", 1), "Tape Output",
        juce::NormalisableRange<float>(-24.0f, 12.0f, 0.1f, 1.71f), 0.0f));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID("tapeEnabled", 1), "Tape Enabled", false));
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID("destroyEnabled", 1), "Destroy Enabled", false));
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID("multiplyEnabled", 1), "Multiply Enabled", false));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("inputGain", 1), "Input Gain",
        juce::NormalisableRange<float>(-24.0f, 12.0f, 0.1f, 1.71f), 0.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("outputGain", 1), "Output Gain",
        juce::NormalisableRange<float>(-24.0f, 12.0f, 0.1f, 1.71f), 0.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("masterMix", 1), "Master Mix",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 1.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("msWidth", 1), "M/S Width",
        juce::NormalisableRange<float>(0.0f, 2.0f, 0.01f), 1.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("distortionDrive", 1), "Distortion Drive",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.5f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("distortionTone", 1), "Distortion Tone",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.5f));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID("distortionMode", 1), "Distortion Mode",
        juce::StringArray{"Soft", "Tube", "Hard"}, 0));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("distortionMix", 1), "Distortion Mix",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 1.0f));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID("distortionEnabled", 1), "Distortion Enabled", false));

    // FX chain slot assignments (0=empty, 1=CRUSH, 3=CHORUS, 4=TAPE, 5=SATURATE, 6=REVERB, 7=HAZE, 8=MULTIPLY, 9=GRAIN, 10=STUTTER, 11=SHIFT)
    for (int i = 0; i < 4; ++i)
        params.push_back(std::make_unique<juce::AudioParameterInt>(
            juce::ParameterID("chainSlot" + juce::String(i), 1),
            "FX Slot " + juce::String(i + 1), 0, 13, i + 1));

    // Reverb (standalone Dattorro plate)
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("reverbMix", 1), "Reverb Mix",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.5f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("reverbSize", 1), "Reverb Size",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.5f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("reverbDecay", 1), "Reverb Decay",
        juce::NormalisableRange<float>(0.0f, 0.99f, 0.01f), 0.5f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("reverbDamp", 1), "Reverb Damp",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.7f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("reverbPreDelay", 1), "Reverb Pre-Delay",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.5f), 10.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("reverbDiffusion", 1), "Reverb Diffusion",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.7f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("reverbWidth", 1), "Reverb Width",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 1.0f));
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID("reverbEnabled", 1), "Reverb Enabled", false));

    // --- Feature additions ---

    // Tape: combined Wobble (drives both wow + flutter), Print-Through level, EQ standard, Motor
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("tapeWow", 1), "Tape Wobble",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.07f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("tapePrintThrough", 1), "Tape Print-Through",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.3f));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID("tapeEQStandard", 1), "Tape EQ Standard",
        juce::StringArray{"NAB", "IEC"}, 0));
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID("tapeMotorEnabled", 1), "Tape Motor", true));

    // HAZE: noise texture injection (slot 7)
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("hazeColor", 1), "Haze Color",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.5f));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID("hazeType", 1), "Haze Type",
        juce::StringArray{"Pink", "White", "Vinyl"}, 0));
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID("hazeEnabled", 1), "Haze Enabled", false));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("hazeMix", 1), "Haze Mix",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.15f));

    // MULTIPLY: simple 4-voice unison thickener (slot 8)
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID("unisonEnabled", 1), "Multiply Enabled", false));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("unisonMix", 1), "Multiply Mix",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.5f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("unisonSpeed", 1), "Multiply Speed",
        juce::NormalisableRange<float>(0.5f, 2.0f, 0.01f), 1.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("unisonOuter", 1), "Multiply 1+2",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 1.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("unisonInner", 1), "Multiply 3+4",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.8f));

    // GRAIN: granular freeze/scatter (slot 9)
    {
        juce::NormalisableRange<float> sizeRange(5.0f, 200.0f, 0.1f);
        sizeRange.setSkewForCentre(30.0f);
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("grainSize", 1), "Grain Size", sizeRange, 50.0f));
    }
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("grainDensity", 1), "Grain Density",
        juce::NormalisableRange<float>(1.0f, 16.0f, 0.1f), 4.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("grainPitch", 1), "Grain Pitch",
        juce::NormalisableRange<float>(-12.0f, 12.0f, 0.01f), 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("grainScatter", 1), "Grain Scatter",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.5f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("grainReverse", 1), "Grain Reverse",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID("grainSync", 1), "Grain Sync",
        juce::StringArray{"Free", "1/64", "1/32T", "1/32", "1/16T", "1/16", "1/8T",
                          "1/8", "1/4T", "1/4", "1/2T", "1/2", "1 Bar"}, 0));
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID("grainEnabled", 1), "Grain Enabled", false));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("grainMix", 1), "Grain Mix",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.5f));

    // STUTTER: buffer repeat/glitch (slot 10)
    {
        juce::NormalisableRange<float> rateRange(10.0f, 1000.0f, 0.1f);
        rateRange.setSkewForCentre(100.0f);
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("stutterRate", 1), "Stutter Rate", rateRange, 125.0f));
    }
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("stutterDecay", 1), "Stutter Decay",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.8f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("stutterDepth", 1), "Stutter Depth",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 1.0f));
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID("stutterEnabled", 1), "Stutter Enabled", false));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("stutterMix", 1), "Stutter Mix",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 1.0f));

    // SHIFT: pitch shifter (slot 11)
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("shiftPitch", 1), "Shift Pitch",
        juce::NormalisableRange<float>(-24.0f, 24.0f, 1.0f), 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("shiftFeedback", 1), "Shift Feedback",
        juce::NormalisableRange<float>(0.0f, 0.95f, 0.01f), 0.0f));
    {
        juce::NormalisableRange<float> toneRange(200.0f, 20000.0f, 1.0f);
        toneRange.setSkewForCentre(2000.0f);
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("shiftTone", 1), "Shift Tone", toneRange, 10000.0f));
    }
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID("shiftEnabled", 1), "Shift Enabled", false));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("shiftMix", 1), "Shift Mix",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.5f));

    // REVERSER: tempo-synced reverse (slot 12)
    params.push_back(std::make_unique<juce::AudioParameterInt>(
        juce::ParameterID("reverserRepeat", 1), "Reverser Repeat", 1, 64, 4)); // repeat count (kHs style "4/16")
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID("reverserDivision", 1), "Reverser Division",
        juce::StringArray{"1/64", "1/32", "1/16T", "1/16", "1/8T", "1/8", "1/4T", "1/4"}, 3)); // default 1/16
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("reverserCrossfade", 1), "Reverser Crossfade",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.15f));
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID("reverserEnabled", 1), "Reverser Enabled", false));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("reverserMix", 1), "Reverser Mix",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.5f));

    // HALFTIME: tempo-synced half/quarter speed (slot 13)
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID("halftimeDivision", 1), "HalfTime Division",
        juce::StringArray{"1/16", "1/8T", "1/8", "1/4", "1/2", "1 Bar", "2 Bars", "4 Bars"}, 2));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID("halftimeSpeed", 1), "HalfTime Speed",
        juce::StringArray{"2x", "4x"}, 0));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("halftimeFade", 1), "HalfTime Fade",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.15f));
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID("halftimeEnabled", 1), "HalfTime Enabled", false));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("halftimeMix", 1), "HalfTime Mix",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.5f));

    return { params.begin(), params.end() };
}

void StardustProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;

    // 2x oversampling for destroy section
    oversampling = std::make_unique<juce::dsp::Oversampling<float>>(
        2, 1, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true);
    oversampling->initProcessing(static_cast<size_t>(samplesPerBlock));

    // CRUSH DSP modules run at native rate (Degrader-style: no oversampling)
    bitCrusher.prepare(sampleRate, samplesPerBlock);
    destroyDrive.prepare(sampleRate, samplesPerBlock);

    chorusEngine.prepare(sampleRate, samplesPerBlock);
    multiplyEngine.prepare(sampleRate, samplesPerBlock);
    tapeEngine.prepare(sampleRate, samplesPerBlock);
    tapeEngine.setStandard(0);    // NAB — hardcoded
    tapeNoiseSpeedParam  = dynamic_cast<juce::AudioParameterChoice*>(apvts.getParameter("tapeNoiseSpeed"));
    // tapeFormulation hardcoded to 456 — no runtime selection needed
    saturation.prepare(sampleRate, samplesPerBlock);
    standaloneReverb.prepare(sampleRate);
    granularEngine.prepare(sampleRate, samplesPerBlock);
    stutterEngine.prepare(sampleRate, samplesPerBlock);
    pitchShifter.prepare(sampleRate, samplesPerBlock);
    reverserEngine.prepare(sampleRate, samplesPerBlock);
    halfTimeEngine.prepare(sampleRate, samplesPerBlock);

    // Pre-allocate with generous headroom for oversampled data
    // Use 8x to handle hosts that exceed the declared block size
    dryBuffer.setSize(2, samplesPerBlock * 8, false, true, true);
    masterDryBuffer.setSize(2, samplesPerBlock * 2, false, true, true);
    msWidthSmoothed.reset(sampleRate, 0.02);
    inputGainSmoothed.reset(sampleRate, 0.01);
    outputGainSmoothed.reset(sampleRate, 0.01);
    modMatrix.prepare(sampleRate);

    // H2: Mains hum incremental oscillators (zero trig per sample)
    const float sr = static_cast<float>(currentSampleRate);
    humOsc60.init(60.0f, sr);
    humOsc120.init(120.0f, sr);
    humOsc180.init(180.0f, sr);
    humOsc240.init(240.0f, sr);

    // H3: Sub-bass rumble
    rumbleLFOPhase = 0.0f;
    std::memset(rumbleState, 0, sizeof(rumbleState));

    // H1: Vinyl crackle
    std::memset(crackleDecay, 0, sizeof(crackleDecay));
    std::memset(crackleImpulse, 0, sizeof(crackleImpulse));
    std::memset(crackleBPState, 0, sizeof(crackleBPState));

    // H4: Signal envelope
    std::memset(hazeSignalEnv, 0, sizeof(hazeSignalEnv));

    // H5: Wow LFO
    hazeWowPhase = 0.0f;

    // H6: Allpass decorrelation
    std::memset(hazeAllpassBuf, 0, sizeof(hazeAllpassBuf));
    hazeAllpassWritePos = 0;

    // Cache parameter pointers for real-time-safe access in processBlock
    // (avoids juce::String heap allocation on the audio thread)
    cachedModLfo1Rate  = apvts.getRawParameterValue("modLfo1Rate");
    cachedModLfo1Depth = apvts.getRawParameterValue("modLfo1Depth");
    cachedModLfo1Wave  = apvts.getRawParameterValue("modLfo1Wave");
    cachedModLfo1Sync  = apvts.getRawParameterValue("modLfo1Sync");
    cachedModLfo2Rate  = apvts.getRawParameterValue("modLfo2Rate");
    cachedModLfo2Depth = apvts.getRawParameterValue("modLfo2Depth");
    cachedModLfo2Wave  = apvts.getRawParameterValue("modLfo2Wave");
    cachedModLfo2Sync  = apvts.getRawParameterValue("modLfo2Sync");

    cachedModSlot1Src = apvts.getRawParameterValue("modSlot1Src");
    cachedModSlot1Tgt = apvts.getRawParameterValue("modSlot1Tgt");
    cachedModSlot1Amt = apvts.getRawParameterValue("modSlot1Amt");
    cachedModSlot2Src = apvts.getRawParameterValue("modSlot2Src");
    cachedModSlot2Tgt = apvts.getRawParameterValue("modSlot2Tgt");
    cachedModSlot2Amt = apvts.getRawParameterValue("modSlot2Amt");
    cachedModSlot3Src = apvts.getRawParameterValue("modSlot3Src");
    cachedModSlot3Tgt = apvts.getRawParameterValue("modSlot3Tgt");
    cachedModSlot3Amt = apvts.getRawParameterValue("modSlot3Amt");
    cachedModSlot4Src = apvts.getRawParameterValue("modSlot4Src");
    cachedModSlot4Tgt = apvts.getRawParameterValue("modSlot4Tgt");
    cachedModSlot4Amt = apvts.getRawParameterValue("modSlot4Amt");

    cachedChainSlot[0] = apvts.getRawParameterValue("chainSlot0");
    cachedChainSlot[1] = apvts.getRawParameterValue("chainSlot1");
    cachedChainSlot[2] = apvts.getRawParameterValue("chainSlot2");
    cachedChainSlot[3] = apvts.getRawParameterValue("chainSlot3");

    // Always report max latency (oversampling + tape base delay) to avoid
    // host re-sync glitches when sections toggle on/off (fix 1.5)
    setLatencySamples(static_cast<int>(oversampling->getLatencyInSamples())
                      + TapeEngine::getBaseDelaySamples());

}

void StardustProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                     juce::MidiBuffer& /*midiMessages*/)
{
    juce::ScopedNoDenormals noDenormals;

    const auto totalNumInputChannels = getTotalNumInputChannels();
    const auto totalNumOutputChannels = getTotalNumOutputChannels();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear(i, 0, buffer.getNumSamples());

    // Flush all DSP state on preset change — prevents old reverb tails, delay
    // contents, and granular buffers from bleeding into the new preset.
    // Uses a short 4ms fade-out to avoid clicks from the hard buffer reset.
    if (presetChangeReset.exchange(false))
    {
        // Quick fade-out of current buffer to avoid click (4ms or remaining samples)
        const int fadeSamples = std::min(buffer.getNumSamples(),
                                         static_cast<int>(currentSampleRate * 0.004));
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            auto* data = buffer.getWritePointer(ch);
            for (int i = 0; i < fadeSamples; ++i)
                data[i] *= static_cast<float>(fadeSamples - i) / static_cast<float>(fadeSamples);
            for (int i = fadeSamples; i < buffer.getNumSamples(); ++i)
                data[i] = 0.0f;
        }

        // Reset all DSP modules (zeroes buffers, resets positions)
        const int blk = buffer.getNumSamples();
        tapeEngine.prepare(currentSampleRate, blk);
        standaloneReverb.prepare(currentSampleRate);
        standaloneReverb.reset();
        chorusEngine.prepare(currentSampleRate, blk);
        multiplyEngine.prepare(currentSampleRate, blk);
        saturation.prepare(currentSampleRate, blk);
        destroyDrive.prepare(currentSampleRate, blk);
        bitCrusher.prepare(currentSampleRate, blk);
        granularEngine.prepare(currentSampleRate, blk);
        stutterEngine.prepare(currentSampleRate, blk);
        pitchShifter.prepare(currentSampleRate, blk);
        reverserEngine.prepare(currentSampleRate, blk);
        halfTimeEngine.prepare(currentSampleRate, blk);

        // Clear haze state (inline DSP, no separate module)
        std::memset(noisePinkB, 0, sizeof(noisePinkB));
        std::memset(hazeColorLP, 0, sizeof(hazeColorLP));

        return; // skip processing this block — next block starts clean
    }

    // Input gain — smoothed per-sample to avoid zipper noise on automation (fix 2.4)
    const float inputGainDb = *apvts.getRawParameterValue("inputGain");
    inputGainSmoothed.setTargetValue(std::pow(10.0f, inputGainDb / 20.0f));
    if (std::abs(inputGainDb) > 0.05f || inputGainSmoothed.isSmoothing())
    {
        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            const float g = inputGainSmoothed.getNextValue();
            for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                buffer.getWritePointer(ch)[i] *= g;
        }
    }
    else
    {
        inputGainSmoothed.skip(buffer.getNumSamples());
    }

    // Measure input levels
    inputLevelLeft.store(buffer.getMagnitude(0, 0, buffer.getNumSamples()));
    if (buffer.getNumChannels() > 1)
        inputLevelRight.store(buffer.getMagnitude(1, 0, buffer.getNumSamples()));

    const auto numSamples = buffer.getNumSamples();
    const auto numChannels = buffer.getNumChannels();

    // Save dry copy for master dry/wet blend
    const float masterMixVal = *apvts.getRawParameterValue("masterMix");
    const bool masterDryOk = masterDryBuffer.getNumSamples() >= numSamples;
    if (masterMixVal < 0.999f && masterDryOk)
    {
        for (int ch = 0; ch < numChannels; ++ch)
            masterDryBuffer.copyFrom(ch, 0, buffer, ch, 0, numSamples);
    }

    // Read parameters
    const float destroyInVal    = *apvts.getRawParameterValue("destroyIn");
    const float destroyOutVal   = *apvts.getRawParameterValue("destroyOut");
    const float destroyMixVal   = *apvts.getRawParameterValue("destroyMix");
    const float destroyFaderVal = *apvts.getRawParameterValue("destroyFader");
    const float destroyBitsVal  = *apvts.getRawParameterValue("destroyBits");
    const float destroyJitterVal = *apvts.getRawParameterValue("destroyJitter");
    // Update modulation matrix LFOs and routing (using cached pointers — no heap alloc)
    modMatrix.setLFO(0, *cachedModLfo1Rate, *cachedModLfo1Depth,
        static_cast<int>(*cachedModLfo1Wave), *cachedModLfo1Sync >= 0.5f);
    modMatrix.setLFO(1, *cachedModLfo2Rate, *cachedModLfo2Depth,
        static_cast<int>(*cachedModLfo2Wave), *cachedModLfo2Sync >= 0.5f);

    modMatrix.setSlot(0, static_cast<int>(*cachedModSlot1Src) - 1,
        static_cast<int>(*cachedModSlot1Tgt) - 1, *cachedModSlot1Amt);
    modMatrix.setSlot(1, static_cast<int>(*cachedModSlot2Src) - 1,
        static_cast<int>(*cachedModSlot2Tgt) - 1, *cachedModSlot2Amt);
    modMatrix.setSlot(2, static_cast<int>(*cachedModSlot3Src) - 1,
        static_cast<int>(*cachedModSlot3Tgt) - 1, *cachedModSlot3Amt);
    modMatrix.setSlot(3, static_cast<int>(*cachedModSlot4Src) - 1,
        static_cast<int>(*cachedModSlot4Tgt) - 1, *cachedModSlot4Amt);
    // Feed envelope follower from input levels (average of L/R peak)
    modMatrix.setEnvFollower((inputLevelLeft.load(std::memory_order_relaxed)
                            + inputLevelRight.load(std::memory_order_relaxed)) * 0.5f);

    // Advance LFOs to block midpoint, then read modulated values (fix 1.1)
    const int halfBlock = numSamples / 2;
    for (int s = 0; s < halfBlock; ++s)
        modMatrix.processSample();

    // Advance remaining LFO samples after reading midpoint values
    for (int s = halfBlock; s < numSamples; ++s)
        modMatrix.processSample();

    const float chorusMixVal   = *apvts.getRawParameterValue("chorusMix");
    const bool destroyOn  = *apvts.getRawParameterValue("destroyEnabled") >= 0.5f;
    const bool multiplyOn = *apvts.getRawParameterValue("multiplyEnabled") >= 0.5f;
    const bool tapeOn     = *apvts.getRawParameterValue("tapeEnabled") >= 0.5f;

    // Reset tape state BEFORE processing when tape toggles on (fix 1.4)
    // Motor ramp follows the section toggle: ramps up on enable, down on disable.
    if (tapeOn != lastTapeOn)
    {
        if (tapeOn) tapeEngine.resetState();
        else tapeEngine.setMotorEnabled(false); // ramp output down smoothly on bypass
        lastTapeOn = tapeOn;
    }

    // If host exceeds our generous headroom (2x declared block size for
    // oversampling, 8x for dry buffer), skip oversampled processing rather
    // than allocating on the audio thread. Audio quality silently degrades
    // but no crash occurs.
    const bool dryBufferOk = dryBuffer.getNumSamples() >= numSamples * 4;

    // Process effects in chain slot order
    for (int slot = 0; slot < 4; ++slot)
    {
        const int fx = static_cast<int>(*cachedChainSlot[slot]);
        switch (fx)
        {
            case 1: // CRUSH — Degrader-style: S&H → bit crush → saturation (no oversampling)
            {
                if (!destroyOn) break;

                // Save dry copy for mix blending
                if (destroyMixVal < 0.999f && dryBufferOk)
                    for (int ch = 0; ch < numChannels; ++ch)
                        dryBuffer.copyFrom(ch, 0, buffer, ch, 0, numSamples);

                // Guard against NaN/Inf from automation glitch
                const float safeFaderVal = (std::isfinite(destroyFaderVal)) ? destroyFaderVal : 26040.0f;
                const float effectiveRate = juce::jlimit(250.0f, 96000.0f, safeFaderVal);

                // 1. BitCrusher: S&H rate reduction → raw truncation (no dither)
                bitCrusher.setBitDepth(destroyBitsVal);
                bitCrusher.setSampleRate(effectiveRate);
                bitCrusher.setJitter(destroyJitterVal);
                bitCrusher.process(buffer);

                // 2. Simple tanh saturation AFTER crush (Degrader-style soft clip)
                //    destroyIn (0–24 dB) drives into saturation, destroyOut adjusts output level
                if (destroyInVal > 0.1f)
                {
                    const float driveGain = std::pow(10.0f, destroyInVal / 20.0f);
                    const float outGain   = std::pow(10.0f, destroyOutVal / 20.0f);
                    for (int ch = 0; ch < numChannels; ++ch)
                    {
                        auto* data = buffer.getWritePointer(ch);
                        for (int i = 0; i < numSamples; ++i)
                        {
                            const float x = data[i] * driveGain;
                            data[i] = FastMath::tanh(x) * outGain;
                        }
                    }
                }

                // Dry/wet mix
                if (destroyMixVal < 0.999f && dryBufferOk)
                {
                    const float dryGain = std::cos(destroyMixVal * juce::MathConstants<float>::halfPi);
                    const float wetGain = std::sin(destroyMixVal * juce::MathConstants<float>::halfPi);
                    for (int ch = 0; ch < numChannels; ++ch)
                    {
                        auto* wet = buffer.getWritePointer(ch);
                        const auto* dry = dryBuffer.getReadPointer(ch);
                        for (int i = 0; i < numSamples; ++i)
                            wet[i] = dry[i] * dryGain + wet[i] * wetGain;
                    }
                }
                break;
            }
            case 3: // CHORUS — Juno-60 ensemble emulation
            {
                if (!multiplyOn) break;
                chorusEngine.setMix(chorusMixVal);
                chorusEngine.setMode(static_cast<int>(*apvts.getRawParameterValue("junoMode")));
                chorusEngine.process(buffer);
                break;
            }
            case 4: // TAPE
            {
                if (!tapeOn) break;
                const float driveN = modMatrix.modulate(ModTarget::TapeDrive, *apvts.getRawParameterValue("tapeDrive"));
                const float mix    = modMatrix.modulate(ModTarget::TapeMix, *apvts.getRawParameterValue("tapeMix"));
                const float glue   = *apvts.getRawParameterValue("tapeGlue");
                const float noise  = *apvts.getRawParameterValue("tapeNoise");
                // Separate Wow + Flutter knobs (direct control, 0-1 maps to 0-0.48 depth)
                const float rawWow = *apvts.getRawParameterValue("tapeWow");
                tapeEngine.setWow(rawWow * 0.48f);
                // Flutter uses quadratic response: slow wow dominates at low settings;
                // flutter only becomes prominent at high wobble (accurate to tape physics).
                tapeEngine.setFlutter(rawWow * rawWow * 0.48f);
                tapeEngine.setHiss(juce::jlimit(0.0f, 1.0f, noise));
                tapeEngine.setBias(juce::jlimit(0.0f, 1.0f, glue));
                tapeEngine.setDrive(juce::jlimit(0.0f, 1.0f, driveN));
                tapeEngine.setMix(mix);
                tapeEngine.setInputGain(*apvts.getRawParameterValue("tapeInput"));
                tapeEngine.setTapeOutputDb(*apvts.getRawParameterValue("tapeOutput"));
                tapeEngine.setPrintThrough(*apvts.getRawParameterValue("tapePrintThrough"));
                tapeEngine.setMotorEnabled(true); // motor always runs while tape section is active
                {
                    static constexpr float kNoiseIps[] = { 7.5f, 15.0f, 30.0f };
                    const int noiseIx = tapeNoiseSpeedParam ? juce::jlimit(0, 2, tapeNoiseSpeedParam->getIndex()) : 1;
                    const float ips = kNoiseIps[noiseIx];
                    tapeEngine.setSpeed(ips);
                    tapeEngine.setHissSpeedIps(ips);
                }
                tapeEngine.setFormulation(0); // Ampex 456 hardcoded
                tapeEngine.process(buffer);
                break;
            }
            case 5: // DISTORTION — Saturation handles its own 8× internal oversampling
            {
                if (!(*apvts.getRawParameterValue("distortionEnabled") >= 0.5f)) break;
                const float driveVal   = *apvts.getRawParameterValue("distortionDrive");
                const float toneVal    = *apvts.getRawParameterValue("distortionTone");
                const float distMixVal = *apvts.getRawParameterValue("distortionMix");

                // Save dry signal for wet/dry blend
                if (distMixVal < 0.999f && dryBufferOk)
                {
                    for (int ch = 0; ch < numChannels; ++ch)
                        dryBuffer.copyFrom(ch, 0, buffer, ch, 0, numSamples);
                }

                // Engine modes: 0=Soft, 1=Tube, 2=Hard
                const int mode = juce::jlimit(0, 2,
                    static_cast<int>(*apvts.getRawParameterValue("distortionMode")));
                saturation.setDrive(driveVal);
                saturation.setMode(mode);
                saturation.processInput(buffer);
                saturation.setTone(toneVal);
                saturation.processTone(buffer);
                saturation.processOutput(buffer);

                if (distMixVal < 0.999f && dryBufferOk)
                {
                    const float dryGain = std::cos(distMixVal * juce::MathConstants<float>::halfPi);
                    const float wetGain = std::sin(distMixVal * juce::MathConstants<float>::halfPi);
                    for (int ch = 0; ch < numChannels; ++ch)
                    {
                        auto* wet = buffer.getWritePointer(ch);
                        const auto* dry = dryBuffer.getReadPointer(ch);
                        for (int i = 0; i < numSamples; ++i)
                            wet[i] = dry[i] * dryGain + wet[i] * wetGain;
                    }
                }
                break;
            }
            case 6: // REVERB (standalone Dattorro plate)
            {
                if (!(*apvts.getRawParameterValue("reverbEnabled") >= 0.5f)) break;

                standaloneReverb.setSize(*apvts.getRawParameterValue("reverbSize"));
                standaloneReverb.setDecay(*apvts.getRawParameterValue("reverbDecay"));
                standaloneReverb.setDamping(*apvts.getRawParameterValue("reverbDamp"));
                standaloneReverb.setPreDelay(*apvts.getRawParameterValue("reverbPreDelay"));
                standaloneReverb.setDiffusion(*apvts.getRawParameterValue("reverbDiffusion"));

                const float reverbMixVal = *apvts.getRawParameterValue("reverbMix");
                const float dryGain = std::cos(reverbMixVal * juce::MathConstants<float>::halfPi);
                const float wetGain = std::sin(reverbMixVal * juce::MathConstants<float>::halfPi);

                auto* dataL = buffer.getWritePointer(0);
                auto* dataR = numChannels > 1 ? buffer.getWritePointer(1) : nullptr;

                const float width = *apvts.getRawParameterValue("reverbWidth");
                // M/S width: 0=mono wet, 1=full stereo wet
                const float msS = width;
                const float msM = 1.0f;

                for (int i = 0; i < numSamples; ++i)
                {
                    const float inL = dataL[i];
                    const float inR = dataR ? dataR[i] : inL;
                    float outL = 0.0f, outR = 0.0f;
                    standaloneReverb.processSample(inL, inR, outL, outR);
                    // Apply M/S width to wet signal before dry/wet blend
                    const float wetMid  = (outL + outR) * 0.5f * msM;
                    const float wetSide = (outL - outR) * 0.5f * msS;
                    const float wL = wetMid + wetSide;
                    const float wR = wetMid - wetSide;
                    dataL[i] = inL * dryGain + wL * wetGain;
                    if (dataR) dataR[i] = inR * dryGain + wR * wetGain;
                }
                break;
            }
            case 7: // HAZE — atmospheric noise texture
            {
                if (!(*apvts.getRawParameterValue("hazeEnabled") >= 0.5f)) break;

                const float hazeColor  = *apvts.getRawParameterValue("hazeColor");
                const int   hazeType   = static_cast<int>(*apvts.getRawParameterValue("hazeType"));
                const float hazeMixVal = *apvts.getRawParameterValue("hazeMix");

                // 1-pole LP coefficient for noise color (hazeColor 0=500Hz, 1=20kHz)
                const float colorFreq = 500.0f + hazeColor * hazeColor * 19500.0f;

                // H5: Slow wow modulation on noise color (~0.5Hz)
                const float hazeWow = std::sin(hazeWowPhase) * 0.3f;  // ±30% cutoff variation
                const float modulatedColorFreq = std::min(colorFreq * (1.0f + hazeWow),
                                                        static_cast<float>(getSampleRate()) * 0.45f);
                const float hazeLPMod = juce::jlimit(0.0f, 0.999f,
                                                      1.0f - std::exp(-juce::MathConstants<float>::twoPi * modulatedColorFreq
                                                                       / static_cast<float>(currentSampleRate)));

                const float noiseScale = hazeMixVal * 0.012f; // ~-38dBFS at full strip
                for (int ch = 0; ch < numChannels; ++ch)
                {
                    auto* data = buffer.getWritePointer(ch);
                    for (int s = 0; s < numSamples; ++s)
                    {
                        float raw = 0.0f;
                        if (hazeType == 1) // White
                        {
                            raw = random.nextFloat() * 2.0f - 1.0f;
                        }
                        else if (hazeType == 2) // Vinyl: pink + crackle impulses
                        {
                            const float w = random.nextFloat() * 2.0f - 1.0f;
                            noisePinkB[0][ch] = 0.99886f * noisePinkB[0][ch] + w * 0.0555179f;
                            noisePinkB[1][ch] = 0.99332f * noisePinkB[1][ch] + w * 0.0750759f;
                            noisePinkB[2][ch] = 0.96900f * noisePinkB[2][ch] + w * 0.1538520f;
                            noisePinkB[3][ch] = 0.86650f * noisePinkB[3][ch] + w * 0.3104856f;
                            noisePinkB[4][ch] = 0.55000f * noisePinkB[4][ch] + w * 0.5329522f;
                            raw = (noisePinkB[0][ch] + noisePinkB[1][ch]
                                 + noisePinkB[2][ch] + noisePinkB[3][ch]
                                 + noisePinkB[4][ch] + w * 0.5362f) * 0.11f;
                            // H1: Improved vinyl crackle — Poisson-distributed impulses with bandpass
                            if (random.nextFloat() < 0.003f) {  // ~130 crackles/sec at 44.1kHz
                                crackleImpulse[ch] = (random.nextFloat() > 0.5f ? 1.0f : -1.0f) * 2.5f;
                                crackleDecay[ch] = 1.0f;
                            }
                            crackleDecay[ch] *= 0.992f;  // ~5ms exponential decay at 44.1kHz
                            float crackle = crackleImpulse[ch] * crackleDecay[ch];
                            // Simple 1-pole BP approximation (800Hz-6kHz character)
                            crackleBPState[ch] += 0.3f * (crackle - crackleBPState[ch]);
                            raw += crackleBPState[ch] * 0.4f;
                        }
                        else // Pink (default)
                        {
                            const float w = random.nextFloat() * 2.0f - 1.0f;
                            noisePinkB[0][ch] = 0.99886f * noisePinkB[0][ch] + w * 0.0555179f;
                            noisePinkB[1][ch] = 0.99332f * noisePinkB[1][ch] + w * 0.0750759f;
                            noisePinkB[2][ch] = 0.96900f * noisePinkB[2][ch] + w * 0.1538520f;
                            noisePinkB[3][ch] = 0.86650f * noisePinkB[3][ch] + w * 0.3104856f;
                            noisePinkB[4][ch] = 0.55000f * noisePinkB[4][ch] + w * 0.5329522f;
                            raw = (noisePinkB[0][ch] + noisePinkB[1][ch]
                                 + noisePinkB[2][ch] + noisePinkB[3][ch]
                                 + noisePinkB[4][ch] + w * 0.5362f) * 0.11f;
                        }

                        // H2: Mains hum (60Hz + harmonics at 120/180/240Hz)
                        const float hum = humOsc60.sinVal * 1.0f
                                        + humOsc120.sinVal * 0.5f
                                        + humOsc180.sinVal * 0.25f
                                        + humOsc240.sinVal * 0.12f;
                        raw += hum * 0.03f;  // subtle, ~-30dB relative to noise

                        // H3: Sub-bass rumble (25Hz resonant noise, AM modulated)
                        const float rumbleNoise = random.nextFloat() * 2.0f - 1.0f;
                        rumbleState[ch] += 0.004f * (rumbleNoise - rumbleState[ch]);  // ~25Hz LP
                        const float rumbleLFO = 0.5f + 0.5f * std::sin(rumbleLFOPhase);  // slow AM
                        raw += rumbleState[ch] * rumbleLFO * 0.15f;

                        // Apply LP color filter (H5: using wow-modulated coefficient)
                        hazeColorLP[ch] += hazeLPMod * (raw - hazeColorLP[ch]);

                        // H4: Noise riding — noise fades when signal is present
                        const float signalLevel = std::abs(data[s]);
                        const float envAlpha = signalLevel > hazeSignalEnv[ch] ? 0.01f : 0.0005f;  // fast attack, slow release
                        hazeSignalEnv[ch] += envAlpha * (signalLevel - hazeSignalEnv[ch]);
                        const float noiseRide = std::max(0.1f, 1.0f - hazeSignalEnv[ch] * 5.0f);

                        // H6: Stereo allpass decorrelation for R channel
                        if (ch == 1) {
                            const int readPos = (hazeAllpassWritePos - kHazeAllpassDelay + 512) & 511;
                            const float delayed = hazeAllpassBuf[readPos];
                            const float allpassOut = delayed - 0.3f * hazeColorLP[ch];
                            hazeAllpassBuf[hazeAllpassWritePos] = hazeColorLP[ch] + 0.3f * allpassOut;
                            hazeAllpassWritePos = (hazeAllpassWritePos + 1) & 511;
                            data[s] += allpassOut * noiseScale * noiseRide;
                        } else {
                            data[s] += hazeColorLP[ch] * noiseScale * noiseRide;
                        }

                        // Advance per-sample phase oscillators on ch==0 only
                        if (ch == 0) {
                            // H2: Advance hum oscillators
                            humOsc60.advance();
                            humOsc120.advance();
                            humOsc180.advance();
                            humOsc240.advance();
                            // Renormalize every 512 samples to prevent magnitude drift
                            if ((s & 511) == 0) { humOsc60.normalize(); humOsc120.normalize();
                                                   humOsc180.normalize(); humOsc240.normalize(); }
                            // H3: Rumble LFO phase (~0.2Hz)
                            rumbleLFOPhase += juce::MathConstants<float>::twoPi * 0.2f / static_cast<float>(currentSampleRate);
                            if (rumbleLFOPhase >= juce::MathConstants<float>::twoPi)
                                rumbleLFOPhase -= juce::MathConstants<float>::twoPi;
                            // H5: Wow LFO phase (~0.5Hz)
                            hazeWowPhase += juce::MathConstants<float>::twoPi * 0.5f / static_cast<float>(currentSampleRate);
                            if (hazeWowPhase >= juce::MathConstants<float>::twoPi)
                                hazeWowPhase -= juce::MathConstants<float>::twoPi;
                        }
                    }
                }
                break;
            }
            case 8: // MULTIPLY — 4-voice unison thickener
            {
                if (!(*apvts.getRawParameterValue("unisonEnabled") >= 0.5f)) break;
                multiplyEngine.setMix(*apvts.getRawParameterValue("unisonMix"));
                multiplyEngine.setSpeed(*apvts.getRawParameterValue("unisonSpeed"));
                multiplyEngine.setPans(*apvts.getRawParameterValue("unisonOuter"),
                                       *apvts.getRawParameterValue("unisonInner"));
                multiplyEngine.process(buffer);
                break;
            }
            case 9: // GRAIN — granular freeze/scatter
            {
                if (!(*apvts.getRawParameterValue("grainEnabled") >= 0.5f)) break;
                granularEngine.setSize(*apvts.getRawParameterValue("grainSize"));
                granularEngine.setDensity(*apvts.getRawParameterValue("grainDensity"));
                granularEngine.setPitch(*apvts.getRawParameterValue("grainPitch"));
                granularEngine.setScatter(*apvts.getRawParameterValue("grainScatter"));
                granularEngine.setReverse(*apvts.getRawParameterValue("grainReverse"));
                {
                    const int syncDiv = static_cast<int>(*apvts.getRawParameterValue("grainSync"));
                    granularEngine.setSyncDivision(syncDiv);
                    if (syncDiv > 0)
                        if (auto* playHead = getPlayHead())
                            if (auto pos = playHead->getPosition())
                                if (auto bpm = pos->getBpm())
                                    granularEngine.setHostBpm(*bpm);
                }
                granularEngine.setMix(*apvts.getRawParameterValue("grainMix"));
                granularEngine.process(buffer);
                break;
            }
            case 10: // STUTTER — buffer repeat/glitch
            {
                if (!(*apvts.getRawParameterValue("stutterEnabled") >= 0.5f)) break;
                stutterEngine.setRate(*apvts.getRawParameterValue("stutterRate"));
                stutterEngine.setDecay(*apvts.getRawParameterValue("stutterDecay"));
                stutterEngine.setDepth(*apvts.getRawParameterValue("stutterDepth"));
                stutterEngine.setMix(*apvts.getRawParameterValue("stutterMix"));
                stutterEngine.process(buffer);
                break;
            }
            case 11: // SHIFT — pitch shifter
            {
                if (!(*apvts.getRawParameterValue("shiftEnabled") >= 0.5f)) break;
                pitchShifter.setPitch(*apvts.getRawParameterValue("shiftPitch"));
                pitchShifter.setFeedback(*apvts.getRawParameterValue("shiftFeedback"));
                pitchShifter.setTone(*apvts.getRawParameterValue("shiftTone"));
                pitchShifter.setMix(*apvts.getRawParameterValue("shiftMix"));
                pitchShifter.process(buffer);
                break;
            }
            case 12: // REVERSER — tempo-synced reverse
            {
                if (!(*apvts.getRawParameterValue("reverserEnabled") >= 0.5f)) break;
                // Read BPM from host (default 120 if unavailable)
                double hostBPM = 120.0;
                if (auto* playHead = getPlayHead())
                    if (auto pos = playHead->getPosition())
                        if (auto bpm = pos->getBpm())
                            hostBPM = *bpm;
                reverserEngine.setBPM(hostBPM);
                reverserEngine.setRepeatCount(static_cast<int>(*apvts.getRawParameterValue("reverserRepeat")));
                reverserEngine.setDivision(static_cast<int>(*apvts.getRawParameterValue("reverserDivision")));
                reverserEngine.setCrossfade(*apvts.getRawParameterValue("reverserCrossfade"));
                reverserEngine.setMix(*apvts.getRawParameterValue("reverserMix"));
                reverserEngine.process(buffer);
                break;
            }
            case 13: // HALFTIME — tempo-synced half/quarter speed
            {
                if (!(*apvts.getRawParameterValue("halftimeEnabled") >= 0.5f)) break;
                double hostBPM = 120.0;
                if (auto* playHead = getPlayHead())
                    if (auto pos = playHead->getPosition())
                        if (auto bpm = pos->getBpm())
                            hostBPM = *bpm;
                halfTimeEngine.setBPM(hostBPM);
                halfTimeEngine.setDivision(static_cast<int>(*apvts.getRawParameterValue("halftimeDivision")));
                halfTimeEngine.setSpeed(static_cast<int>(*apvts.getRawParameterValue("halftimeSpeed")));
                halfTimeEngine.setFade(*apvts.getRawParameterValue("halftimeFade"));
                halfTimeEngine.setMix(*apvts.getRawParameterValue("halftimeMix"));
                halfTimeEngine.process(buffer);
                break;
            }
            default: break; // 0 = empty slot
        }
    }

    // M/S stereo width control (after all processing, before output gain)
    const float msWidthVal = *apvts.getRawParameterValue("msWidth");
    msWidthSmoothed.setTargetValue(msWidthVal);
    if (numChannels > 1 && std::abs(msWidthVal - 1.0f) > 0.01f)
    {
        auto* dataL = buffer.getWritePointer(0);
        auto* dataR = buffer.getWritePointer(1);
        for (int i = 0; i < numSamples; ++i)
        {
            const float width = msWidthSmoothed.getNextValue();
            const float mid  = (dataL[i] + dataR[i]) * 0.5f;
            const float side = (dataL[i] - dataR[i]) * 0.5f * width;
            dataL[i] = mid + side;
            dataR[i] = mid - side;
        }
    }

    // Output gain — smoothed per-sample to avoid zipper noise (fix 2.4)
    const float outputGainDb = *apvts.getRawParameterValue("outputGain");
    outputGainSmoothed.setTargetValue(std::pow(10.0f, outputGainDb / 20.0f));
    if (std::abs(outputGainDb) > 0.05f || outputGainSmoothed.isSmoothing())
    {
        for (int i = 0; i < numSamples; ++i)
        {
            const float g = outputGainSmoothed.getNextValue();
            for (int ch = 0; ch < numChannels; ++ch)
                buffer.getWritePointer(ch)[i] *= g;
        }
    }
    else
    {
        outputGainSmoothed.skip(numSamples);
    }

    // Master dry/wet blend (constant-power crossfade) — before limiter so dry path is also protected
    if (masterMixVal < 0.999f && masterDryOk)
    {
        // Apply output gain to dry buffer so both paths have consistent gain (fix 2.10)
        const float outGainLin = outputGainSmoothed.getCurrentValue();
        if (std::abs(outGainLin - 1.0f) > 0.001f)
        {
            for (int ch = 0; ch < numChannels; ++ch)
                masterDryBuffer.applyGain(ch, 0, numSamples, outGainLin);
        }

        const float dryGain = std::cos(masterMixVal * juce::MathConstants<float>::halfPi);
        const float wetGain = std::sin(masterMixVal * juce::MathConstants<float>::halfPi);
        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto* wet = buffer.getWritePointer(ch);
            const auto* dry = masterDryBuffer.getReadPointer(ch);
            for (int i = 0; i < numSamples; ++i)
                wet[i] = dry[i] * dryGain + wet[i] * wetGain;
        }
    }

    // Output safety limiter — catches runaway DSP only (threshold 2.0 = +6 dBFS).
    // Never engages on normal programme material; only prevents hard clipping from
    // internal processing bugs (e.g. J-A instability).
    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto* data = buffer.getWritePointer(ch);
        for (int i = 0; i < numSamples; ++i)
        {
            const float s = data[i];
            if (std::abs(s) > 2.0f)
                data[i] = FastMath::tanh(s * 0.5f) * 2.0f;
        }
    }

    // Measure output levels
    outputLevelLeft.store(buffer.getMagnitude(0, 0, buffer.getNumSamples()));
    if (buffer.getNumChannels() > 1)
        outputLevelRight.store(buffer.getMagnitude(1, 0, buffer.getNumSamples()));
}

void StardustProcessor::setCurrentProgram(int index)
{
    PresetLockGuard lock(presetLock);
    if (index < 0 || index >= static_cast<int>(allPresets.size())) return;
    loadPreset(index);
}

const juce::String StardustProcessor::getProgramName(int index)
{
    PresetLockGuard g(presetLock);
    if (index >= 0 && index < static_cast<int>(allPresets.size()))
        return allPresets[static_cast<size_t>(index)].name;
    return {};
}

void StardustProcessor::loadPreset(int index)
{
    // Copy values under lock, then apply outside lock to avoid deadlock
    // with host callbacks that may re-enter
    std::map<juce::String, float> values;
    {
        PresetLockGuard g(presetLock);
        if (index < 0 || index >= static_cast<int>(allPresets.size()))
            return;
        values = allPresets[static_cast<size_t>(index)].values;
        currentPresetIndex.store(index);
    }

    for (const auto& [paramId, value] : values)
    {
        if (auto* param = apvts.getParameter(paramId))
            param->setValueNotifyingHost(param->convertTo0to1(value));
    }

    // Auto-enable any effect that is assigned to a chain slot.
    // Prevents presets from having visible but disabled effects.
    static const std::pair<int, const char*> slotEnableMap[] = {
        { 1,  "destroyEnabled" },
        { 3,  "multiplyEnabled" },  // chorus shares multiply toggle
        { 4,  "tapeEnabled" },
        { 5,  "distortionEnabled" },
        { 6,  "reverbEnabled" },
        { 7,  "hazeEnabled" },
        { 8,  "unisonEnabled" },
        { 9,  "grainEnabled" },
        { 10, "stutterEnabled" },
        { 11, "shiftEnabled" },
        { 12, "reverserEnabled" },
        { 13, "halftimeEnabled" },
    };
    for (int slot = 0; slot < 4; ++slot)
    {
        const auto key = "chainSlot" + juce::String(slot);
        const auto it = values.find(key);
        if (it == values.end()) continue;
        const int fx = static_cast<int>(it->second);
        if (fx == 0) continue;
        for (const auto& [id, enableParam] : slotEnableMap)
        {
            if (id == fx)
            {
                if (auto* param = apvts.getParameter(enableParam))
                    param->setValueNotifyingHost(1.0f);
                break;
            }
        }
    }

    // Snapshot actual normalized values after host has applied them
    {
        PresetLockGuard g(presetLock);
        loadedPresetNormValues.clear();
        for (const auto& [paramId, value] : values)
        {
            if (auto* param = apvts.getParameter(paramId))
                loadedPresetNormValues[paramId] = param->getValue();
        }
        presetLoadGrace.store(10); // skip dirty check for 10 frames (~330ms)
    }

    presetDirty.store(false);
    presetChangeReset.store(true); // tell processBlock to flush DSP buffers
}

bool StardustProcessor::isFactoryPreset(int index) const
{
    PresetLockGuard g(presetLock);
    if (index < 0 || index >= static_cast<int>(allPresets.size()))
        return false;
    return allPresets[static_cast<size_t>(index)].isFactory;
}

juce::File StardustProcessor::getUserPresetsDir()
{
    auto dir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                   .getChildFile("Stardust")
                   .getChildFile("Presets");
    if (!dir.exists())
    {
        auto result = dir.createDirectory();
        if (result.failed())
            DBG("Failed to create presets directory: " + result.getErrorMessage());
    }
    return dir;
}

std::set<juce::String> StardustProcessor::loadFavorites()
{
    std::set<juce::String> favs;
    auto file = getUserPresetsDir().getChildFile("favorites.json");
    if (!file.existsAsFile()) return favs;

    auto text = file.loadFileAsString();
    auto json = juce::JSON::parse(text);
    if (auto* arr = json.getArray())
    {
        for (const auto& item : *arr)
            favs.insert(item.toString());
    }
    return favs;
}

void StardustProcessor::saveFavorites(const std::set<juce::String>& favs)
{
    juce::Array<juce::var> arr;
    for (const auto& name : favs)
        arr.add(juce::var(name));

    auto file = getUserPresetsDir().getChildFile("favorites.json");
    file.replaceWithText(juce::JSON::toString(juce::var(arr)));
}

void StardustProcessor::saveUserPreset(const juce::String& name)
{
    saveUserPreset(name, "");
}

void StardustProcessor::saveUserPreset(const juce::String& name, const juce::String& bank)
{
    auto safeName = name.removeCharacters("/\\:*?\"<>|").trim().substring(0, 64);
    if (safeName.isEmpty()) return;
    auto safeBank = bank.removeCharacters("/\\:*?\"<>|").trim().substring(0, 64);

    auto xml = std::make_unique<juce::XmlElement>("StardustPreset");
    xml->setAttribute("name", safeName);
    xml->setAttribute("version", STARDUST_VERSION);

    for (auto* param : getParameters())
    {
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*>(param))
        {
            auto* child = xml->createNewChildElement("Parameter");
            child->setAttribute("id", ranged->getParameterID());
            child->setAttribute("value", ranged->convertFrom0to1(ranged->getValue()));
        }
    }

    auto dir = getUserPresetsDir();
    if (safeBank.isNotEmpty())
    {
        dir = dir.getChildFile(safeBank);
        if (!dir.exists())
            dir.createDirectory();
    }

    auto file = dir.getChildFile(safeName + ".xml");
    if (!xml->writeTo(file))
        DBG("Failed to save preset: " + file.getFullPathName());
    presetDirty.store(false);
    refreshPresets();
}

void StardustProcessor::deleteUserPreset(int index)
{
    juce::String fileName;
    juce::String bankName;
    {
        PresetLockGuard g(presetLock);
        if (index < 0 || index >= static_cast<int>(allPresets.size()))
            return;
        if (allPresets[static_cast<size_t>(index)].isFactory)
            return;
        fileName = allPresets[static_cast<size_t>(index)].name;
        bankName = allPresets[static_cast<size_t>(index)].bank;
    }

    auto dir = getUserPresetsDir();
    if (bankName.isNotEmpty())
        dir = dir.getChildFile(bankName);

    auto file = dir.getChildFile(fileName + ".xml");
    if (file.existsAsFile() && !file.deleteFile())
        DBG("Failed to delete preset: " + file.getFullPathName());

    refreshPresets();
    {
        PresetLockGuard g(presetLock);
        if (currentPresetIndex.load() >= static_cast<int>(allPresets.size()))
            currentPresetIndex.store(0);
    }
}

void StardustProcessor::scanUserPresets()
{
    auto dir = getUserPresetsDir();

    // Helper: parse XML files from a directory into allPresets
    auto scanDir = [this](const juce::File& folder, const juce::String& bankName)
    {
        auto files = folder.findChildFiles(juce::File::findFiles, false, "*.xml");
        files.sort();

        for (const auto& file : files)
        {
            auto xml = juce::XmlDocument::parse(file);
            if (xml == nullptr || !xml->hasTagName("StardustPreset"))
                continue;

            Preset preset;
            preset.name = xml->getStringAttribute("name", file.getFileNameWithoutExtension()).substring(0, 32);
            preset.isFactory = false;
            preset.bank = bankName;

            for (auto* child : xml->getChildIterator())
            {
                if (child->hasTagName("Parameter"))
                {
                    auto id = child->getStringAttribute("id");
                    auto val = static_cast<float>(child->getDoubleAttribute("value"));
                    if (id.isNotEmpty())
                        preset.values[id] = val;
                }
            }

            allPresets.push_back(std::move(preset));
        }
    };

    // Scan root directory (USER bank)
    scanDir(dir, "");

    // Scan subdirectories (custom banks)
    auto subdirs = dir.findChildFiles(juce::File::findDirectories, false);
    subdirs.sort();
    for (const auto& subdir : subdirs)
        scanDir(subdir, subdir.getFileName());
}

void StardustProcessor::rebuildAllPresets()
{
    allPresets.clear();
    for (const auto& fp : factoryPresets)
    {
        Preset p = { fp.name, fp.values, true, fp.bank };
        allPresets.push_back(std::move(p));
    }
    scanUserPresets();
}

void StardustProcessor::refreshPresets()
{
    PresetLockGuard g(presetLock);
    rebuildAllPresets();
    if (currentPresetIndex.load() >= static_cast<int>(allPresets.size()))
        currentPresetIndex.store(0);
}

std::vector<juce::String> StardustProcessor::getUserBanks() const
{
    std::vector<juce::String> banks;
    auto dir = getUserPresetsDir();
    auto subdirs = dir.findChildFiles(juce::File::findDirectories, false);
    subdirs.sort();
    for (const auto& subdir : subdirs)
        banks.push_back(subdir.getFileName());
    return banks;
}

void StardustProcessor::importBank(const juce::File& sourceFolder)
{
    if (!sourceFolder.isDirectory())
        return;

    auto dir = getUserPresetsDir();
    auto bankName = sourceFolder.getFileName();

    // Avoid name collision
    auto target = dir.getChildFile(bankName);
    int suffix = 2;
    while (target.exists())
    {
        target = dir.getChildFile(bankName + " " + juce::String(suffix));
        ++suffix;
    }
    target.createDirectory();

    auto files = sourceFolder.findChildFiles(juce::File::findFiles, false, "*.xml");
    for (const auto& file : files)
    {
        if (file.isSymbolicLink() || file.getSize() > 1024 * 1024)
            continue;
        file.copyFileTo(target.getChildFile(file.getFileName()));
    }

    refreshPresets();
}

void StardustProcessor::deleteUserBank(const juce::String& bankName)
{
    if (bankName.isEmpty() || bankName.containsChar('/') || bankName.containsChar('\\') || bankName == ".." || bankName == ".")
        return;
    auto dir = getUserPresetsDir().getChildFile(bankName);
    if (!dir.isAChildOf(getUserPresetsDir()))
        return;
    if (dir.isDirectory())
        dir.deleteRecursively();
    refreshPresets();
}

void StardustProcessor::renameUserBank(const juce::String& oldName, const juce::String& newName)
{
    if (oldName.isEmpty() || newName.isEmpty() || oldName == newName)
        return;
    if (oldName.containsChar('/') || oldName.containsChar('\\') || oldName == ".." || oldName == ".")
        return;
    if (newName.containsChar('/') || newName.containsChar('\\') || newName == ".." || newName == ".")
        return;
    auto dir = getUserPresetsDir();
    auto oldDir = dir.getChildFile(oldName);
    auto newDir = dir.getChildFile(newName);
    if (!oldDir.isAChildOf(dir) || !newDir.isAChildOf(dir))
        return;
    if (oldDir.isDirectory() && !newDir.exists())
        oldDir.moveFileTo(newDir);
    refreshPresets();
}

void StardustProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    {
        PresetLockGuard g(presetLock);
        const int idx = currentPresetIndex.load();
        state.setProperty("presetIndex", idx, nullptr);
        if (idx >= 0 && idx < static_cast<int>(allPresets.size()))
            state.setProperty("presetName", allPresets[static_cast<size_t>(idx)].name, nullptr);
    }
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void StardustProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    if (xmlState != nullptr && xmlState->hasTagName(apvts.state.getType()))
    {
        auto tree = juce::ValueTree::fromXml(*xmlState);
        int savedIndex = tree.getProperty("presetIndex", 0);
        juce::String savedName = tree.getProperty("presetName", "");

        apvts.replaceState(tree);

        refreshPresets();
        {
            PresetLockGuard g(presetLock);
            bool found = false;
            if (savedName.isNotEmpty())
            {
                for (int i = 0; i < static_cast<int>(allPresets.size()); ++i)
                {
                    if (allPresets[static_cast<size_t>(i)].name == savedName)
                    {
                        currentPresetIndex.store(i);
                        found = true;
                        break;
                    }
                }
            }
            if (!found)
            {
                currentPresetIndex.store(juce::jlimit(0, juce::jmax(0, static_cast<int>(allPresets.size()) - 1), savedIndex));
            }

            // Snapshot current param values for dirty detection
            const int idx = currentPresetIndex.load();
            loadedPresetNormValues.clear();
            if (idx >= 0 && idx < static_cast<int>(allPresets.size()))
            {
                for (const auto& [paramId, val] : allPresets[static_cast<size_t>(idx)].values)
                {
                    if (auto* param = apvts.getParameter(paramId))
                        loadedPresetNormValues[paramId] = param->getValue();
                }
            }
            presetLoadGrace.store(10);
        }
    }
}

juce::AudioProcessorEditor* StardustProcessor::createEditor()
{
    return new StardustEditor(*this);
}

void StardustProcessor::initFactoryPresets()
{
    factoryPresets = getAllFactoryPresets();
}

#if 0 // Old factory presets removed — now in src/presets/ bank files
static void oldPresets() {
    std::vector<Preset> old = {
        { "Init", {
            {"chainSlot0", 0.0f}, {"chainSlot1", 0.0f}, {"chainSlot2", 0.0f}, {"chainSlot3", 0.0f}, {"destroyEnabled", 0.0f}, {"granularEnabled", 0.0f}, {"multiplyEnabled", 0.0f}, {"tapeEnabled", 0.0f}, {"destroyFader", 19000.0f}, {"destroyBits", 12.0f}, {"destroyIn", 0.0f}, {"destroyOut", 0.0f}, {"destroyMix", 1.0f}, {"filterCutoff", 1.0f}, {"filterLfo", 0.0f}, {"grainCloud", 0.3f}, {"grainDrift", 0.2f}, {"grainSpace", 0.3f}, {"grainMorph", 0.5f}, {"grainSizeSync", 0.0f}, {"grainReverse", 0.0f}, {"grainMix", 0.0f}, {"grainFreeze", 0.0f}, {"chorusSpeed", 1.0f}, {"chorusMix", 0.0f}, {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.8f}, {"tapeDrive", 0.02f}, {"tapeWear", 0.0f}, {"tapeGlue", 0.5f}, {"tapeNoise", 0.0f}, {"tapeNoiseSpeed", 1.0f}, {"tapeOutput", 0.0f}, {"tapeMix", 1.0f}, {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}, {"msWidth", 1.0f}, {"distortionEnabled", 0.0f}, {"distortionDrive", 0.5f}, {"distortionTone", 0.5f}, {"multiplyDepth", 0.13f}, {"multiplyTone", 0.7f}, {"distortionBias", 0.0f}, {"distortionMode", 0.0f}, {"grainShape", 0.0f}, {"distortionAsym", 0.5f}, {"multiplyFeedback", 0.0f}, {"multiplyLfoShape", 0.0f}, {"multiplyTempoSync", 0.0f}, {"multiplyShimmer", 0.2f}, {"grainPitchQuant", 0.0f}, {"grainHarm", 0.0f}
        }},
        { "SP-950 Classic", {
            {"chainSlot0", 1.0f}, {"chainSlot1", 0.0f}, {"chainSlot2", 0.0f}, {"chainSlot3", 0.0f}, {"destroyEnabled", 1.0f}, {"granularEnabled", 0.0f}, {"multiplyEnabled", 0.0f}, {"tapeEnabled", 0.0f}, {"destroyFader", 19000.0f}, {"destroyBits", 12.0f}, {"destroyIn", 0.0f}, {"destroyOut", 0.0f}, {"destroyMix", 1.0f}, {"filterCutoff", 0.85f}, {"filterLfo", 0.0f}, {"grainCloud", 0.3f}, {"grainDrift", 0.2f}, {"grainSpace", 0.3f}, {"grainMorph", 0.5f}, {"grainSizeSync", 0.0f}, {"grainReverse", 0.0f}, {"grainMix", 0.0f}, {"grainFreeze", 0.0f}, {"chorusSpeed", 1.0f}, {"chorusMix", 0.0f}, {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.8f}, {"tapeDrive", 0.02f}, {"tapeWear", 0.0f}, {"tapeGlue", 0.5f}, {"tapeNoise", 0.0f}, {"tapeNoiseSpeed", 1.0f}, {"tapeOutput", 0.0f}, {"tapeMix", 1.0f}, {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}, {"msWidth", 1.0f}, {"distortionEnabled", 0.0f}, {"distortionDrive", 0.5f}, {"distortionTone", 0.5f}, {"multiplyDepth", 0.13f}, {"multiplyTone", 0.7f}, {"distortionBias", 0.0f}, {"distortionMode", 0.0f}, {"grainShape", 0.0f}, {"distortionAsym", 0.5f}, {"multiplyFeedback", 0.0f}, {"multiplyLfoShape", 0.0f}, {"multiplyTempoSync", 0.0f}, {"multiplyShimmer", 0.2f}, {"grainPitchQuant", 0.0f}, {"grainHarm", 0.0f}
        }},
        { "SP-950 Scorched", {
            {"chainSlot0", 1.0f}, {"chainSlot1", 0.0f}, {"chainSlot2", 0.0f}, {"chainSlot3", 0.0f}, {"destroyEnabled", 1.0f}, {"granularEnabled", 0.0f}, {"multiplyEnabled", 0.0f}, {"tapeEnabled", 0.0f}, {"destroyFader", 19000.0f}, {"destroyBits", 12.0f}, {"destroyIn", 10.0f}, {"destroyOut", -4.0f}, {"destroyMix", 1.0f}, {"filterCutoff", 0.7f}, {"filterLfo", 0.0f}, {"grainCloud", 0.3f}, {"grainDrift", 0.2f}, {"grainSpace", 0.3f}, {"grainMorph", 0.5f}, {"grainSizeSync", 0.0f}, {"grainReverse", 0.0f}, {"grainMix", 0.0f}, {"grainFreeze", 0.0f}, {"chorusSpeed", 1.0f}, {"chorusMix", 0.0f}, {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.8f}, {"tapeDrive", 0.02f}, {"tapeWear", 0.0f}, {"tapeGlue", 0.5f}, {"tapeNoise", 0.0f}, {"tapeNoiseSpeed", 1.0f}, {"tapeOutput", 0.0f}, {"tapeMix", 1.0f}, {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}, {"msWidth", 1.0f}, {"distortionEnabled", 0.0f}, {"distortionDrive", 0.5f}, {"distortionTone", 0.5f}, {"multiplyDepth", 0.13f}, {"multiplyTone", 0.7f}, {"distortionBias", 0.0f}, {"distortionMode", 0.0f}, {"grainShape", 0.0f}, {"distortionAsym", 0.5f}, {"multiplyFeedback", 0.0f}, {"multiplyLfoShape", 0.0f}, {"multiplyTempoSync", 0.0f}, {"multiplyShimmer", 0.2f}, {"grainPitchQuant", 0.0f}, {"grainHarm", 0.0f}
        }},
        { "33 RPM Haze", {
            {"chainSlot0", 1.0f}, {"chainSlot1", 0.0f}, {"chainSlot2", 0.0f}, {"chainSlot3", 0.0f}, {"destroyEnabled", 1.0f}, {"granularEnabled", 0.0f}, {"multiplyEnabled", 0.0f}, {"tapeEnabled", 0.0f}, {"destroyFader", 26040.0f}, {"destroyBits", 12.0f}, {"destroyIn", 0.0f}, {"destroyOut", 0.0f}, {"destroyMix", 1.0f}, {"filterCutoff", 0.55f}, {"filterLfo", 0.0f}, {"grainCloud", 0.3f}, {"grainDrift", 0.2f}, {"grainSpace", 0.3f}, {"grainMorph", 0.5f}, {"grainSizeSync", 0.0f}, {"grainReverse", 0.0f}, {"grainMix", 0.0f}, {"grainFreeze", 0.0f}, {"chorusSpeed", 1.0f}, {"chorusMix", 0.0f}, {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.8f}, {"tapeDrive", 0.02f}, {"tapeWear", 0.0f}, {"tapeGlue", 0.5f}, {"tapeNoise", 0.0f}, {"tapeNoiseSpeed", 1.0f}, {"tapeOutput", 0.0f}, {"tapeMix", 1.0f}, {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}, {"msWidth", 1.0f}, {"distortionEnabled", 0.0f}, {"distortionDrive", 0.5f}, {"distortionTone", 0.5f}, {"multiplyDepth", 0.13f}, {"multiplyTone", 0.7f}, {"distortionBias", 0.0f}, {"distortionMode", 0.0f}, {"grainShape", 0.0f}, {"distortionAsym", 0.5f}, {"multiplyFeedback", 0.0f}, {"multiplyLfoShape", 0.0f}, {"multiplyTempoSync", 0.0f}, {"multiplyShimmer", 0.2f}, {"grainPitchQuant", 0.0f}, {"grainHarm", 0.0f}
        }},
        { "78 RPM Shellac", {
            {"chainSlot0", 1.0f}, {"chainSlot1", 0.0f}, {"chainSlot2", 0.0f}, {"chainSlot3", 0.0f}, {"destroyEnabled", 1.0f}, {"granularEnabled", 0.0f}, {"multiplyEnabled", 0.0f}, {"tapeEnabled", 0.0f}, {"destroyFader", 4000.0f}, {"destroyBits", 12.0f}, {"destroyIn", 2.0f}, {"destroyOut", 0.0f}, {"destroyMix", 1.0f}, {"filterCutoff", 0.95f}, {"filterLfo", 0.0f}, {"grainCloud", 0.3f}, {"grainDrift", 0.2f}, {"grainSpace", 0.3f}, {"grainMorph", 0.5f}, {"grainSizeSync", 0.0f}, {"grainReverse", 0.0f}, {"grainMix", 0.0f}, {"grainFreeze", 0.0f}, {"chorusSpeed", 1.0f}, {"chorusMix", 0.0f}, {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.8f}, {"tapeDrive", 0.02f}, {"tapeWear", 0.0f}, {"tapeGlue", 0.5f}, {"tapeNoise", 0.0f}, {"tapeNoiseSpeed", 1.0f}, {"tapeOutput", 0.0f}, {"tapeMix", 1.0f}, {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}, {"msWidth", 1.0f}, {"distortionEnabled", 0.0f}, {"distortionDrive", 0.5f}, {"distortionTone", 0.5f}, {"multiplyDepth", 0.13f}, {"multiplyTone", 0.7f}, {"distortionBias", 0.0f}, {"distortionMode", 0.0f}, {"grainShape", 0.0f}, {"distortionAsym", 0.5f}, {"multiplyFeedback", 0.0f}, {"multiplyLfoShape", 0.0f}, {"multiplyTempoSync", 0.0f}, {"multiplyShimmer", 0.2f}, {"grainPitchQuant", 0.0f}, {"grainHarm", 0.0f}
        }},
        { "ADC Overdrive", {
            {"chainSlot0", 1.0f}, {"chainSlot1", 0.0f}, {"chainSlot2", 0.0f}, {"chainSlot3", 0.0f}, {"destroyEnabled", 1.0f}, {"granularEnabled", 0.0f}, {"multiplyEnabled", 0.0f}, {"tapeEnabled", 0.0f}, {"destroyFader", 19000.0f}, {"destroyBits", 12.0f}, {"destroyIn", 12.0f}, {"destroyOut", -6.0f}, {"destroyMix", 1.0f}, {"filterCutoff", 0.9f}, {"filterLfo", 0.0f}, {"grainCloud", 0.3f}, {"grainDrift", 0.2f}, {"grainSpace", 0.3f}, {"grainMorph", 0.5f}, {"grainSizeSync", 0.0f}, {"grainReverse", 0.0f}, {"grainMix", 0.0f}, {"grainFreeze", 0.0f}, {"chorusSpeed", 1.0f}, {"chorusMix", 0.0f}, {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.8f}, {"tapeDrive", 0.02f}, {"tapeWear", 0.0f}, {"tapeGlue", 0.5f}, {"tapeNoise", 0.0f}, {"tapeNoiseSpeed", 1.0f}, {"tapeOutput", 0.0f}, {"tapeMix", 1.0f}, {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}, {"msWidth", 1.0f}, {"distortionEnabled", 0.0f}, {"distortionDrive", 0.5f}, {"distortionTone", 0.5f}, {"multiplyDepth", 0.13f}, {"multiplyTone", 0.7f}, {"distortionBias", 0.0f}, {"distortionMode", 0.0f}, {"grainShape", 0.0f}, {"distortionAsym", 0.5f}, {"multiplyFeedback", 0.0f}, {"multiplyLfoShape", 0.0f}, {"multiplyTempoSync", 0.0f}, {"multiplyShimmer", 0.2f}, {"grainPitchQuant", 0.0f}, {"grainHarm", 0.0f}
        }},
        { "Muffled Sampler", {
            {"chainSlot0", 1.0f}, {"chainSlot1", 0.0f}, {"chainSlot2", 0.0f}, {"chainSlot3", 0.0f}, {"destroyEnabled", 1.0f}, {"granularEnabled", 0.0f}, {"multiplyEnabled", 0.0f}, {"tapeEnabled", 0.0f}, {"destroyFader", 19000.0f}, {"destroyBits", 12.0f}, {"destroyIn", 0.0f}, {"destroyOut", 0.0f}, {"destroyMix", 1.0f}, {"filterCutoff", 0.25f}, {"filterLfo", 0.0f}, {"grainCloud", 0.3f}, {"grainDrift", 0.2f}, {"grainSpace", 0.3f}, {"grainMorph", 0.5f}, {"grainSizeSync", 0.0f}, {"grainReverse", 0.0f}, {"grainMix", 0.0f}, {"grainFreeze", 0.0f}, {"chorusSpeed", 1.0f}, {"chorusMix", 0.0f}, {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.8f}, {"tapeDrive", 0.02f}, {"tapeWear", 0.0f}, {"tapeGlue", 0.5f}, {"tapeNoise", 0.0f}, {"tapeNoiseSpeed", 1.0f}, {"tapeOutput", 0.0f}, {"tapeMix", 1.0f}, {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}, {"msWidth", 1.0f}, {"distortionEnabled", 0.0f}, {"distortionDrive", 0.5f}, {"distortionTone", 0.5f}, {"multiplyDepth", 0.13f}, {"multiplyTone", 0.7f}, {"distortionBias", 0.0f}, {"distortionMode", 0.0f}, {"grainShape", 0.0f}, {"distortionAsym", 0.5f}, {"multiplyFeedback", 0.0f}, {"multiplyLfoShape", 0.0f}, {"multiplyTempoSync", 0.0f}, {"multiplyShimmer", 0.2f}, {"grainPitchQuant", 0.0f}, {"grainHarm", 0.0f}
        }},
        { "Telephone Line", {
            {"chainSlot0", 1.0f}, {"chainSlot1", 0.0f}, {"chainSlot2", 0.0f}, {"chainSlot3", 0.0f}, {"destroyEnabled", 1.0f}, {"granularEnabled", 0.0f}, {"multiplyEnabled", 0.0f}, {"tapeEnabled", 0.0f}, {"destroyFader", 8000.0f}, {"destroyBits", 12.0f}, {"destroyIn", 4.0f}, {"destroyOut", -2.0f}, {"destroyMix", 1.0f}, {"filterCutoff", 0.2f}, {"filterLfo", 0.0f}, {"grainCloud", 0.3f}, {"grainDrift", 0.2f}, {"grainSpace", 0.3f}, {"grainMorph", 0.5f}, {"grainSizeSync", 0.0f}, {"grainReverse", 0.0f}, {"grainMix", 0.0f}, {"grainFreeze", 0.0f}, {"chorusSpeed", 1.0f}, {"chorusMix", 0.0f}, {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.8f}, {"tapeDrive", 0.02f}, {"tapeWear", 0.0f}, {"tapeGlue", 0.5f}, {"tapeNoise", 0.0f}, {"tapeNoiseSpeed", 1.0f}, {"tapeOutput", 0.0f}, {"tapeMix", 1.0f}, {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}, {"msWidth", 1.0f}, {"distortionEnabled", 0.0f}, {"distortionDrive", 0.5f}, {"distortionTone", 0.5f}, {"multiplyDepth", 0.13f}, {"multiplyTone", 0.7f}, {"distortionBias", 0.0f}, {"distortionMode", 0.0f}, {"grainShape", 0.0f}, {"distortionAsym", 0.5f}, {"multiplyFeedback", 0.0f}, {"multiplyLfoShape", 0.0f}, {"multiplyTempoSync", 0.0f}, {"multiplyShimmer", 0.2f}, {"grainPitchQuant", 0.0f}, {"grainHarm", 0.0f}
        }},
        { "Digital Grit", {
            {"chainSlot0", 1.0f}, {"chainSlot1", 0.0f}, {"chainSlot2", 0.0f}, {"chainSlot3", 0.0f}, {"destroyEnabled", 1.0f}, {"granularEnabled", 0.0f}, {"multiplyEnabled", 0.0f}, {"tapeEnabled", 0.0f}, {"destroyFader", 8000.0f}, {"destroyBits", 8.0f}, {"destroyIn", 6.0f}, {"destroyOut", -3.0f}, {"destroyMix", 1.0f}, {"filterCutoff", 0.75f}, {"filterLfo", 0.0f}, {"grainCloud", 0.3f}, {"grainDrift", 0.2f}, {"grainSpace", 0.3f}, {"grainMorph", 0.5f}, {"grainSizeSync", 0.0f}, {"grainReverse", 0.0f}, {"grainMix", 0.0f}, {"grainFreeze", 0.0f}, {"chorusSpeed", 1.0f}, {"chorusMix", 0.0f}, {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.8f}, {"tapeDrive", 0.02f}, {"tapeWear", 0.0f}, {"tapeGlue", 0.5f}, {"tapeNoise", 0.0f}, {"tapeNoiseSpeed", 1.0f}, {"tapeOutput", 0.0f}, {"tapeMix", 1.0f}, {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}, {"msWidth", 1.0f}, {"distortionEnabled", 0.0f}, {"distortionDrive", 0.5f}, {"distortionTone", 0.5f}, {"multiplyDepth", 0.13f}, {"multiplyTone", 0.7f}, {"distortionBias", 0.0f}, {"distortionMode", 0.0f}, {"grainShape", 0.0f}, {"distortionAsym", 0.5f}, {"multiplyFeedback", 0.0f}, {"multiplyLfoShape", 0.0f}, {"multiplyTempoSync", 0.0f}, {"multiplyShimmer", 0.2f}, {"grainPitchQuant", 0.0f}, {"grainHarm", 0.0f}
        }},
        { "Warm Vinyl", {
            {"chainSlot0", 1.0f}, {"chainSlot1", 0.0f}, {"chainSlot2", 0.0f}, {"chainSlot3", 0.0f}, {"destroyEnabled", 1.0f}, {"granularEnabled", 0.0f}, {"multiplyEnabled", 0.0f}, {"tapeEnabled", 0.0f}, {"destroyFader", 26040.0f}, {"destroyBits", 12.0f}, {"destroyIn", 0.0f}, {"destroyOut", 0.0f}, {"destroyMix", 0.7f}, {"filterCutoff", 0.6f}, {"filterLfo", 0.0f}, {"grainCloud", 0.3f}, {"grainDrift", 0.2f}, {"grainSpace", 0.3f}, {"grainMorph", 0.5f}, {"grainSizeSync", 0.0f}, {"grainReverse", 0.0f}, {"grainMix", 0.0f}, {"grainFreeze", 0.0f}, {"chorusSpeed", 1.0f}, {"chorusMix", 0.0f}, {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.8f}, {"tapeDrive", 0.02f}, {"tapeWear", 0.0f}, {"tapeGlue", 0.5f}, {"tapeNoise", 0.0f}, {"tapeNoiseSpeed", 1.0f}, {"tapeOutput", 0.0f}, {"tapeMix", 1.0f}, {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}, {"msWidth", 1.0f}, {"distortionEnabled", 0.0f}, {"distortionDrive", 0.5f}, {"distortionTone", 0.5f}, {"multiplyDepth", 0.13f}, {"multiplyTone", 0.7f}, {"distortionBias", 0.0f}, {"distortionMode", 0.0f}, {"grainShape", 0.0f}, {"distortionAsym", 0.5f}, {"multiplyFeedback", 0.0f}, {"multiplyLfoShape", 0.0f}, {"multiplyTempoSync", 0.0f}, {"multiplyShimmer", 0.2f}, {"grainPitchQuant", 0.0f}, {"grainHarm", 0.0f}
        }},
        { "Lo-Fi Radio", {
            {"chainSlot0", 1.0f}, {"chainSlot1", 0.0f}, {"chainSlot2", 0.0f}, {"chainSlot3", 0.0f}, {"destroyEnabled", 1.0f}, {"granularEnabled", 0.0f}, {"multiplyEnabled", 0.0f}, {"tapeEnabled", 0.0f}, {"destroyFader", 19000.0f}, {"destroyBits", 12.0f}, {"destroyIn", 3.0f}, {"destroyOut", 0.0f}, {"destroyMix", 1.0f}, {"filterCutoff", 0.4f}, {"filterLfo", 0.15f}, {"grainCloud", 0.3f}, {"grainDrift", 0.2f}, {"grainSpace", 0.3f}, {"grainMorph", 0.5f}, {"grainSizeSync", 0.0f}, {"grainReverse", 0.0f}, {"grainMix", 0.0f}, {"grainFreeze", 0.0f}, {"chorusSpeed", 1.0f}, {"chorusMix", 0.0f}, {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.8f}, {"tapeDrive", 0.02f}, {"tapeWear", 0.0f}, {"tapeGlue", 0.5f}, {"tapeNoise", 0.0f}, {"tapeNoiseSpeed", 1.0f}, {"tapeOutput", 0.0f}, {"tapeMix", 1.0f}, {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}, {"msWidth", 1.0f}, {"distortionEnabled", 0.0f}, {"distortionDrive", 0.5f}, {"distortionTone", 0.5f}, {"multiplyDepth", 0.13f}, {"multiplyTone", 0.7f}, {"distortionBias", 0.0f}, {"distortionMode", 0.0f}, {"grainShape", 0.0f}, {"distortionAsym", 0.5f}, {"multiplyFeedback", 0.0f}, {"multiplyLfoShape", 0.0f}, {"multiplyTempoSync", 0.0f}, {"multiplyShimmer", 0.2f}, {"grainPitchQuant", 0.0f}, {"grainHarm", 0.0f}
        }},
        { "8-Bit Nostalgia", {
            {"chainSlot0", 1.0f}, {"chainSlot1", 0.0f}, {"chainSlot2", 0.0f}, {"chainSlot3", 0.0f}, {"destroyEnabled", 1.0f}, {"granularEnabled", 0.0f}, {"multiplyEnabled", 0.0f}, {"tapeEnabled", 0.0f}, {"destroyFader", 6000.0f}, {"destroyBits", 8.0f}, {"destroyIn", 3.0f}, {"destroyOut", 0.0f}, {"destroyMix", 1.0f}, {"filterCutoff", 0.65f}, {"filterLfo", 0.0f}, {"grainCloud", 0.3f}, {"grainDrift", 0.2f}, {"grainSpace", 0.3f}, {"grainMorph", 0.5f}, {"grainSizeSync", 0.0f}, {"grainReverse", 0.0f}, {"grainMix", 0.0f}, {"grainFreeze", 0.0f}, {"chorusSpeed", 1.0f}, {"chorusMix", 0.0f}, {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.8f}, {"tapeDrive", 0.02f}, {"tapeWear", 0.0f}, {"tapeGlue", 0.5f}, {"tapeNoise", 0.0f}, {"tapeNoiseSpeed", 1.0f}, {"tapeOutput", 0.0f}, {"tapeMix", 1.0f}, {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}, {"msWidth", 1.0f}, {"distortionEnabled", 0.0f}, {"distortionDrive", 0.5f}, {"distortionTone", 0.5f}, {"multiplyDepth", 0.13f}, {"multiplyTone", 0.7f}, {"distortionBias", 0.0f}, {"distortionMode", 0.0f}, {"grainShape", 0.0f}, {"distortionAsym", 0.5f}, {"multiplyFeedback", 0.0f}, {"multiplyLfoShape", 0.0f}, {"multiplyTempoSync", 0.0f}, {"multiplyShimmer", 0.2f}, {"grainPitchQuant", 0.0f}, {"grainHarm", 0.0f}
        }},
        { "Subtle Color", {
            {"chainSlot0", 1.0f}, {"chainSlot1", 0.0f}, {"chainSlot2", 0.0f}, {"chainSlot3", 0.0f}, {"destroyEnabled", 1.0f}, {"granularEnabled", 0.0f}, {"multiplyEnabled", 0.0f}, {"tapeEnabled", 0.0f}, {"destroyFader", 19000.0f}, {"destroyBits", 12.0f}, {"destroyIn", 0.0f}, {"destroyOut", 0.0f}, {"destroyMix", 0.35f}, {"filterCutoff", 0.9f}, {"filterLfo", 0.0f}, {"grainCloud", 0.3f}, {"grainDrift", 0.2f}, {"grainSpace", 0.3f}, {"grainMorph", 0.5f}, {"grainSizeSync", 0.0f}, {"grainReverse", 0.0f}, {"grainMix", 0.0f}, {"grainFreeze", 0.0f}, {"chorusSpeed", 1.0f}, {"chorusMix", 0.0f}, {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.8f}, {"tapeDrive", 0.02f}, {"tapeWear", 0.0f}, {"tapeGlue", 0.5f}, {"tapeNoise", 0.0f}, {"tapeNoiseSpeed", 1.0f}, {"tapeOutput", 0.0f}, {"tapeMix", 1.0f}, {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}, {"msWidth", 1.0f}, {"distortionEnabled", 0.0f}, {"distortionDrive", 0.5f}, {"distortionTone", 0.5f}, {"multiplyDepth", 0.13f}, {"multiplyTone", 0.7f}, {"distortionBias", 0.0f}, {"distortionMode", 0.0f}, {"grainShape", 0.0f}, {"distortionAsym", 0.5f}, {"multiplyFeedback", 0.0f}, {"multiplyLfoShape", 0.0f}, {"multiplyTempoSync", 0.0f}, {"multiplyShimmer", 0.2f}, {"grainPitchQuant", 0.0f}, {"grainHarm", 0.0f}
        }},
        { "Bitcrushed 4-bit", {
            {"chainSlot0", 1.0f}, {"chainSlot1", 0.0f}, {"chainSlot2", 0.0f}, {"chainSlot3", 0.0f}, {"destroyEnabled", 1.0f}, {"granularEnabled", 0.0f}, {"multiplyEnabled", 0.0f}, {"tapeEnabled", 0.0f}, {"destroyFader", 6000.0f}, {"destroyBits", 4.0f}, {"destroyIn", 6.0f}, {"destroyOut", -4.0f}, {"destroyMix", 1.0f}, {"filterCutoff", 0.8f}, {"filterLfo", 0.0f}, {"grainCloud", 0.3f}, {"grainDrift", 0.2f}, {"grainSpace", 0.3f}, {"grainMorph", 0.5f}, {"grainSizeSync", 0.0f}, {"grainReverse", 0.0f}, {"grainMix", 0.0f}, {"grainFreeze", 0.0f}, {"chorusSpeed", 1.0f}, {"chorusMix", 0.0f}, {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.8f}, {"tapeDrive", 0.02f}, {"tapeWear", 0.0f}, {"tapeGlue", 0.5f}, {"tapeNoise", 0.0f}, {"tapeNoiseSpeed", 1.0f}, {"tapeOutput", 0.0f}, {"tapeMix", 1.0f}, {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}, {"msWidth", 1.0f}, {"distortionEnabled", 0.0f}, {"distortionDrive", 0.5f}, {"distortionTone", 0.5f}, {"multiplyDepth", 0.13f}, {"multiplyTone", 0.7f}, {"distortionBias", 0.0f}, {"distortionMode", 0.0f}, {"grainShape", 0.0f}, {"distortionAsym", 0.5f}, {"multiplyFeedback", 0.0f}, {"multiplyLfoShape", 0.0f}, {"multiplyTempoSync", 0.0f}, {"multiplyShimmer", 0.2f}, {"grainPitchQuant", 0.0f}, {"grainHarm", 0.0f}
        }},
        { "SP-950 Filtered", {
            {"chainSlot0", 1.0f}, {"chainSlot1", 0.0f}, {"chainSlot2", 0.0f}, {"chainSlot3", 0.0f}, {"destroyEnabled", 1.0f}, {"granularEnabled", 0.0f}, {"multiplyEnabled", 0.0f}, {"tapeEnabled", 0.0f}, {"destroyFader", 19000.0f}, {"destroyBits", 12.0f}, {"destroyIn", 0.0f}, {"destroyOut", 0.0f}, {"destroyMix", 1.0f}, {"filterCutoff", 0.5f}, {"filterLfo", 0.3f}, {"grainCloud", 0.3f}, {"grainDrift", 0.2f}, {"grainSpace", 0.3f}, {"grainMorph", 0.5f}, {"grainSizeSync", 0.0f}, {"grainReverse", 0.0f}, {"grainMix", 0.0f}, {"grainFreeze", 0.0f}, {"chorusSpeed", 1.0f}, {"chorusMix", 0.0f}, {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.8f}, {"tapeDrive", 0.02f}, {"tapeWear", 0.0f}, {"tapeGlue", 0.5f}, {"tapeNoise", 0.0f}, {"tapeNoiseSpeed", 1.0f}, {"tapeOutput", 0.0f}, {"tapeMix", 1.0f}, {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}, {"msWidth", 1.0f}, {"distortionEnabled", 0.0f}, {"distortionDrive", 0.5f}, {"distortionTone", 0.5f}, {"multiplyDepth", 0.13f}, {"multiplyTone", 0.7f}, {"distortionBias", 0.0f}, {"distortionMode", 0.0f}, {"grainShape", 0.0f}, {"distortionAsym", 0.5f}, {"multiplyFeedback", 0.0f}, {"multiplyLfoShape", 0.0f}, {"multiplyTempoSync", 0.0f}, {"multiplyShimmer", 0.2f}, {"grainPitchQuant", 0.0f}, {"grainHarm", 0.0f}
        }},
        { "Morning Fog", {
            {"chainSlot0", 2.0f}, {"chainSlot1", 0.0f}, {"chainSlot2", 0.0f}, {"chainSlot3", 0.0f}, {"destroyEnabled", 0.0f}, {"granularEnabled", 1.0f}, {"multiplyEnabled", 0.0f}, {"tapeEnabled", 0.0f}, {"destroyFader", 19000.0f}, {"destroyBits", 12.0f}, {"destroyIn", 0.0f}, {"destroyOut", 0.0f}, {"destroyMix", 1.0f}, {"filterCutoff", 1.0f}, {"filterLfo", 0.0f}, {"grainCloud", 0.2f}, {"grainDrift", 0.1f}, {"grainSpace", 0.5f}, {"grainMorph", 0.5f}, {"grainSizeSync", 0.0f}, {"grainReverse", 0.0f}, {"grainMix", 0.6f}, {"grainFreeze", 0.0f}, {"chorusSpeed", 1.0f}, {"chorusMix", 0.0f}, {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.8f}, {"tapeDrive", 0.02f}, {"tapeWear", 0.0f}, {"tapeGlue", 0.5f}, {"tapeNoise", 0.0f}, {"tapeNoiseSpeed", 1.0f}, {"tapeOutput", 0.0f}, {"tapeMix", 1.0f}, {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}, {"msWidth", 1.0f}, {"distortionEnabled", 0.0f}, {"distortionDrive", 0.5f}, {"distortionTone", 0.5f}, {"multiplyDepth", 0.13f}, {"multiplyTone", 0.7f}, {"distortionBias", 0.0f}, {"distortionMode", 0.0f}, {"grainShape", 0.0f}, {"distortionAsym", 0.5f}, {"multiplyFeedback", 0.0f}, {"multiplyLfoShape", 0.0f}, {"multiplyTempoSync", 0.0f}, {"multiplyShimmer", 0.2f}, {"grainPitchQuant", 0.0f}, {"grainHarm", 0.0f}
        }},
        { "Shimmer Pad", {
            {"chainSlot0", 2.0f}, {"chainSlot1", 0.0f}, {"chainSlot2", 0.0f}, {"chainSlot3", 0.0f}, {"destroyEnabled", 0.0f}, {"granularEnabled", 1.0f}, {"multiplyEnabled", 0.0f}, {"tapeEnabled", 0.0f}, {"destroyFader", 19000.0f}, {"destroyBits", 12.0f}, {"destroyIn", 0.0f}, {"destroyOut", 0.0f}, {"destroyMix", 1.0f}, {"filterCutoff", 1.0f}, {"filterLfo", 0.0f}, {"grainCloud", 0.8f}, {"grainDrift", 0.15f}, {"grainSpace", 0.7f}, {"grainMorph", 0.62f}, {"grainSizeSync", 0.0f}, {"grainReverse", 0.0f}, {"grainMix", 0.75f}, {"grainFreeze", 0.0f}, {"chorusSpeed", 1.0f}, {"chorusMix", 0.0f}, {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.8f}, {"tapeDrive", 0.02f}, {"tapeWear", 0.0f}, {"tapeGlue", 0.5f}, {"tapeNoise", 0.0f}, {"tapeNoiseSpeed", 1.0f}, {"tapeOutput", 0.0f}, {"tapeMix", 1.0f}, {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}, {"msWidth", 1.3f}, {"distortionEnabled", 0.0f}, {"distortionDrive", 0.5f}, {"distortionTone", 0.5f}, {"multiplyDepth", 0.13f}, {"multiplyTone", 0.7f}, {"distortionBias", 0.0f}, {"distortionMode", 0.0f}, {"grainShape", 0.0f}, {"distortionAsym", 0.5f}, {"multiplyFeedback", 0.0f}, {"multiplyLfoShape", 0.0f}, {"multiplyTempoSync", 0.0f}, {"multiplyShimmer", 0.2f}, {"grainPitchQuant", 0.0f}, {"grainHarm", 0.0f}
        }},
        { "Frozen Cathedral", {
            {"chainSlot0", 2.0f}, {"chainSlot1", 0.0f}, {"chainSlot2", 0.0f}, {"chainSlot3", 0.0f}, {"destroyEnabled", 0.0f}, {"granularEnabled", 1.0f}, {"multiplyEnabled", 0.0f}, {"tapeEnabled", 0.0f}, {"destroyFader", 19000.0f}, {"destroyBits", 12.0f}, {"destroyIn", 0.0f}, {"destroyOut", 0.0f}, {"destroyMix", 1.0f}, {"filterCutoff", 1.0f}, {"filterLfo", 0.0f}, {"grainCloud", 0.4f}, {"grainDrift", 0.05f}, {"grainSpace", 0.9f}, {"grainMorph", 0.5f}, {"grainSizeSync", 0.0f}, {"grainReverse", 0.0f}, {"grainMix", 1.0f}, {"grainFreeze", 1.0f}, {"chorusSpeed", 1.0f}, {"chorusMix", 0.0f}, {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.8f}, {"tapeDrive", 0.02f}, {"tapeWear", 0.0f}, {"tapeGlue", 0.5f}, {"tapeNoise", 0.0f}, {"tapeNoiseSpeed", 1.0f}, {"tapeOutput", 0.0f}, {"tapeMix", 1.0f}, {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}, {"msWidth", 1.5f}, {"distortionEnabled", 0.0f}, {"distortionDrive", 0.5f}, {"distortionTone", 0.5f}, {"multiplyDepth", 0.13f}, {"multiplyTone", 0.7f}, {"distortionBias", 0.0f}, {"distortionMode", 0.0f}, {"grainShape", 0.0f}, {"distortionAsym", 0.5f}, {"multiplyFeedback", 0.0f}, {"multiplyLfoShape", 0.0f}, {"multiplyTempoSync", 0.0f}, {"multiplyShimmer", 0.2f}, {"grainPitchQuant", 0.0f}, {"grainHarm", 0.0f}
        }},
        { "Scattered Glass", {
            {"chainSlot0", 2.0f}, {"chainSlot1", 0.0f}, {"chainSlot2", 0.0f}, {"chainSlot3", 0.0f}, {"destroyEnabled", 0.0f}, {"granularEnabled", 1.0f}, {"multiplyEnabled", 0.0f}, {"tapeEnabled", 0.0f}, {"destroyFader", 19000.0f}, {"destroyBits", 12.0f}, {"destroyIn", 0.0f}, {"destroyOut", 0.0f}, {"destroyMix", 1.0f}, {"filterCutoff", 1.0f}, {"filterLfo", 0.0f}, {"grainCloud", 0.9f}, {"grainDrift", 0.8f}, {"grainSpace", 0.4f}, {"grainMorph", 0.5f}, {"grainSizeSync", 0.0f}, {"grainReverse", 0.0f}, {"grainMix", 0.8f}, {"grainFreeze", 0.0f}, {"chorusSpeed", 1.0f}, {"chorusMix", 0.0f}, {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.8f}, {"tapeDrive", 0.02f}, {"tapeWear", 0.0f}, {"tapeGlue", 0.5f}, {"tapeNoise", 0.0f}, {"tapeNoiseSpeed", 1.0f}, {"tapeOutput", 0.0f}, {"tapeMix", 1.0f}, {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}, {"msWidth", 1.0f}, {"distortionEnabled", 0.0f}, {"distortionDrive", 0.5f}, {"distortionTone", 0.5f}, {"multiplyDepth", 0.13f}, {"multiplyTone", 0.7f}, {"distortionBias", 0.0f}, {"distortionMode", 0.0f}, {"grainShape", 0.0f}, {"distortionAsym", 0.5f}, {"multiplyFeedback", 0.0f}, {"multiplyLfoShape", 0.0f}, {"multiplyTempoSync", 0.0f}, {"multiplyShimmer", 0.2f}, {"grainPitchQuant", 0.0f}, {"grainHarm", 0.0f}
        }},
        { "Pitch Cloud Up", {
            {"chainSlot0", 2.0f}, {"chainSlot1", 0.0f}, {"chainSlot2", 0.0f}, {"chainSlot3", 0.0f}, {"destroyEnabled", 0.0f}, {"granularEnabled", 1.0f}, {"multiplyEnabled", 0.0f}, {"tapeEnabled", 0.0f}, {"destroyFader", 19000.0f}, {"destroyBits", 12.0f}, {"destroyIn", 0.0f}, {"destroyOut", 0.0f}, {"destroyMix", 1.0f}, {"filterCutoff", 1.0f}, {"filterLfo", 0.0f}, {"grainCloud", 0.6f}, {"grainDrift", 0.2f}, {"grainSpace", 0.5f}, {"grainMorph", 0.75f}, {"grainSizeSync", 0.0f}, {"grainReverse", 0.0f}, {"grainMix", 0.7f}, {"grainFreeze", 0.0f}, {"chorusSpeed", 1.0f}, {"chorusMix", 0.0f}, {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.8f}, {"tapeDrive", 0.02f}, {"tapeWear", 0.0f}, {"tapeGlue", 0.5f}, {"tapeNoise", 0.0f}, {"tapeNoiseSpeed", 1.0f}, {"tapeOutput", 0.0f}, {"tapeMix", 1.0f}, {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}, {"msWidth", 1.0f}, {"distortionEnabled", 0.0f}, {"distortionDrive", 0.5f}, {"distortionTone", 0.5f}, {"multiplyDepth", 0.13f}, {"multiplyTone", 0.7f}, {"distortionBias", 0.0f}, {"distortionMode", 0.0f}, {"grainShape", 0.0f}, {"distortionAsym", 0.5f}, {"multiplyFeedback", 0.0f}, {"multiplyLfoShape", 0.0f}, {"multiplyTempoSync", 0.0f}, {"multiplyShimmer", 0.2f}, {"grainPitchQuant", 0.0f}, {"grainHarm", 0.0f}
        }},
        { "Pitch Cloud Down", {
            {"chainSlot0", 2.0f}, {"chainSlot1", 0.0f}, {"chainSlot2", 0.0f}, {"chainSlot3", 0.0f}, {"destroyEnabled", 0.0f}, {"granularEnabled", 1.0f}, {"multiplyEnabled", 0.0f}, {"tapeEnabled", 0.0f}, {"destroyFader", 19000.0f}, {"destroyBits", 12.0f}, {"destroyIn", 0.0f}, {"destroyOut", 0.0f}, {"destroyMix", 1.0f}, {"filterCutoff", 1.0f}, {"filterLfo", 0.0f}, {"grainCloud", 0.6f}, {"grainDrift", 0.2f}, {"grainSpace", 0.5f}, {"grainMorph", 0.25f}, {"grainSizeSync", 0.0f}, {"grainReverse", 0.0f}, {"grainMix", 0.7f}, {"grainFreeze", 0.0f}, {"chorusSpeed", 1.0f}, {"chorusMix", 0.0f}, {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.8f}, {"tapeDrive", 0.02f}, {"tapeWear", 0.0f}, {"tapeGlue", 0.5f}, {"tapeNoise", 0.0f}, {"tapeNoiseSpeed", 1.0f}, {"tapeOutput", 0.0f}, {"tapeMix", 1.0f}, {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}, {"msWidth", 1.0f}, {"distortionEnabled", 0.0f}, {"distortionDrive", 0.5f}, {"distortionTone", 0.5f}, {"multiplyDepth", 0.13f}, {"multiplyTone", 0.7f}, {"distortionBias", 0.0f}, {"distortionMode", 0.0f}, {"grainShape", 0.0f}, {"distortionAsym", 0.5f}, {"multiplyFeedback", 0.0f}, {"multiplyLfoShape", 0.0f}, {"multiplyTempoSync", 0.0f}, {"multiplyShimmer", 0.2f}, {"grainPitchQuant", 0.0f}, {"grainHarm", 0.0f}
        }},
        { "Ambient Dust", {
            {"chainSlot0", 2.0f}, {"chainSlot1", 0.0f}, {"chainSlot2", 0.0f}, {"chainSlot3", 0.0f}, {"destroyEnabled", 0.0f}, {"granularEnabled", 1.0f}, {"multiplyEnabled", 0.0f}, {"tapeEnabled", 0.0f}, {"destroyFader", 19000.0f}, {"destroyBits", 12.0f}, {"destroyIn", 0.0f}, {"destroyOut", 0.0f}, {"destroyMix", 1.0f}, {"filterCutoff", 1.0f}, {"filterLfo", 0.0f}, {"grainCloud", 0.5f}, {"grainDrift", 0.4f}, {"grainSpace", 0.65f}, {"grainMorph", 0.52f}, {"grainSizeSync", 0.0f}, {"grainReverse", 0.0f}, {"grainMix", 0.5f}, {"grainFreeze", 0.0f}, {"chorusSpeed", 1.0f}, {"chorusMix", 0.0f}, {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.8f}, {"tapeDrive", 0.02f}, {"tapeWear", 0.0f}, {"tapeGlue", 0.5f}, {"tapeNoise", 0.0f}, {"tapeNoiseSpeed", 1.0f}, {"tapeOutput", 0.0f}, {"tapeMix", 1.0f}, {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}, {"msWidth", 1.4f}
        }},
        { "Dense Swarm", {
            {"chainSlot0", 2.0f}, {"chainSlot1", 0.0f}, {"chainSlot2", 0.0f}, {"chainSlot3", 0.0f}, {"destroyEnabled", 0.0f}, {"granularEnabled", 1.0f}, {"multiplyEnabled", 0.0f}, {"tapeEnabled", 0.0f}, {"destroyFader", 19000.0f}, {"destroyBits", 12.0f}, {"destroyIn", 0.0f}, {"destroyOut", 0.0f}, {"destroyMix", 1.0f}, {"filterCutoff", 1.0f}, {"filterLfo", 0.0f}, {"grainCloud", 1.0f}, {"grainDrift", 0.6f}, {"grainSpace", 0.3f}, {"grainMorph", 0.5f}, {"grainSizeSync", 0.0f}, {"grainReverse", 0.0f}, {"grainMix", 0.85f}, {"grainFreeze", 0.0f}, {"chorusSpeed", 1.0f}, {"chorusMix", 0.0f}, {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.8f}, {"tapeDrive", 0.02f}, {"tapeWear", 0.0f}, {"tapeGlue", 0.5f}, {"tapeNoise", 0.0f}, {"tapeNoiseSpeed", 1.0f}, {"tapeOutput", 0.0f}, {"tapeMix", 1.0f}, {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}, {"msWidth", 1.0f}, {"distortionEnabled", 0.0f}, {"distortionDrive", 0.5f}, {"distortionTone", 0.5f}, {"multiplyDepth", 0.13f}, {"multiplyTone", 0.7f}, {"distortionBias", 0.0f}, {"distortionMode", 0.0f}, {"grainShape", 0.0f}, {"distortionAsym", 0.5f}, {"multiplyFeedback", 0.0f}, {"multiplyLfoShape", 0.0f}, {"multiplyTempoSync", 0.0f}, {"multiplyShimmer", 0.2f}, {"grainPitchQuant", 0.0f}, {"grainHarm", 0.0f}
        }},
        { "Sparse Droplets", {
            {"chainSlot0", 2.0f}, {"chainSlot1", 0.0f}, {"chainSlot2", 0.0f}, {"chainSlot3", 0.0f}, {"destroyEnabled", 0.0f}, {"granularEnabled", 1.0f}, {"multiplyEnabled", 0.0f}, {"tapeEnabled", 0.0f}, {"destroyFader", 19000.0f}, {"destroyBits", 12.0f}, {"destroyIn", 0.0f}, {"destroyOut", 0.0f}, {"destroyMix", 1.0f}, {"filterCutoff", 1.0f}, {"filterLfo", 0.0f}, {"grainCloud", 0.05f}, {"grainDrift", 0.3f}, {"grainSpace", 0.6f}, {"grainMorph", 0.55f}, {"grainSizeSync", 0.0f}, {"grainReverse", 0.0f}, {"grainMix", 0.65f}, {"grainFreeze", 0.0f}, {"chorusSpeed", 1.0f}, {"chorusMix", 0.0f}, {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.8f}, {"tapeDrive", 0.02f}, {"tapeWear", 0.0f}, {"tapeGlue", 0.5f}, {"tapeNoise", 0.0f}, {"tapeNoiseSpeed", 1.0f}, {"tapeOutput", 0.0f}, {"tapeMix", 1.0f}, {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}, {"msWidth", 1.0f}, {"distortionEnabled", 0.0f}, {"distortionDrive", 0.5f}, {"distortionTone", 0.5f}, {"multiplyDepth", 0.13f}, {"multiplyTone", 0.7f}, {"distortionBias", 0.0f}, {"distortionMode", 0.0f}, {"grainShape", 0.0f}, {"distortionAsym", 0.5f}, {"multiplyFeedback", 0.0f}, {"multiplyLfoShape", 0.0f}, {"multiplyTempoSync", 0.0f}, {"multiplyShimmer", 0.2f}, {"grainPitchQuant", 0.0f}, {"grainHarm", 0.0f}
        }},
        { "Reverse Drift", {
            {"chainSlot0", 2.0f}, {"chainSlot1", 0.0f}, {"chainSlot2", 0.0f}, {"chainSlot3", 0.0f}, {"destroyEnabled", 0.0f}, {"granularEnabled", 1.0f}, {"multiplyEnabled", 0.0f}, {"tapeEnabled", 0.0f}, {"destroyFader", 19000.0f}, {"destroyBits", 12.0f}, {"destroyIn", 0.0f}, {"destroyOut", 0.0f}, {"destroyMix", 1.0f}, {"filterCutoff", 1.0f}, {"filterLfo", 0.0f}, {"grainCloud", 0.35f}, {"grainDrift", 0.9f}, {"grainSpace", 0.45f}, {"grainMorph", 0.5f}, {"grainSizeSync", 0.0f}, {"grainReverse", 0.0f}, {"grainMix", 0.7f}, {"grainFreeze", 0.0f}, {"chorusSpeed", 1.0f}, {"chorusMix", 0.0f}, {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.8f}, {"tapeDrive", 0.02f}, {"tapeWear", 0.0f}, {"tapeGlue", 0.5f}, {"tapeNoise", 0.0f}, {"tapeNoiseSpeed", 1.0f}, {"tapeOutput", 0.0f}, {"tapeMix", 1.0f}, {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}, {"msWidth", 1.0f}, {"distortionEnabled", 0.0f}, {"distortionDrive", 0.5f}, {"distortionTone", 0.5f}, {"multiplyDepth", 0.13f}, {"multiplyTone", 0.7f}, {"distortionBias", 0.0f}, {"distortionMode", 0.0f}, {"grainShape", 0.0f}, {"distortionAsym", 0.5f}, {"multiplyFeedback", 0.0f}, {"multiplyLfoShape", 0.0f}, {"multiplyTempoSync", 0.0f}, {"multiplyShimmer", 0.2f}, {"grainPitchQuant", 0.0f}, {"grainHarm", 0.0f}
        }},
        { "Octave Halo", {
            {"chainSlot0", 2.0f}, {"chainSlot1", 0.0f}, {"chainSlot2", 0.0f}, {"chainSlot3", 0.0f}, {"destroyEnabled", 0.0f}, {"granularEnabled", 1.0f}, {"multiplyEnabled", 0.0f}, {"tapeEnabled", 0.0f}, {"destroyFader", 19000.0f}, {"destroyBits", 12.0f}, {"destroyIn", 0.0f}, {"destroyOut", 0.0f}, {"destroyMix", 1.0f}, {"filterCutoff", 1.0f}, {"filterLfo", 0.0f}, {"grainCloud", 0.7f}, {"grainDrift", 0.1f}, {"grainSpace", 0.8f}, {"grainMorph", 1.0f}, {"grainSizeSync", 0.0f}, {"grainReverse", 0.0f}, {"grainMix", 0.8f}, {"grainFreeze", 0.0f}, {"chorusSpeed", 1.0f}, {"chorusMix", 0.0f}, {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.8f}, {"tapeDrive", 0.02f}, {"tapeWear", 0.0f}, {"tapeGlue", 0.5f}, {"tapeNoise", 0.0f}, {"tapeNoiseSpeed", 1.0f}, {"tapeOutput", 0.0f}, {"tapeMix", 1.0f}, {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}, {"msWidth", 1.6f}
        }},
        { "Grain Whisper", {
            {"chainSlot0", 2.0f}, {"chainSlot1", 0.0f}, {"chainSlot2", 0.0f}, {"chainSlot3", 0.0f}, {"destroyEnabled", 0.0f}, {"granularEnabled", 1.0f}, {"multiplyEnabled", 0.0f}, {"tapeEnabled", 0.0f}, {"destroyFader", 19000.0f}, {"destroyBits", 12.0f}, {"destroyIn", 0.0f}, {"destroyOut", 0.0f}, {"destroyMix", 1.0f}, {"filterCutoff", 1.0f}, {"filterLfo", 0.0f}, {"grainCloud", 0.15f}, {"grainDrift", 0.25f}, {"grainSpace", 0.35f}, {"grainMorph", 0.48f}, {"grainSizeSync", 0.0f}, {"grainReverse", 0.0f}, {"grainMix", 0.4f}, {"grainFreeze", 0.0f}, {"chorusSpeed", 1.0f}, {"chorusMix", 0.0f}, {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.8f}, {"tapeDrive", 0.02f}, {"tapeWear", 0.0f}, {"tapeGlue", 0.5f}, {"tapeNoise", 0.0f}, {"tapeNoiseSpeed", 1.0f}, {"tapeOutput", 0.0f}, {"tapeMix", 1.0f}, {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}, {"msWidth", 1.0f}, {"distortionEnabled", 0.0f}, {"distortionDrive", 0.5f}, {"distortionTone", 0.5f}, {"multiplyDepth", 0.13f}, {"multiplyTone", 0.7f}, {"distortionBias", 0.0f}, {"distortionMode", 0.0f}, {"grainShape", 0.0f}, {"distortionAsym", 0.5f}, {"multiplyFeedback", 0.0f}, {"multiplyLfoShape", 0.0f}, {"multiplyTempoSync", 0.0f}, {"multiplyShimmer", 0.2f}, {"grainPitchQuant", 0.0f}, {"grainHarm", 0.0f}
        }},
        { "Frozen Void", {
            {"chainSlot0", 2.0f}, {"chainSlot1", 0.0f}, {"chainSlot2", 0.0f}, {"chainSlot3", 0.0f}, {"destroyEnabled", 0.0f}, {"granularEnabled", 1.0f}, {"multiplyEnabled", 0.0f}, {"tapeEnabled", 0.0f}, {"destroyFader", 19000.0f}, {"destroyBits", 12.0f}, {"destroyIn", 0.0f}, {"destroyOut", 0.0f}, {"destroyMix", 1.0f}, {"filterCutoff", 1.0f}, {"filterLfo", 0.0f}, {"grainCloud", 0.2f}, {"grainDrift", 0.02f}, {"grainSpace", 1.0f}, {"grainMorph", 0.5f}, {"grainSizeSync", 0.0f}, {"grainReverse", 0.0f}, {"grainMix", 1.0f}, {"grainFreeze", 1.0f}, {"chorusSpeed", 1.0f}, {"chorusMix", 0.0f}, {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.8f}, {"tapeDrive", 0.02f}, {"tapeWear", 0.0f}, {"tapeGlue", 0.5f}, {"tapeNoise", 0.0f}, {"tapeNoiseSpeed", 1.0f}, {"tapeOutput", 0.0f}, {"tapeMix", 1.0f}, {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}, {"msWidth", 1.8f}
        }},
        { "Reel To Reel", {
            {"chainSlot0", 4.0f}, {"chainSlot1", 0.0f}, {"chainSlot2", 0.0f}, {"chainSlot3", 0.0f}, {"destroyEnabled", 0.0f}, {"granularEnabled", 0.0f}, {"multiplyEnabled", 0.0f}, {"tapeEnabled", 1.0f}, {"destroyFader", 19000.0f}, {"destroyBits", 12.0f}, {"destroyIn", 0.0f}, {"destroyOut", 0.0f}, {"destroyMix", 1.0f}, {"filterCutoff", 1.0f}, {"filterLfo", 0.0f}, {"grainCloud", 0.3f}, {"grainDrift", 0.2f}, {"grainSpace", 0.3f}, {"grainMorph", 0.5f}, {"grainSizeSync", 0.0f}, {"grainReverse", 0.0f}, {"grainMix", 0.0f}, {"grainFreeze", 0.0f}, {"chorusSpeed", 1.0f}, {"chorusMix", 0.0f}, {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.8f}, {"tapeDrive", 0.12f}, {"tapeWear", 0.15f}, {"tapeGlue", 0.5f}, {"tapeNoise", 0.05f}, {"tapeNoiseSpeed", 1.0f}, {"tapeOutput", 0.0f}, {"tapeMix", 1.0f}, {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}, {"msWidth", 1.0f}, {"distortionEnabled", 0.0f}, {"distortionDrive", 0.5f}, {"distortionTone", 0.5f}, {"multiplyDepth", 0.13f}, {"multiplyTone", 0.7f}, {"distortionBias", 0.0f}, {"distortionMode", 0.0f}, {"grainShape", 0.0f}, {"distortionAsym", 0.5f}, {"multiplyFeedback", 0.0f}, {"multiplyLfoShape", 0.0f}, {"multiplyTempoSync", 0.0f}, {"multiplyShimmer", 0.2f}, {"grainPitchQuant", 0.0f}, {"grainHarm", 0.0f}
        }},
        { "Tape Warmth", {
            {"chainSlot0", 4.0f}, {"chainSlot1", 0.0f}, {"chainSlot2", 0.0f}, {"chainSlot3", 0.0f}, {"destroyEnabled", 0.0f}, {"granularEnabled", 0.0f}, {"multiplyEnabled", 0.0f}, {"tapeEnabled", 1.0f}, {"destroyFader", 19000.0f}, {"destroyBits", 12.0f}, {"destroyIn", 0.0f}, {"destroyOut", 0.0f}, {"destroyMix", 1.0f}, {"filterCutoff", 1.0f}, {"filterLfo", 0.0f}, {"grainCloud", 0.3f}, {"grainDrift", 0.2f}, {"grainSpace", 0.3f}, {"grainMorph", 0.5f}, {"grainSizeSync", 0.0f}, {"grainReverse", 0.0f}, {"grainMix", 0.0f}, {"grainFreeze", 0.0f}, {"chorusSpeed", 1.0f}, {"chorusMix", 0.0f}, {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.8f}, {"tapeDrive", 0.11f}, {"tapeWear", 0.13f}, {"tapeGlue", 0.5f}, {"tapeNoise", 0.04f}, {"tapeNoiseSpeed", 1.0f}, {"tapeOutput", 0.0f}, {"tapeMix", 1.0f}, {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}, {"msWidth", 1.0f}, {"distortionEnabled", 0.0f}, {"distortionDrive", 0.5f}, {"distortionTone", 0.5f}, {"multiplyDepth", 0.13f}, {"multiplyTone", 0.7f}, {"distortionBias", 0.0f}, {"distortionMode", 0.0f}, {"grainShape", 0.0f}, {"distortionAsym", 0.5f}, {"multiplyFeedback", 0.0f}, {"multiplyLfoShape", 0.0f}, {"multiplyTempoSync", 0.0f}, {"multiplyShimmer", 0.2f}, {"grainPitchQuant", 0.0f}, {"grainHarm", 0.0f}
        }},
        { "Wobble Machine", {
            {"chainSlot0", 4.0f}, {"chainSlot1", 0.0f}, {"chainSlot2", 0.0f}, {"chainSlot3", 0.0f}, {"destroyEnabled", 0.0f}, {"granularEnabled", 0.0f}, {"multiplyEnabled", 0.0f}, {"tapeEnabled", 1.0f}, {"destroyFader", 19000.0f}, {"destroyBits", 12.0f}, {"destroyIn", 0.0f}, {"destroyOut", 0.0f}, {"destroyMix", 1.0f}, {"filterCutoff", 1.0f}, {"filterLfo", 0.0f}, {"grainCloud", 0.3f}, {"grainDrift", 0.2f}, {"grainSpace", 0.3f}, {"grainMorph", 0.5f}, {"grainSizeSync", 0.0f}, {"grainReverse", 0.0f}, {"grainMix", 0.0f}, {"grainFreeze", 0.0f}, {"chorusSpeed", 1.0f}, {"chorusMix", 0.0f}, {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.8f}, {"tapeDrive", 0.27f}, {"tapeWear", 0.36f}, {"tapeGlue", 0.5f}, {"tapeNoise", 0.18f}, {"tapeNoiseSpeed", 1.0f}, {"tapeOutput", 0.0f}, {"tapeMix", 1.0f}, {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}, {"msWidth", 1.0f}, {"distortionEnabled", 0.0f}, {"distortionDrive", 0.5f}, {"distortionTone", 0.5f}, {"multiplyDepth", 0.13f}, {"multiplyTone", 0.7f}, {"distortionBias", 0.0f}, {"distortionMode", 0.0f}, {"grainShape", 0.0f}, {"distortionAsym", 0.5f}, {"multiplyFeedback", 0.0f}, {"multiplyLfoShape", 0.0f}, {"multiplyTempoSync", 0.0f}, {"multiplyShimmer", 0.2f}, {"grainPitchQuant", 0.0f}, {"grainHarm", 0.0f}
        }},
        { "Old Cassette", {
            {"chainSlot0", 4.0f}, {"chainSlot1", 0.0f}, {"chainSlot2", 0.0f}, {"chainSlot3", 0.0f}, {"destroyEnabled", 0.0f}, {"granularEnabled", 0.0f}, {"multiplyEnabled", 0.0f}, {"tapeEnabled", 1.0f}, {"destroyFader", 19000.0f}, {"destroyBits", 12.0f}, {"destroyIn", 0.0f}, {"destroyOut", 0.0f}, {"destroyMix", 1.0f}, {"filterCutoff", 1.0f}, {"filterLfo", 0.0f}, {"grainCloud", 0.3f}, {"grainDrift", 0.2f}, {"grainSpace", 0.3f}, {"grainMorph", 0.5f}, {"grainSizeSync", 0.0f}, {"grainReverse", 0.0f}, {"grainMix", 0.0f}, {"grainFreeze", 0.0f}, {"chorusSpeed", 1.0f}, {"chorusMix", 0.0f}, {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.8f}, {"tapeDrive", 0.32f}, {"tapeWear", 0.43f}, {"tapeGlue", 0.4f}, {"tapeNoise", 0.25f}, {"tapeNoiseSpeed", 1.0f}, {"tapeOutput", 0.0f}, {"tapeMix", 1.0f}, {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}, {"msWidth", 1.0f}, {"distortionEnabled", 0.0f}, {"distortionDrive", 0.5f}, {"distortionTone", 0.5f}, {"multiplyDepth", 0.13f}, {"multiplyTone", 0.7f}, {"distortionBias", 0.0f}, {"distortionMode", 0.0f}, {"grainShape", 0.0f}, {"distortionAsym", 0.5f}, {"multiplyFeedback", 0.0f}, {"multiplyLfoShape", 0.0f}, {"multiplyTempoSync", 0.0f}, {"multiplyShimmer", 0.2f}, {"grainPitchQuant", 0.0f}, {"grainHarm", 0.0f}
        }},
        { "Tape Saturation", {
            {"chainSlot0", 4.0f}, {"chainSlot1", 0.0f}, {"chainSlot2", 0.0f}, {"chainSlot3", 0.0f}, {"destroyEnabled", 0.0f}, {"granularEnabled", 0.0f}, {"multiplyEnabled", 0.0f}, {"tapeEnabled", 1.0f}, {"destroyFader", 19000.0f}, {"destroyBits", 12.0f}, {"destroyIn", 0.0f}, {"destroyOut", 0.0f}, {"destroyMix", 1.0f}, {"filterCutoff", 1.0f}, {"filterLfo", 0.0f}, {"grainCloud", 0.3f}, {"grainDrift", 0.2f}, {"grainSpace", 0.3f}, {"grainMorph", 0.5f}, {"grainSizeSync", 0.0f}, {"grainReverse", 0.0f}, {"grainMix", 0.0f}, {"grainFreeze", 0.0f}, {"chorusSpeed", 1.0f}, {"chorusMix", 0.0f}, {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.8f}, {"tapeDrive", 0.23f}, {"tapeWear", 0.32f}, {"tapeGlue", 0.7f}, {"tapeNoise", 0.14f}, {"tapeNoiseSpeed", 1.0f}, {"tapeOutput", 0.0f}, {"tapeMix", 1.0f}, {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}, {"msWidth", 1.0f}, {"distortionEnabled", 0.0f}, {"distortionDrive", 0.5f}, {"distortionTone", 0.5f}, {"multiplyDepth", 0.13f}, {"multiplyTone", 0.7f}, {"distortionBias", 0.0f}, {"distortionMode", 0.0f}, {"grainShape", 0.0f}, {"distortionAsym", 0.5f}, {"multiplyFeedback", 0.0f}, {"multiplyLfoShape", 0.0f}, {"multiplyTempoSync", 0.0f}, {"multiplyShimmer", 0.2f}, {"grainPitchQuant", 0.0f}, {"grainHarm", 0.0f}
        }},
        { "Mastering Glue", {
            {"chainSlot0", 4.0f}, {"chainSlot1", 0.0f}, {"chainSlot2", 0.0f}, {"chainSlot3", 0.0f}, {"destroyEnabled", 0.0f}, {"granularEnabled", 0.0f}, {"multiplyEnabled", 0.0f}, {"tapeEnabled", 1.0f}, {"destroyFader", 19000.0f}, {"destroyBits", 12.0f}, {"destroyIn", 0.0f}, {"destroyOut", 0.0f}, {"destroyMix", 1.0f}, {"filterCutoff", 1.0f}, {"filterLfo", 0.0f}, {"grainCloud", 0.3f}, {"grainDrift", 0.2f}, {"grainSpace", 0.3f}, {"grainMorph", 0.5f}, {"grainSizeSync", 0.0f}, {"grainReverse", 0.0f}, {"grainMix", 0.0f}, {"grainFreeze", 0.0f}, {"chorusSpeed", 1.0f}, {"chorusMix", 0.0f}, {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.8f}, {"tapeDrive", 0.1f}, {"tapeWear", 0.16f}, {"tapeGlue", 0.5f}, {"tapeNoise", 0.03f}, {"tapeNoiseSpeed", 1.0f}, {"tapeOutput", 0.0f}, {"tapeMix", 0.5f}, {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}, {"msWidth", 1.0f}, {"distortionEnabled", 0.0f}, {"distortionDrive", 0.5f}, {"distortionTone", 0.5f}, {"multiplyDepth", 0.13f}, {"multiplyTone", 0.7f}, {"distortionBias", 0.0f}, {"distortionMode", 0.0f}, {"grainShape", 0.0f}, {"distortionAsym", 0.5f}, {"multiplyFeedback", 0.0f}, {"multiplyLfoShape", 0.0f}, {"multiplyTempoSync", 0.0f}, {"multiplyShimmer", 0.2f}, {"grainPitchQuant", 0.0f}, {"grainHarm", 0.0f}
        }},
        { "30 IPS Punch", {
            {"chainSlot0", 4.0f}, {"chainSlot1", 0.0f}, {"chainSlot2", 0.0f}, {"chainSlot3", 0.0f}, {"destroyEnabled", 0.0f}, {"granularEnabled", 0.0f}, {"multiplyEnabled", 0.0f}, {"tapeEnabled", 1.0f}, {"destroyFader", 19000.0f}, {"destroyBits", 12.0f}, {"destroyIn", 0.0f}, {"destroyOut", 0.0f}, {"destroyMix", 1.0f}, {"filterCutoff", 1.0f}, {"filterLfo", 0.0f}, {"grainCloud", 0.3f}, {"grainDrift", 0.2f}, {"grainSpace", 0.3f}, {"grainMorph", 0.5f}, {"grainSizeSync", 0.0f}, {"grainReverse", 0.0f}, {"grainMix", 0.0f}, {"grainFreeze", 0.0f}, {"chorusSpeed", 1.0f}, {"chorusMix", 0.0f}, {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.8f}, {"tapeDrive", 0.13f}, {"tapeWear", 0.18f}, {"tapeGlue", 0.5f}, {"tapeNoise", 0.05f}, {"tapeNoiseSpeed", 2.0f}, {"tapeOutput", 0.0f}, {"tapeMix", 1.0f}, {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}, {"msWidth", 1.0f}, {"distortionEnabled", 0.0f}, {"distortionDrive", 0.5f}, {"distortionTone", 0.5f}, {"multiplyDepth", 0.13f}, {"multiplyTone", 0.7f}, {"distortionBias", 0.0f}, {"distortionMode", 0.0f}, {"grainShape", 0.0f}, {"distortionAsym", 0.5f}, {"multiplyFeedback", 0.0f}, {"multiplyLfoShape", 0.0f}, {"multiplyTempoSync", 0.0f}, {"multiplyShimmer", 0.2f}, {"grainPitchQuant", 0.0f}, {"grainHarm", 0.0f}
        }},
        { "Ampex Vintage", {
            {"chainSlot0", 4.0f}, {"chainSlot1", 0.0f}, {"chainSlot2", 0.0f}, {"chainSlot3", 0.0f}, {"destroyEnabled", 0.0f}, {"granularEnabled", 0.0f}, {"multiplyEnabled", 0.0f}, {"tapeEnabled", 1.0f}, {"destroyFader", 19000.0f}, {"destroyBits", 12.0f}, {"destroyIn", 0.0f}, {"destroyOut", 0.0f}, {"destroyMix", 1.0f}, {"filterCutoff", 1.0f}, {"filterLfo", 0.0f}, {"grainCloud", 0.3f}, {"grainDrift", 0.2f}, {"grainSpace", 0.3f}, {"grainMorph", 0.5f}, {"grainSizeSync", 0.0f}, {"grainReverse", 0.0f}, {"grainMix", 0.0f}, {"grainFreeze", 0.0f}, {"chorusSpeed", 1.0f}, {"chorusMix", 0.0f}, {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.8f}, {"tapeDrive", 0.12f}, {"tapeWear", 0.14f}, {"tapeGlue", 0.45f}, {"tapeNoise", 0.04f}, {"tapeNoiseSpeed", 1.0f}, {"tapeOutput", 0.0f}, {"tapeMix", 1.0f}, {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}, {"msWidth", 1.0f}, {"distortionEnabled", 0.0f}, {"distortionDrive", 0.5f}, {"distortionTone", 0.5f}, {"multiplyDepth", 0.13f}, {"multiplyTone", 0.7f}, {"distortionBias", 0.0f}, {"distortionMode", 0.0f}, {"grainShape", 0.0f}, {"distortionAsym", 0.5f}, {"multiplyFeedback", 0.0f}, {"multiplyLfoShape", 0.0f}, {"multiplyTempoSync", 0.0f}, {"multiplyShimmer", 0.2f}, {"grainPitchQuant", 0.0f}, {"grainHarm", 0.0f}
        }},
        { "SM900 Clarity", {
            {"chainSlot0", 4.0f}, {"chainSlot1", 0.0f}, {"chainSlot2", 0.0f}, {"chainSlot3", 0.0f}, {"destroyEnabled", 0.0f}, {"granularEnabled", 0.0f}, {"multiplyEnabled", 0.0f}, {"tapeEnabled", 1.0f}, {"destroyFader", 19000.0f}, {"destroyBits", 12.0f}, {"destroyIn", 0.0f}, {"destroyOut", 0.0f}, {"destroyMix", 1.0f}, {"filterCutoff", 1.0f}, {"filterLfo", 0.0f}, {"grainCloud", 0.3f}, {"grainDrift", 0.2f}, {"grainSpace", 0.3f}, {"grainMorph", 0.5f}, {"grainSizeSync", 0.0f}, {"grainReverse", 0.0f}, {"grainMix", 0.0f}, {"grainFreeze", 0.0f}, {"chorusSpeed", 1.0f}, {"chorusMix", 0.0f}, {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.8f}, {"tapeDrive", 0.03f}, {"tapeWear", 0.06f}, {"tapeGlue", 0.5f}, {"tapeNoise", 0.0f}, {"tapeNoiseSpeed", 1.0f}, {"tapeOutput", 0.0f}, {"tapeMix", 1.0f}, {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}, {"msWidth", 1.0f}, {"distortionEnabled", 0.0f}, {"distortionDrive", 0.5f}, {"distortionTone", 0.5f}, {"multiplyDepth", 0.13f}, {"multiplyTone", 0.7f}, {"distortionBias", 0.0f}, {"distortionMode", 0.0f}, {"grainShape", 0.0f}, {"distortionAsym", 0.5f}, {"multiplyFeedback", 0.0f}, {"multiplyLfoShape", 0.0f}, {"multiplyTempoSync", 0.0f}, {"multiplyShimmer", 0.2f}, {"grainPitchQuant", 0.0f}, {"grainHarm", 0.0f}
        }},
        { "Hiss Machine", {
            {"chainSlot0", 4.0f}, {"chainSlot1", 0.0f}, {"chainSlot2", 0.0f}, {"chainSlot3", 0.0f}, {"destroyEnabled", 0.0f}, {"granularEnabled", 0.0f}, {"multiplyEnabled", 0.0f}, {"tapeEnabled", 1.0f}, {"destroyFader", 19000.0f}, {"destroyBits", 12.0f}, {"destroyIn", 0.0f}, {"destroyOut", 0.0f}, {"destroyMix", 1.0f}, {"filterCutoff", 1.0f}, {"filterLfo", 0.0f}, {"grainCloud", 0.3f}, {"grainDrift", 0.2f}, {"grainSpace", 0.3f}, {"grainMorph", 0.5f}, {"grainSizeSync", 0.0f}, {"grainReverse", 0.0f}, {"grainMix", 0.0f}, {"grainFreeze", 0.0f}, {"chorusSpeed", 1.0f}, {"chorusMix", 0.0f}, {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.8f}, {"tapeDrive", 0.2f}, {"tapeWear", 0.26f}, {"tapeGlue", 0.35f}, {"tapeNoise", 0.11f}, {"tapeNoiseSpeed", 1.0f}, {"tapeOutput", 0.0f}, {"tapeMix", 1.0f}, {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}, {"msWidth", 1.0f}, {"distortionEnabled", 0.0f}, {"distortionDrive", 0.5f}, {"distortionTone", 0.5f}, {"multiplyDepth", 0.13f}, {"multiplyTone", 0.7f}, {"distortionBias", 0.0f}, {"distortionMode", 0.0f}, {"grainShape", 0.0f}, {"distortionAsym", 0.5f}, {"multiplyFeedback", 0.0f}, {"multiplyLfoShape", 0.0f}, {"multiplyTempoSync", 0.0f}, {"multiplyShimmer", 0.2f}, {"grainPitchQuant", 0.0f}, {"grainHarm", 0.0f}
        }},
        { "Tape Overdrive", {
            {"chainSlot0", 4.0f}, {"chainSlot1", 0.0f}, {"chainSlot2", 0.0f}, {"chainSlot3", 0.0f}, {"destroyEnabled", 0.0f}, {"granularEnabled", 0.0f}, {"multiplyEnabled", 0.0f}, {"tapeEnabled", 1.0f}, {"destroyFader", 19000.0f}, {"destroyBits", 12.0f}, {"destroyIn", 0.0f}, {"destroyOut", 0.0f}, {"destroyMix", 1.0f}, {"filterCutoff", 1.0f}, {"filterLfo", 0.0f}, {"grainCloud", 0.3f}, {"grainDrift", 0.2f}, {"grainSpace", 0.3f}, {"grainMorph", 0.5f}, {"grainSizeSync", 0.0f}, {"grainReverse", 0.0f}, {"grainMix", 0.0f}, {"grainFreeze", 0.0f}, {"chorusSpeed", 1.0f}, {"chorusMix", 0.0f}, {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.8f}, {"tapeDrive", 0.33f}, {"tapeWear", 0.44f}, {"tapeGlue", 0.8f}, {"tapeNoise", 0.26f}, {"tapeNoiseSpeed", 1.0f}, {"tapeOutput", 0.0f}, {"tapeMix", 1.0f}, {"inputGain", 3.0f}, {"outputGain", -3.0f}, {"masterMix", 1.0f}, {"msWidth", 1.0f}, {"distortionEnabled", 0.0f}, {"distortionDrive", 0.5f}, {"distortionTone", 0.5f}, {"multiplyDepth", 0.13f}, {"multiplyTone", 0.7f}, {"distortionBias", 0.0f}, {"distortionMode", 0.0f}, {"grainShape", 0.0f}, {"distortionAsym", 0.5f}, {"multiplyFeedback", 0.0f}, {"multiplyLfoShape", 0.0f}, {"multiplyTempoSync", 0.0f}, {"multiplyShimmer", 0.2f}, {"grainPitchQuant", 0.0f}, {"grainHarm", 0.0f}
        }},
        { "IEC Broadcast", {
            {"chainSlot0", 4.0f}, {"chainSlot1", 0.0f}, {"chainSlot2", 0.0f}, {"chainSlot3", 0.0f}, {"destroyEnabled", 0.0f}, {"granularEnabled", 0.0f}, {"multiplyEnabled", 0.0f}, {"tapeEnabled", 1.0f}, {"destroyFader", 19000.0f}, {"destroyBits", 12.0f}, {"destroyIn", 0.0f}, {"destroyOut", 0.0f}, {"destroyMix", 1.0f}, {"filterCutoff", 1.0f}, {"filterLfo", 0.0f}, {"grainCloud", 0.3f}, {"grainDrift", 0.2f}, {"grainSpace", 0.3f}, {"grainMorph", 0.5f}, {"grainSizeSync", 0.0f}, {"grainReverse", 0.0f}, {"grainMix", 0.0f}, {"grainFreeze", 0.0f}, {"chorusSpeed", 1.0f}, {"chorusMix", 0.0f}, {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.8f}, {"tapeDrive", 0.07f}, {"tapeWear", 0.1f}, {"tapeGlue", 0.5f}, {"tapeNoise", 0.02f}, {"tapeNoiseSpeed", 1.0f}, {"tapeOutput", 0.0f}, {"tapeMix", 1.0f}, {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}, {"msWidth", 1.0f}, {"distortionEnabled", 0.0f}, {"distortionDrive", 0.5f}, {"distortionTone", 0.5f}, {"multiplyDepth", 0.13f}, {"multiplyTone", 0.7f}, {"distortionBias", 0.0f}, {"distortionMode", 0.0f}, {"grainShape", 0.0f}, {"distortionAsym", 0.5f}, {"multiplyFeedback", 0.0f}, {"multiplyLfoShape", 0.0f}, {"multiplyTempoSync", 0.0f}, {"multiplyShimmer", 0.2f}, {"grainPitchQuant", 0.0f}, {"grainHarm", 0.0f}
        }},
        { "Classic Chorus", {
            {"chainSlot0", 3.0f}, {"chainSlot1", 0.0f}, {"chainSlot2", 0.0f}, {"chainSlot3", 0.0f}, {"destroyEnabled", 0.0f}, {"granularEnabled", 0.0f}, {"multiplyEnabled", 1.0f}, {"tapeEnabled", 0.0f}, {"destroyFader", 19000.0f}, {"destroyBits", 12.0f}, {"destroyIn", 0.0f}, {"destroyOut", 0.0f}, {"destroyMix", 1.0f}, {"filterCutoff", 1.0f}, {"filterLfo", 0.0f}, {"grainCloud", 0.3f}, {"grainDrift", 0.2f}, {"grainSpace", 0.3f}, {"grainMorph", 0.5f}, {"grainSizeSync", 0.0f}, {"grainReverse", 0.0f}, {"grainMix", 0.0f}, {"grainFreeze", 0.0f}, {"chorusSpeed", 1.0f}, {"chorusMix", 0.5f}, {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.8f}, {"tapeDrive", 0.02f}, {"tapeWear", 0.0f}, {"tapeGlue", 0.5f}, {"tapeNoise", 0.0f}, {"tapeNoiseSpeed", 1.0f}, {"tapeOutput", 0.0f}, {"tapeMix", 1.0f}, {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}, {"msWidth", 1.0f}, {"distortionEnabled", 0.0f}, {"distortionDrive", 0.5f}, {"distortionTone", 0.5f}, {"multiplyDepth", 0.13f}, {"multiplyTone", 0.7f}, {"distortionBias", 0.0f}, {"distortionMode", 0.0f}, {"grainShape", 0.0f}, {"distortionAsym", 0.5f}, {"multiplyFeedback", 0.0f}, {"multiplyLfoShape", 0.0f}, {"multiplyTempoSync", 0.0f}, {"multiplyShimmer", 0.2f}, {"grainPitchQuant", 0.0f}, {"grainHarm", 0.0f}
        }},
        { "Slow Ensemble", {
            {"chainSlot0", 3.0f}, {"chainSlot1", 0.0f}, {"chainSlot2", 0.0f}, {"chainSlot3", 0.0f}, {"destroyEnabled", 0.0f}, {"granularEnabled", 0.0f}, {"multiplyEnabled", 1.0f}, {"tapeEnabled", 0.0f}, {"destroyFader", 19000.0f}, {"destroyBits", 12.0f}, {"destroyIn", 0.0f}, {"destroyOut", 0.0f}, {"destroyMix", 1.0f}, {"filterCutoff", 1.0f}, {"filterLfo", 0.0f}, {"grainCloud", 0.3f}, {"grainDrift", 0.2f}, {"grainSpace", 0.3f}, {"grainMorph", 0.5f}, {"grainSizeSync", 0.0f}, {"grainReverse", 0.0f}, {"grainMix", 0.0f}, {"grainFreeze", 0.0f}, {"chorusSpeed", 0.3f}, {"chorusMix", 0.6f}, {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.6f}, {"tapeDrive", 0.02f}, {"tapeWear", 0.0f}, {"tapeGlue", 0.5f}, {"tapeNoise", 0.0f}, {"tapeNoiseSpeed", 1.0f}, {"tapeOutput", 0.0f}, {"tapeMix", 1.0f}, {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}, {"msWidth", 1.2f}
        }},
        { "Fast Shimmer", {
            {"chainSlot0", 3.0f}, {"chainSlot1", 0.0f}, {"chainSlot2", 0.0f}, {"chainSlot3", 0.0f}, {"destroyEnabled", 0.0f}, {"granularEnabled", 0.0f}, {"multiplyEnabled", 1.0f}, {"tapeEnabled", 0.0f}, {"destroyFader", 19000.0f}, {"destroyBits", 12.0f}, {"destroyIn", 0.0f}, {"destroyOut", 0.0f}, {"destroyMix", 1.0f}, {"filterCutoff", 1.0f}, {"filterLfo", 0.0f}, {"grainCloud", 0.3f}, {"grainDrift", 0.2f}, {"grainSpace", 0.3f}, {"grainMorph", 0.5f}, {"grainSizeSync", 0.0f}, {"grainReverse", 0.0f}, {"grainMix", 0.0f}, {"grainFreeze", 0.0f}, {"chorusSpeed", 4.0f}, {"chorusMix", 0.4f}, {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.8f}, {"tapeDrive", 0.02f}, {"tapeWear", 0.0f}, {"tapeGlue", 0.5f}, {"tapeNoise", 0.0f}, {"tapeNoiseSpeed", 1.0f}, {"tapeOutput", 0.0f}, {"tapeMix", 1.0f}, {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}, {"msWidth", 1.0f}, {"distortionEnabled", 0.0f}, {"distortionDrive", 0.5f}, {"distortionTone", 0.5f}, {"multiplyDepth", 0.13f}, {"multiplyTone", 0.7f}, {"distortionBias", 0.0f}, {"distortionMode", 0.0f}, {"grainShape", 0.0f}, {"distortionAsym", 0.5f}, {"multiplyFeedback", 0.0f}, {"multiplyLfoShape", 0.0f}, {"multiplyTempoSync", 0.0f}, {"multiplyShimmer", 0.2f}, {"grainPitchQuant", 0.0f}, {"grainHarm", 0.0f}
        }},
        { "Wide Stereo Wash", {
            {"chainSlot0", 3.0f}, {"chainSlot1", 0.0f}, {"chainSlot2", 0.0f}, {"chainSlot3", 0.0f}, {"destroyEnabled", 0.0f}, {"granularEnabled", 0.0f}, {"multiplyEnabled", 1.0f}, {"tapeEnabled", 0.0f}, {"destroyFader", 19000.0f}, {"destroyBits", 12.0f}, {"destroyIn", 0.0f}, {"destroyOut", 0.0f}, {"destroyMix", 1.0f}, {"filterCutoff", 1.0f}, {"filterLfo", 0.0f}, {"grainCloud", 0.3f}, {"grainDrift", 0.2f}, {"grainSpace", 0.3f}, {"grainMorph", 0.5f}, {"grainSizeSync", 0.0f}, {"grainReverse", 0.0f}, {"grainMix", 0.0f}, {"grainFreeze", 0.0f}, {"chorusSpeed", 0.8f}, {"chorusMix", 0.7f}, {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.3f}, {"tapeDrive", 0.02f}, {"tapeWear", 0.0f}, {"tapeGlue", 0.5f}, {"tapeNoise", 0.0f}, {"tapeNoiseSpeed", 1.0f}, {"tapeOutput", 0.0f}, {"tapeMix", 1.0f}, {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}, {"msWidth", 1.5f}, {"distortionEnabled", 0.0f}, {"distortionDrive", 0.5f}, {"distortionTone", 0.5f}, {"multiplyDepth", 0.13f}, {"multiplyTone", 0.7f}, {"distortionBias", 0.0f}, {"distortionMode", 0.0f}, {"grainShape", 0.0f}, {"distortionAsym", 0.5f}, {"multiplyFeedback", 0.0f}, {"multiplyLfoShape", 0.0f}, {"multiplyTempoSync", 0.0f}, {"multiplyShimmer", 0.2f}, {"grainPitchQuant", 0.0f}, {"grainHarm", 0.0f}
        }},
        { "Tight Doubles", {
            {"chainSlot0", 3.0f}, {"chainSlot1", 0.0f}, {"chainSlot2", 0.0f}, {"chainSlot3", 0.0f}, {"destroyEnabled", 0.0f}, {"granularEnabled", 0.0f}, {"multiplyEnabled", 1.0f}, {"tapeEnabled", 0.0f}, {"destroyFader", 19000.0f}, {"destroyBits", 12.0f}, {"destroyIn", 0.0f}, {"destroyOut", 0.0f}, {"destroyMix", 1.0f}, {"filterCutoff", 1.0f}, {"filterLfo", 0.0f}, {"grainCloud", 0.3f}, {"grainDrift", 0.2f}, {"grainSpace", 0.3f}, {"grainMorph", 0.5f}, {"grainSizeSync", 0.0f}, {"grainReverse", 0.0f}, {"grainMix", 0.0f}, {"grainFreeze", 0.0f}, {"chorusSpeed", 1.5f}, {"chorusMix", 0.35f}, {"multiplyPanOuter", 0.5f}, {"multiplyPanInner", 0.4f}, {"tapeDrive", 0.02f}, {"tapeWear", 0.0f}, {"tapeGlue", 0.5f}, {"tapeNoise", 0.0f}, {"tapeNoiseSpeed", 1.0f}, {"tapeOutput", 0.0f}, {"tapeMix", 1.0f}, {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}, {"msWidth", 1.0f}, {"distortionEnabled", 0.0f}, {"distortionDrive", 0.5f}, {"distortionTone", 0.5f}, {"multiplyDepth", 0.13f}, {"multiplyTone", 0.7f}, {"distortionBias", 0.0f}, {"distortionMode", 0.0f}, {"grainShape", 0.0f}, {"distortionAsym", 0.5f}, {"multiplyFeedback", 0.0f}, {"multiplyLfoShape", 0.0f}, {"multiplyTempoSync", 0.0f}, {"multiplyShimmer", 0.2f}, {"grainPitchQuant", 0.0f}, {"grainHarm", 0.0f}
        }},
        { "Vibrato Voice", {
            {"chainSlot0", 3.0f}, {"chainSlot1", 0.0f}, {"chainSlot2", 0.0f}, {"chainSlot3", 0.0f}, {"destroyEnabled", 0.0f}, {"granularEnabled", 0.0f}, {"multiplyEnabled", 1.0f}, {"tapeEnabled", 0.0f}, {"destroyFader", 19000.0f}, {"destroyBits", 12.0f}, {"destroyIn", 0.0f}, {"destroyOut", 0.0f}, {"destroyMix", 1.0f}, {"filterCutoff", 1.0f}, {"filterLfo", 0.0f}, {"grainCloud", 0.3f}, {"grainDrift", 0.2f}, {"grainSpace", 0.3f}, {"grainMorph", 0.5f}, {"grainSizeSync", 0.0f}, {"grainReverse", 0.0f}, {"grainMix", 0.0f}, {"grainFreeze", 0.0f}, {"chorusSpeed", 5.0f}, {"chorusMix", 0.3f}, {"multiplyPanOuter", 0.7f}, {"multiplyPanInner", 0.7f}, {"tapeDrive", 0.02f}, {"tapeWear", 0.0f}, {"tapeGlue", 0.5f}, {"tapeNoise", 0.0f}, {"tapeNoiseSpeed", 1.0f}, {"tapeOutput", 0.0f}, {"tapeMix", 1.0f}, {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}, {"msWidth", 1.0f}, {"distortionEnabled", 0.0f}, {"distortionDrive", 0.5f}, {"distortionTone", 0.5f}, {"multiplyDepth", 0.13f}, {"multiplyTone", 0.7f}, {"distortionBias", 0.0f}, {"distortionMode", 0.0f}, {"grainShape", 0.0f}, {"distortionAsym", 0.5f}, {"multiplyFeedback", 0.0f}, {"multiplyLfoShape", 0.0f}, {"multiplyTempoSync", 0.0f}, {"multiplyShimmer", 0.2f}, {"grainPitchQuant", 0.0f}, {"grainHarm", 0.0f}
        }},
        { "Mono Thicken", {
            {"chainSlot0", 3.0f}, {"chainSlot1", 0.0f}, {"chainSlot2", 0.0f}, {"chainSlot3", 0.0f}, {"destroyEnabled", 0.0f}, {"granularEnabled", 0.0f}, {"multiplyEnabled", 1.0f}, {"tapeEnabled", 0.0f}, {"destroyFader", 19000.0f}, {"destroyBits", 12.0f}, {"destroyIn", 0.0f}, {"destroyOut", 0.0f}, {"destroyMix", 1.0f}, {"filterCutoff", 1.0f}, {"filterLfo", 0.0f}, {"grainCloud", 0.3f}, {"grainDrift", 0.2f}, {"grainSpace", 0.3f}, {"grainMorph", 0.5f}, {"grainSizeSync", 0.0f}, {"grainReverse", 0.0f}, {"grainMix", 0.0f}, {"grainFreeze", 0.0f}, {"chorusSpeed", 0.6f}, {"chorusMix", 0.45f}, {"multiplyPanOuter", 0.2f}, {"multiplyPanInner", 0.1f}, {"tapeDrive", 0.02f}, {"tapeWear", 0.0f}, {"tapeGlue", 0.5f}, {"tapeNoise", 0.0f}, {"tapeNoiseSpeed", 1.0f}, {"tapeOutput", 0.0f}, {"tapeMix", 1.0f}, {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}, {"msWidth", 0.8f}
        }},
        { "Detuned Spread", {
            {"chainSlot0", 3.0f}, {"chainSlot1", 0.0f}, {"chainSlot2", 0.0f}, {"chainSlot3", 0.0f}, {"destroyEnabled", 0.0f}, {"granularEnabled", 0.0f}, {"multiplyEnabled", 1.0f}, {"tapeEnabled", 0.0f}, {"destroyFader", 19000.0f}, {"destroyBits", 12.0f}, {"destroyIn", 0.0f}, {"destroyOut", 0.0f}, {"destroyMix", 1.0f}, {"filterCutoff", 1.0f}, {"filterLfo", 0.0f}, {"grainCloud", 0.3f}, {"grainDrift", 0.2f}, {"grainSpace", 0.3f}, {"grainMorph", 0.5f}, {"grainSizeSync", 0.0f}, {"grainReverse", 0.0f}, {"grainMix", 0.0f}, {"grainFreeze", 0.0f}, {"chorusSpeed", 0.15f}, {"chorusMix", 0.8f}, {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 1.0f}, {"tapeDrive", 0.02f}, {"tapeWear", 0.0f}, {"tapeGlue", 0.5f}, {"tapeNoise", 0.0f}, {"tapeNoiseSpeed", 1.0f}, {"tapeOutput", 0.0f}, {"tapeMix", 1.0f}, {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}, {"msWidth", 1.8f}
        }},
        { "Dusty Cassette", {
            {"chainSlot0", 1.0f}, {"chainSlot1", 4.0f}, {"chainSlot2", 0.0f}, {"chainSlot3", 0.0f}, {"destroyEnabled", 1.0f}, {"granularEnabled", 0.0f}, {"multiplyEnabled", 0.0f}, {"tapeEnabled", 1.0f}, {"destroyFader", 19000.0f}, {"destroyBits", 12.0f}, {"destroyIn", 2.0f}, {"destroyOut", 0.0f}, {"destroyMix", 0.8f}, {"filterCutoff", 0.65f}, {"filterLfo", 0.0f}, {"grainCloud", 0.3f}, {"grainDrift", 0.2f}, {"grainSpace", 0.3f}, {"grainMorph", 0.5f}, {"grainSizeSync", 0.0f}, {"grainReverse", 0.0f}, {"grainMix", 0.0f}, {"grainFreeze", 0.0f}, {"chorusSpeed", 1.0f}, {"chorusMix", 0.0f}, {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.8f}, {"tapeDrive", 0.23f}, {"tapeWear", 0.31f}, {"tapeGlue", 0.45f}, {"tapeNoise", 0.14f}, {"tapeNoiseSpeed", 1.0f}, {"tapeOutput", 0.0f}, {"tapeMix", 1.0f}, {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}, {"msWidth", 1.0f}, {"distortionEnabled", 0.0f}, {"distortionDrive", 0.5f}, {"distortionTone", 0.5f}, {"multiplyDepth", 0.13f}, {"multiplyTone", 0.7f}, {"distortionBias", 0.0f}, {"distortionMode", 0.0f}, {"grainShape", 0.0f}, {"distortionAsym", 0.5f}, {"multiplyFeedback", 0.0f}, {"multiplyLfoShape", 0.0f}, {"multiplyTempoSync", 0.0f}, {"multiplyShimmer", 0.2f}, {"grainPitchQuant", 0.0f}, {"grainHarm", 0.0f}
        }},
        { "Sampler Through Tape", {
            {"chainSlot0", 1.0f}, {"chainSlot1", 4.0f}, {"chainSlot2", 0.0f}, {"chainSlot3", 0.0f}, {"destroyEnabled", 1.0f}, {"granularEnabled", 0.0f}, {"multiplyEnabled", 0.0f}, {"tapeEnabled", 1.0f}, {"destroyFader", 19000.0f}, {"destroyBits", 12.0f}, {"destroyIn", 0.0f}, {"destroyOut", 0.0f}, {"destroyMix", 1.0f}, {"filterCutoff", 0.8f}, {"filterLfo", 0.0f}, {"grainCloud", 0.3f}, {"grainDrift", 0.2f}, {"grainSpace", 0.3f}, {"grainMorph", 0.5f}, {"grainSizeSync", 0.0f}, {"grainReverse", 0.0f}, {"grainMix", 0.0f}, {"grainFreeze", 0.0f}, {"chorusSpeed", 1.0f}, {"chorusMix", 0.0f}, {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.8f}, {"tapeDrive", 0.11f}, {"tapeWear", 0.13f}, {"tapeGlue", 0.5f}, {"tapeNoise", 0.04f}, {"tapeNoiseSpeed", 1.0f}, {"tapeOutput", 0.0f}, {"tapeMix", 1.0f}, {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}, {"msWidth", 1.0f}, {"distortionEnabled", 0.0f}, {"distortionDrive", 0.5f}, {"distortionTone", 0.5f}, {"multiplyDepth", 0.13f}, {"multiplyTone", 0.7f}, {"distortionBias", 0.0f}, {"distortionMode", 0.0f}, {"grainShape", 0.0f}, {"distortionAsym", 0.5f}, {"multiplyFeedback", 0.0f}, {"multiplyLfoShape", 0.0f}, {"multiplyTempoSync", 0.0f}, {"multiplyShimmer", 0.2f}, {"grainPitchQuant", 0.0f}, {"grainHarm", 0.0f}
        }},
        { "VHS Memories", {
            {"chainSlot0", 1.0f}, {"chainSlot1", 4.0f}, {"chainSlot2", 0.0f}, {"chainSlot3", 0.0f}, {"destroyEnabled", 1.0f}, {"granularEnabled", 0.0f}, {"multiplyEnabled", 0.0f}, {"tapeEnabled", 1.0f}, {"destroyFader", 19000.0f}, {"destroyBits", 12.0f}, {"destroyIn", 3.0f}, {"destroyOut", -1.0f}, {"destroyMix", 0.9f}, {"filterCutoff", 0.5f}, {"filterLfo", 0.1f}, {"grainCloud", 0.3f}, {"grainDrift", 0.2f}, {"grainSpace", 0.3f}, {"grainMorph", 0.5f}, {"grainSizeSync", 0.0f}, {"grainReverse", 0.0f}, {"grainMix", 0.0f}, {"grainFreeze", 0.0f}, {"chorusSpeed", 1.0f}, {"chorusMix", 0.0f}, {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.8f}, {"tapeDrive", 0.28f}, {"tapeWear", 0.38f}, {"tapeGlue", 0.4f}, {"tapeNoise", 0.2f}, {"tapeNoiseSpeed", 1.0f}, {"tapeOutput", 0.0f}, {"tapeMix", 1.0f}, {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}, {"msWidth", 1.0f}, {"distortionEnabled", 0.0f}, {"distortionDrive", 0.5f}, {"distortionTone", 0.5f}, {"multiplyDepth", 0.13f}, {"multiplyTone", 0.7f}, {"distortionBias", 0.0f}, {"distortionMode", 0.0f}, {"grainShape", 0.0f}, {"distortionAsym", 0.5f}, {"multiplyFeedback", 0.0f}, {"multiplyLfoShape", 0.0f}, {"multiplyTempoSync", 0.0f}, {"multiplyShimmer", 0.2f}, {"grainPitchQuant", 0.0f}, {"grainHarm", 0.0f}
        }},
        { "Vinyl Through Reel", {
            {"chainSlot0", 1.0f}, {"chainSlot1", 4.0f}, {"chainSlot2", 0.0f}, {"chainSlot3", 0.0f}, {"destroyEnabled", 1.0f}, {"granularEnabled", 0.0f}, {"multiplyEnabled", 0.0f}, {"tapeEnabled", 1.0f}, {"destroyFader", 26040.0f}, {"destroyBits", 12.0f}, {"destroyIn", 0.0f}, {"destroyOut", 0.0f}, {"destroyMix", 0.75f}, {"filterCutoff", 0.6f}, {"filterLfo", 0.0f}, {"grainCloud", 0.3f}, {"grainDrift", 0.2f}, {"grainSpace", 0.3f}, {"grainMorph", 0.5f}, {"grainSizeSync", 0.0f}, {"grainReverse", 0.0f}, {"grainMix", 0.0f}, {"grainFreeze", 0.0f}, {"chorusSpeed", 1.0f}, {"chorusMix", 0.0f}, {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.8f}, {"tapeDrive", 0.12f}, {"tapeWear", 0.14f}, {"tapeGlue", 0.5f}, {"tapeNoise", 0.04f}, {"tapeNoiseSpeed", 1.0f}, {"tapeOutput", 0.0f}, {"tapeMix", 1.0f}, {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}, {"msWidth", 1.0f}, {"distortionEnabled", 0.0f}, {"distortionDrive", 0.5f}, {"distortionTone", 0.5f}, {"multiplyDepth", 0.13f}, {"multiplyTone", 0.7f}, {"distortionBias", 0.0f}, {"distortionMode", 0.0f}, {"grainShape", 0.0f}, {"distortionAsym", 0.5f}, {"multiplyFeedback", 0.0f}, {"multiplyLfoShape", 0.0f}, {"multiplyTempoSync", 0.0f}, {"multiplyShimmer", 0.2f}, {"grainPitchQuant", 0.0f}, {"grainHarm", 0.0f}
        }},
        { "Broken Walkman", {
            {"chainSlot0", 1.0f}, {"chainSlot1", 4.0f}, {"chainSlot2", 0.0f}, {"chainSlot3", 0.0f}, {"destroyEnabled", 1.0f}, {"granularEnabled", 0.0f}, {"multiplyEnabled", 0.0f}, {"tapeEnabled", 1.0f}, {"destroyFader", 13500.0f}, {"destroyBits", 12.0f}, {"destroyIn", 5.0f}, {"destroyOut", -3.0f}, {"destroyMix", 1.0f}, {"filterCutoff", 0.35f}, {"filterLfo", 0.2f}, {"grainCloud", 0.3f}, {"grainDrift", 0.2f}, {"grainSpace", 0.3f}, {"grainMorph", 0.5f}, {"grainSizeSync", 0.0f}, {"grainReverse", 0.0f}, {"grainMix", 0.0f}, {"grainFreeze", 0.0f}, {"chorusSpeed", 1.0f}, {"chorusMix", 0.0f}, {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.8f}, {"tapeDrive", 0.46f}, {"tapeWear", 0.63f}, {"tapeGlue", 0.3f}, {"tapeNoise", 0.49f}, {"tapeNoiseSpeed", 1.0f}, {"tapeOutput", 0.0f}, {"tapeMix", 1.0f}, {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}, {"msWidth", 1.0f}, {"distortionEnabled", 0.0f}, {"distortionDrive", 0.5f}, {"distortionTone", 0.5f}, {"multiplyDepth", 0.13f}, {"multiplyTone", 0.7f}, {"distortionBias", 0.0f}, {"distortionMode", 0.0f}, {"grainShape", 0.0f}, {"distortionAsym", 0.5f}, {"multiplyFeedback", 0.0f}, {"multiplyLfoShape", 0.0f}, {"multiplyTempoSync", 0.0f}, {"multiplyShimmer", 0.2f}, {"grainPitchQuant", 0.0f}, {"grainHarm", 0.0f}
        }},
        { "Warm Sampler Dub", {
            {"chainSlot0", 1.0f}, {"chainSlot1", 4.0f}, {"chainSlot2", 0.0f}, {"chainSlot3", 0.0f}, {"destroyEnabled", 1.0f}, {"granularEnabled", 0.0f}, {"multiplyEnabled", 0.0f}, {"tapeEnabled", 1.0f}, {"destroyFader", 19000.0f}, {"destroyBits", 12.0f}, {"destroyIn", 0.0f}, {"destroyOut", 2.0f}, {"destroyMix", 0.6f}, {"filterCutoff", 0.7f}, {"filterLfo", 0.0f}, {"grainCloud", 0.3f}, {"grainDrift", 0.2f}, {"grainSpace", 0.3f}, {"grainMorph", 0.5f}, {"grainSizeSync", 0.0f}, {"grainReverse", 0.0f}, {"grainMix", 0.0f}, {"grainFreeze", 0.0f}, {"chorusSpeed", 1.0f}, {"chorusMix", 0.0f}, {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.8f}, {"tapeDrive", 0.16f}, {"tapeWear", 0.22f}, {"tapeGlue", 0.6f}, {"tapeNoise", 0.07f}, {"tapeNoiseSpeed", 1.0f}, {"tapeOutput", 0.0f}, {"tapeMix", 1.0f}, {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}, {"msWidth", 1.0f}, {"distortionEnabled", 0.0f}, {"distortionDrive", 0.5f}, {"distortionTone", 0.5f}, {"multiplyDepth", 0.13f}, {"multiplyTone", 0.7f}, {"distortionBias", 0.0f}, {"distortionMode", 0.0f}, {"grainShape", 0.0f}, {"distortionAsym", 0.5f}, {"multiplyFeedback", 0.0f}, {"multiplyLfoShape", 0.0f}, {"multiplyTempoSync", 0.0f}, {"multiplyShimmer", 0.2f}, {"grainPitchQuant", 0.0f}, {"grainHarm", 0.0f}
        }},
        { "Lo-Fi Beat Tape", {
            {"chainSlot0", 1.0f}, {"chainSlot1", 4.0f}, {"chainSlot2", 0.0f}, {"chainSlot3", 0.0f}, {"destroyEnabled", 1.0f}, {"granularEnabled", 0.0f}, {"multiplyEnabled", 0.0f}, {"tapeEnabled", 1.0f}, {"destroyFader", 19000.0f}, {"destroyBits", 12.0f}, {"destroyIn", 4.0f}, {"destroyOut", -2.0f}, {"destroyMix", 0.85f}, {"filterCutoff", 0.55f}, {"filterLfo", 0.0f}, {"grainCloud", 0.3f}, {"grainDrift", 0.2f}, {"grainSpace", 0.3f}, {"grainMorph", 0.5f}, {"grainSizeSync", 0.0f}, {"grainReverse", 0.0f}, {"grainMix", 0.0f}, {"grainFreeze", 0.0f}, {"chorusSpeed", 1.0f}, {"chorusMix", 0.0f}, {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.8f}, {"tapeDrive", 0.19f}, {"tapeWear", 0.24f}, {"tapeGlue", 0.5f}, {"tapeNoise", 0.09f}, {"tapeNoiseSpeed", 1.0f}, {"tapeOutput", 0.0f}, {"tapeMix", 1.0f}, {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}, {"msWidth", 1.0f}, {"distortionEnabled", 0.0f}, {"distortionDrive", 0.5f}, {"distortionTone", 0.5f}, {"multiplyDepth", 0.13f}, {"multiplyTone", 0.7f}, {"distortionBias", 0.0f}, {"distortionMode", 0.0f}, {"grainShape", 0.0f}, {"distortionAsym", 0.5f}, {"multiplyFeedback", 0.0f}, {"multiplyLfoShape", 0.0f}, {"multiplyTempoSync", 0.0f}, {"multiplyShimmer", 0.2f}, {"grainPitchQuant", 0.0f}, {"grainHarm", 0.0f}
        }},
        { "Crushed Tape Drone", {
            {"chainSlot0", 1.0f}, {"chainSlot1", 4.0f}, {"chainSlot2", 0.0f}, {"chainSlot3", 0.0f}, {"destroyEnabled", 1.0f}, {"granularEnabled", 0.0f}, {"multiplyEnabled", 0.0f}, {"tapeEnabled", 1.0f}, {"destroyFader", 6000.0f}, {"destroyBits", 12.0f}, {"destroyIn", 8.0f}, {"destroyOut", -5.0f}, {"destroyMix", 1.0f}, {"filterCutoff", 0.45f}, {"filterLfo", 0.0f}, {"grainCloud", 0.3f}, {"grainDrift", 0.2f}, {"grainSpace", 0.3f}, {"grainMorph", 0.5f}, {"grainSizeSync", 0.0f}, {"grainReverse", 0.0f}, {"grainMix", 0.0f}, {"grainFreeze", 0.0f}, {"chorusSpeed", 1.0f}, {"chorusMix", 0.0f}, {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.8f}, {"tapeDrive", 0.33f}, {"tapeWear", 0.44f}, {"tapeGlue", 0.65f}, {"tapeNoise", 0.26f}, {"tapeNoiseSpeed", 1.0f}, {"tapeOutput", 0.0f}, {"tapeMix", 1.0f}, {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}, {"msWidth", 1.0f}, {"distortionEnabled", 0.0f}, {"distortionDrive", 0.5f}, {"distortionTone", 0.5f}, {"multiplyDepth", 0.13f}, {"multiplyTone", 0.7f}, {"distortionBias", 0.0f}, {"distortionMode", 0.0f}, {"grainShape", 0.0f}, {"distortionAsym", 0.5f}, {"multiplyFeedback", 0.0f}, {"multiplyLfoShape", 0.0f}, {"multiplyTempoSync", 0.0f}, {"multiplyShimmer", 0.2f}, {"grainPitchQuant", 0.0f}, {"grainHarm", 0.0f}
        }},
        { "Subtle Degradation", {
            {"chainSlot0", 1.0f}, {"chainSlot1", 4.0f}, {"chainSlot2", 0.0f}, {"chainSlot3", 0.0f}, {"destroyEnabled", 1.0f}, {"granularEnabled", 0.0f}, {"multiplyEnabled", 0.0f}, {"tapeEnabled", 1.0f}, {"destroyFader", 19000.0f}, {"destroyBits", 12.0f}, {"destroyIn", 0.0f}, {"destroyOut", 0.0f}, {"destroyMix", 0.4f}, {"filterCutoff", 0.85f}, {"filterLfo", 0.0f}, {"grainCloud", 0.3f}, {"grainDrift", 0.2f}, {"grainSpace", 0.3f}, {"grainMorph", 0.5f}, {"grainSizeSync", 0.0f}, {"grainReverse", 0.0f}, {"grainMix", 0.0f}, {"grainFreeze", 0.0f}, {"chorusSpeed", 1.0f}, {"chorusMix", 0.0f}, {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.8f}, {"tapeDrive", 0.07f}, {"tapeWear", 0.12f}, {"tapeGlue", 0.5f}, {"tapeNoise", 0.02f}, {"tapeNoiseSpeed", 1.0f}, {"tapeOutput", 0.0f}, {"tapeMix", 0.6f}, {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}, {"msWidth", 1.0f}, {"distortionEnabled", 0.0f}, {"distortionDrive", 0.5f}, {"distortionTone", 0.5f}, {"multiplyDepth", 0.13f}, {"multiplyTone", 0.7f}, {"distortionBias", 0.0f}, {"distortionMode", 0.0f}, {"grainShape", 0.0f}, {"distortionAsym", 0.5f}, {"multiplyFeedback", 0.0f}, {"multiplyLfoShape", 0.0f}, {"multiplyTempoSync", 0.0f}, {"multiplyShimmer", 0.2f}, {"grainPitchQuant", 0.0f}, {"grainHarm", 0.0f}
        }},
        { "78 RPM Wax Cylinder", {
            {"chainSlot0", 1.0f}, {"chainSlot1", 4.0f}, {"chainSlot2", 0.0f}, {"chainSlot3", 0.0f}, {"destroyEnabled", 1.0f}, {"granularEnabled", 0.0f}, {"multiplyEnabled", 0.0f}, {"tapeEnabled", 1.0f}, {"destroyFader", 4000.0f}, {"destroyBits", 12.0f}, {"destroyIn", 2.0f}, {"destroyOut", 0.0f}, {"destroyMix", 1.0f}, {"filterCutoff", 0.3f}, {"filterLfo", 0.05f}, {"grainCloud", 0.3f}, {"grainDrift", 0.2f}, {"grainSpace", 0.3f}, {"grainMorph", 0.5f}, {"grainSizeSync", 0.0f}, {"grainReverse", 0.0f}, {"grainMix", 0.0f}, {"grainFreeze", 0.0f}, {"chorusSpeed", 1.0f}, {"chorusMix", 0.0f}, {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.8f}, {"tapeDrive", 0.37f}, {"tapeWear", 0.5f}, {"tapeGlue", 0.35f}, {"tapeNoise", 0.33f}, {"tapeNoiseSpeed", 1.0f}, {"tapeOutput", 0.0f}, {"tapeMix", 1.0f}, {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}, {"msWidth", 0.7f}
        }},
        { "Shimmer Ensemble", {
            {"chainSlot0", 2.0f}, {"chainSlot1", 3.0f}, {"chainSlot2", 0.0f}, {"chainSlot3", 0.0f}, {"destroyEnabled", 0.0f}, {"granularEnabled", 1.0f}, {"multiplyEnabled", 1.0f}, {"tapeEnabled", 0.0f}, {"destroyFader", 19000.0f}, {"destroyBits", 12.0f}, {"destroyIn", 0.0f}, {"destroyOut", 0.0f}, {"destroyMix", 1.0f}, {"filterCutoff", 1.0f}, {"filterLfo", 0.0f}, {"grainCloud", 0.5f}, {"grainDrift", 0.15f}, {"grainSpace", 0.6f}, {"grainMorph", 0.6f}, {"grainSizeSync", 0.0f}, {"grainReverse", 0.0f}, {"grainMix", 0.6f}, {"grainFreeze", 0.0f}, {"chorusSpeed", 0.7f}, {"chorusMix", 0.5f}, {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.6f}, {"tapeDrive", 0.02f}, {"tapeWear", 0.0f}, {"tapeGlue", 0.5f}, {"tapeNoise", 0.0f}, {"tapeNoiseSpeed", 1.0f}, {"tapeOutput", 0.0f}, {"tapeMix", 1.0f}, {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}, {"msWidth", 1.4f}
        }},
        { "Wide Cloud Layer", {
            {"chainSlot0", 2.0f}, {"chainSlot1", 3.0f}, {"chainSlot2", 0.0f}, {"chainSlot3", 0.0f}, {"destroyEnabled", 0.0f}, {"granularEnabled", 1.0f}, {"multiplyEnabled", 1.0f}, {"tapeEnabled", 0.0f}, {"destroyFader", 19000.0f}, {"destroyBits", 12.0f}, {"destroyIn", 0.0f}, {"destroyOut", 0.0f}, {"destroyMix", 1.0f}, {"filterCutoff", 1.0f}, {"filterLfo", 0.0f}, {"grainCloud", 0.7f}, {"grainDrift", 0.3f}, {"grainSpace", 0.7f}, {"grainMorph", 0.5f}, {"grainSizeSync", 0.0f}, {"grainReverse", 0.0f}, {"grainMix", 0.75f}, {"grainFreeze", 0.0f}, {"chorusSpeed", 0.4f}, {"chorusMix", 0.6f}, {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.4f}, {"tapeDrive", 0.02f}, {"tapeWear", 0.0f}, {"tapeGlue", 0.5f}, {"tapeNoise", 0.0f}, {"tapeNoiseSpeed", 1.0f}, {"tapeOutput", 0.0f}, {"tapeMix", 1.0f}, {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}, {"msWidth", 1.7f}
        }},
        { "Frozen Chorus", {
            {"chainSlot0", 2.0f}, {"chainSlot1", 3.0f}, {"chainSlot2", 0.0f}, {"chainSlot3", 0.0f}, {"destroyEnabled", 0.0f}, {"granularEnabled", 1.0f}, {"multiplyEnabled", 1.0f}, {"tapeEnabled", 0.0f}, {"destroyFader", 19000.0f}, {"destroyBits", 12.0f}, {"destroyIn", 0.0f}, {"destroyOut", 0.0f}, {"destroyMix", 1.0f}, {"filterCutoff", 1.0f}, {"filterLfo", 0.0f}, {"grainCloud", 0.3f}, {"grainDrift", 0.05f}, {"grainSpace", 0.85f}, {"grainMorph", 0.5f}, {"grainSizeSync", 0.0f}, {"grainReverse", 0.0f}, {"grainMix", 1.0f}, {"grainFreeze", 1.0f}, {"chorusSpeed", 0.5f}, {"chorusMix", 0.55f}, {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.7f}, {"tapeDrive", 0.02f}, {"tapeWear", 0.0f}, {"tapeGlue", 0.5f}, {"tapeNoise", 0.0f}, {"tapeNoiseSpeed", 1.0f}, {"tapeOutput", 0.0f}, {"tapeMix", 1.0f}, {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}, {"msWidth", 1.5f}, {"distortionEnabled", 0.0f}, {"distortionDrive", 0.5f}, {"distortionTone", 0.5f}, {"multiplyDepth", 0.13f}, {"multiplyTone", 0.7f}, {"distortionBias", 0.0f}, {"distortionMode", 0.0f}, {"grainShape", 0.0f}, {"distortionAsym", 0.5f}, {"multiplyFeedback", 0.0f}, {"multiplyLfoShape", 0.0f}, {"multiplyTempoSync", 0.0f}, {"multiplyShimmer", 0.2f}, {"grainPitchQuant", 0.0f}, {"grainHarm", 0.0f}
        }},
        { "Scatter Voices", {
            {"chainSlot0", 2.0f}, {"chainSlot1", 3.0f}, {"chainSlot2", 0.0f}, {"chainSlot3", 0.0f}, {"destroyEnabled", 0.0f}, {"granularEnabled", 1.0f}, {"multiplyEnabled", 1.0f}, {"tapeEnabled", 0.0f}, {"destroyFader", 19000.0f}, {"destroyBits", 12.0f}, {"destroyIn", 0.0f}, {"destroyOut", 0.0f}, {"destroyMix", 1.0f}, {"filterCutoff", 1.0f}, {"filterLfo", 0.0f}, {"grainCloud", 0.85f}, {"grainDrift", 0.7f}, {"grainSpace", 0.35f}, {"grainMorph", 0.5f}, {"grainSizeSync", 0.0f}, {"grainReverse", 0.0f}, {"grainMix", 0.7f}, {"grainFreeze", 0.0f}, {"chorusSpeed", 1.2f}, {"chorusMix", 0.4f}, {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.5f}, {"tapeDrive", 0.02f}, {"tapeWear", 0.0f}, {"tapeGlue", 0.5f}, {"tapeNoise", 0.0f}, {"tapeNoiseSpeed", 1.0f}, {"tapeOutput", 0.0f}, {"tapeMix", 1.0f}, {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}, {"msWidth", 1.3f}, {"distortionEnabled", 0.0f}, {"distortionDrive", 0.5f}, {"distortionTone", 0.5f}, {"multiplyDepth", 0.13f}, {"multiplyTone", 0.7f}, {"distortionBias", 0.0f}, {"distortionMode", 0.0f}, {"grainShape", 0.0f}, {"distortionAsym", 0.5f}, {"multiplyFeedback", 0.0f}, {"multiplyLfoShape", 0.0f}, {"multiplyTempoSync", 0.0f}, {"multiplyShimmer", 0.2f}, {"grainPitchQuant", 0.0f}, {"grainHarm", 0.0f}
        }},
        { "Ambient Multiplier", {
            {"chainSlot0", 2.0f}, {"chainSlot1", 3.0f}, {"chainSlot2", 0.0f}, {"chainSlot3", 0.0f}, {"destroyEnabled", 0.0f}, {"granularEnabled", 1.0f}, {"multiplyEnabled", 1.0f}, {"tapeEnabled", 0.0f}, {"destroyFader", 19000.0f}, {"destroyBits", 12.0f}, {"destroyIn", 0.0f}, {"destroyOut", 0.0f}, {"destroyMix", 1.0f}, {"filterCutoff", 1.0f}, {"filterLfo", 0.0f}, {"grainCloud", 0.4f}, {"grainDrift", 0.2f}, {"grainSpace", 0.75f}, {"grainMorph", 0.55f}, {"grainSizeSync", 0.0f}, {"grainReverse", 0.0f}, {"grainMix", 0.5f}, {"grainFreeze", 0.0f}, {"chorusSpeed", 0.2f}, {"chorusMix", 0.7f}, {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.3f}, {"tapeDrive", 0.02f}, {"tapeWear", 0.0f}, {"tapeGlue", 0.5f}, {"tapeNoise", 0.0f}, {"tapeNoiseSpeed", 1.0f}, {"tapeOutput", 0.0f}, {"tapeMix", 1.0f}, {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}, {"msWidth", 1.6f}
        }},
        { "Pitched Swirl", {
            {"chainSlot0", 2.0f}, {"chainSlot1", 3.0f}, {"chainSlot2", 0.0f}, {"chainSlot3", 0.0f}, {"destroyEnabled", 0.0f}, {"granularEnabled", 1.0f}, {"multiplyEnabled", 1.0f}, {"tapeEnabled", 0.0f}, {"destroyFader", 19000.0f}, {"destroyBits", 12.0f}, {"destroyIn", 0.0f}, {"destroyOut", 0.0f}, {"destroyMix", 1.0f}, {"filterCutoff", 1.0f}, {"filterLfo", 0.0f}, {"grainCloud", 0.6f}, {"grainDrift", 0.25f}, {"grainSpace", 0.5f}, {"grainMorph", 0.72f}, {"grainSizeSync", 0.0f}, {"grainReverse", 0.0f}, {"grainMix", 0.65f}, {"grainFreeze", 0.0f}, {"chorusSpeed", 2.5f}, {"chorusMix", 0.45f}, {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.8f}, {"tapeDrive", 0.02f}, {"tapeWear", 0.0f}, {"tapeGlue", 0.5f}, {"tapeNoise", 0.0f}, {"tapeNoiseSpeed", 1.0f}, {"tapeOutput", 0.0f}, {"tapeMix", 1.0f}, {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}, {"msWidth", 1.2f}
        }},
        { "Grain Nebula", {
            {"chainSlot0", 2.0f}, {"chainSlot1", 3.0f}, {"chainSlot2", 0.0f}, {"chainSlot3", 0.0f}, {"destroyEnabled", 0.0f}, {"granularEnabled", 1.0f}, {"multiplyEnabled", 1.0f}, {"tapeEnabled", 0.0f}, {"destroyFader", 19000.0f}, {"destroyBits", 12.0f}, {"destroyIn", 0.0f}, {"destroyOut", 0.0f}, {"destroyMix", 1.0f}, {"filterCutoff", 1.0f}, {"filterLfo", 0.0f}, {"grainCloud", 0.95f}, {"grainDrift", 0.5f}, {"grainSpace", 0.8f}, {"grainMorph", 0.45f}, {"grainSizeSync", 0.0f}, {"grainReverse", 0.0f}, {"grainMix", 0.85f}, {"grainFreeze", 0.0f}, {"chorusSpeed", 0.3f}, {"chorusMix", 0.65f}, {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.5f}, {"tapeDrive", 0.02f}, {"tapeWear", 0.0f}, {"tapeGlue", 0.5f}, {"tapeNoise", 0.0f}, {"tapeNoiseSpeed", 1.0f}, {"tapeOutput", 0.0f}, {"tapeMix", 1.0f}, {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}, {"msWidth", 2.0f}
        }},
        { "Suboctave Bloom", {
            {"chainSlot0", 2.0f}, {"chainSlot1", 3.0f}, {"chainSlot2", 0.0f}, {"chainSlot3", 0.0f}, {"destroyEnabled", 0.0f}, {"granularEnabled", 1.0f}, {"multiplyEnabled", 1.0f}, {"tapeEnabled", 0.0f}, {"destroyFader", 19000.0f}, {"destroyBits", 12.0f}, {"destroyIn", 0.0f}, {"destroyOut", 0.0f}, {"destroyMix", 1.0f}, {"filterCutoff", 1.0f}, {"filterLfo", 0.0f}, {"grainCloud", 0.3f}, {"grainDrift", 0.1f}, {"grainSpace", 0.6f}, {"grainMorph", 0.0f}, {"grainSizeSync", 0.0f}, {"grainReverse", 0.0f}, {"grainMix", 0.7f}, {"grainFreeze", 0.0f}, {"chorusSpeed", 0.6f}, {"chorusMix", 0.5f}, {"multiplyPanOuter", 0.8f}, {"multiplyPanInner", 0.6f}, {"tapeDrive", 0.02f}, {"tapeWear", 0.0f}, {"tapeGlue", 0.5f}, {"tapeNoise", 0.0f}, {"tapeNoiseSpeed", 1.0f}, {"tapeOutput", 0.0f}, {"tapeMix", 1.0f}, {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}, {"msWidth", 1.3f}, {"distortionEnabled", 0.0f}, {"distortionDrive", 0.5f}, {"distortionTone", 0.5f}, {"multiplyDepth", 0.13f}, {"multiplyTone", 0.7f}, {"distortionBias", 0.0f}, {"distortionMode", 0.0f}, {"grainShape", 0.0f}, {"distortionAsym", 0.5f}, {"multiplyFeedback", 0.0f}, {"multiplyLfoShape", 0.0f}, {"multiplyTempoSync", 0.0f}, {"multiplyShimmer", 0.2f}, {"grainPitchQuant", 0.0f}, {"grainHarm", 0.0f}
        }},
        { "Grain Through Tape", {
            {"chainSlot0", 2.0f}, {"chainSlot1", 4.0f}, {"chainSlot2", 0.0f}, {"chainSlot3", 0.0f}, {"destroyEnabled", 0.0f}, {"granularEnabled", 1.0f}, {"multiplyEnabled", 0.0f}, {"tapeEnabled", 1.0f}, {"destroyFader", 19000.0f}, {"destroyBits", 12.0f}, {"destroyIn", 0.0f}, {"destroyOut", 0.0f}, {"destroyMix", 1.0f}, {"filterCutoff", 1.0f}, {"filterLfo", 0.0f}, {"grainCloud", 0.4f}, {"grainDrift", 0.2f}, {"grainSpace", 0.4f}, {"grainMorph", 0.5f}, {"grainSizeSync", 0.0f}, {"grainReverse", 0.0f}, {"grainMix", 0.6f}, {"grainFreeze", 0.0f}, {"chorusSpeed", 1.0f}, {"chorusMix", 0.0f}, {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.8f}, {"tapeDrive", 0.1f}, {"tapeWear", 0.12f}, {"tapeGlue", 0.5f}, {"tapeNoise", 0.03f}, {"tapeNoiseSpeed", 1.0f}, {"tapeOutput", 0.0f}, {"tapeMix", 1.0f}, {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}, {"msWidth", 1.0f}, {"distortionEnabled", 0.0f}, {"distortionDrive", 0.5f}, {"distortionTone", 0.5f}, {"multiplyDepth", 0.13f}, {"multiplyTone", 0.7f}, {"distortionBias", 0.0f}, {"distortionMode", 0.0f}, {"grainShape", 0.0f}, {"distortionAsym", 0.5f}, {"multiplyFeedback", 0.0f}, {"multiplyLfoShape", 0.0f}, {"multiplyTempoSync", 0.0f}, {"multiplyShimmer", 0.2f}, {"grainPitchQuant", 0.0f}, {"grainHarm", 0.0f}
        }},
        { "Organic Texture", {
            {"chainSlot0", 2.0f}, {"chainSlot1", 4.0f}, {"chainSlot2", 0.0f}, {"chainSlot3", 0.0f}, {"destroyEnabled", 0.0f}, {"granularEnabled", 1.0f}, {"multiplyEnabled", 0.0f}, {"tapeEnabled", 1.0f}, {"destroyFader", 19000.0f}, {"destroyBits", 12.0f}, {"destroyIn", 0.0f}, {"destroyOut", 0.0f}, {"destroyMix", 1.0f}, {"filterCutoff", 1.0f}, {"filterLfo", 0.0f}, {"grainCloud", 0.35f}, {"grainDrift", 0.35f}, {"grainSpace", 0.5f}, {"grainMorph", 0.48f}, {"grainSizeSync", 0.0f}, {"grainReverse", 0.0f}, {"grainMix", 0.5f}, {"grainFreeze", 0.0f}, {"chorusSpeed", 1.0f}, {"chorusMix", 0.0f}, {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.8f}, {"tapeDrive", 0.12f}, {"tapeWear", 0.14f}, {"tapeGlue", 0.55f}, {"tapeNoise", 0.04f}, {"tapeNoiseSpeed", 1.0f}, {"tapeOutput", 0.0f}, {"tapeMix", 1.0f}, {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}, {"msWidth", 1.2f}
        }},
        { "Frozen On Tape", {
            {"chainSlot0", 2.0f}, {"chainSlot1", 4.0f}, {"chainSlot2", 0.0f}, {"chainSlot3", 0.0f}, {"destroyEnabled", 0.0f}, {"granularEnabled", 1.0f}, {"multiplyEnabled", 0.0f}, {"tapeEnabled", 1.0f}, {"destroyFader", 19000.0f}, {"destroyBits", 12.0f}, {"destroyIn", 0.0f}, {"destroyOut", 0.0f}, {"destroyMix", 1.0f}, {"filterCutoff", 1.0f}, {"filterLfo", 0.0f}, {"grainCloud", 0.25f}, {"grainDrift", 0.05f}, {"grainSpace", 0.8f}, {"grainMorph", 0.5f}, {"grainSizeSync", 0.0f}, {"grainReverse", 0.0f}, {"grainMix", 1.0f}, {"grainFreeze", 1.0f}, {"chorusSpeed", 1.0f}, {"chorusMix", 0.0f}, {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.8f}, {"tapeDrive", 0.16f}, {"tapeWear", 0.2f}, {"tapeGlue", 0.5f}, {"tapeNoise", 0.07f}, {"tapeNoiseSpeed", 1.0f}, {"tapeOutput", 0.0f}, {"tapeMix", 1.0f}, {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}, {"msWidth", 1.0f}, {"distortionEnabled", 0.0f}, {"distortionDrive", 0.5f}, {"distortionTone", 0.5f}, {"multiplyDepth", 0.13f}, {"multiplyTone", 0.7f}, {"distortionBias", 0.0f}, {"distortionMode", 0.0f}, {"grainShape", 0.0f}, {"distortionAsym", 0.5f}, {"multiplyFeedback", 0.0f}, {"multiplyLfoShape", 0.0f}, {"multiplyTempoSync", 0.0f}, {"multiplyShimmer", 0.2f}, {"grainPitchQuant", 0.0f}, {"grainHarm", 0.0f}
        }},
        { "Warped Shimmer", {
            {"chainSlot0", 2.0f}, {"chainSlot1", 4.0f}, {"chainSlot2", 0.0f}, {"chainSlot3", 0.0f}, {"destroyEnabled", 0.0f}, {"granularEnabled", 1.0f}, {"multiplyEnabled", 0.0f}, {"tapeEnabled", 1.0f}, {"destroyFader", 19000.0f}, {"destroyBits", 12.0f}, {"destroyIn", 0.0f}, {"destroyOut", 0.0f}, {"destroyMix", 1.0f}, {"filterCutoff", 1.0f}, {"filterLfo", 0.0f}, {"grainCloud", 0.65f}, {"grainDrift", 0.15f}, {"grainSpace", 0.65f}, {"grainMorph", 0.65f}, {"grainSizeSync", 0.0f}, {"grainReverse", 0.0f}, {"grainMix", 0.7f}, {"grainFreeze", 0.0f}, {"chorusSpeed", 1.0f}, {"chorusMix", 0.0f}, {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.8f}, {"tapeDrive", 0.13f}, {"tapeWear", 0.18f}, {"tapeGlue", 0.5f}, {"tapeNoise", 0.05f}, {"tapeNoiseSpeed", 1.0f}, {"tapeOutput", 0.0f}, {"tapeMix", 1.0f}, {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}, {"msWidth", 1.3f}, {"distortionEnabled", 0.0f}, {"distortionDrive", 0.5f}, {"distortionTone", 0.5f}, {"multiplyDepth", 0.13f}, {"multiplyTone", 0.7f}, {"distortionBias", 0.0f}, {"distortionMode", 0.0f}, {"grainShape", 0.0f}, {"distortionAsym", 0.5f}, {"multiplyFeedback", 0.0f}, {"multiplyLfoShape", 0.0f}, {"multiplyTempoSync", 0.0f}, {"multiplyShimmer", 0.2f}, {"grainPitchQuant", 0.0f}, {"grainHarm", 0.0f}
        }},
        { "Tape Scatter", {
            {"chainSlot0", 2.0f}, {"chainSlot1", 4.0f}, {"chainSlot2", 0.0f}, {"chainSlot3", 0.0f}, {"destroyEnabled", 0.0f}, {"granularEnabled", 1.0f}, {"multiplyEnabled", 0.0f}, {"tapeEnabled", 1.0f}, {"destroyFader", 19000.0f}, {"destroyBits", 12.0f}, {"destroyIn", 0.0f}, {"destroyOut", 0.0f}, {"destroyMix", 1.0f}, {"filterCutoff", 1.0f}, {"filterLfo", 0.0f}, {"grainCloud", 0.8f}, {"grainDrift", 0.75f}, {"grainSpace", 0.3f}, {"grainMorph", 0.5f}, {"grainSizeSync", 0.0f}, {"grainReverse", 0.0f}, {"grainMix", 0.8f}, {"grainFreeze", 0.0f}, {"chorusSpeed", 1.0f}, {"chorusMix", 0.0f}, {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.8f}, {"tapeDrive", 0.12f}, {"tapeWear", 0.14f}, {"tapeGlue", 0.5f}, {"tapeNoise", 0.04f}, {"tapeNoiseSpeed", 1.0f}, {"tapeOutput", 0.0f}, {"tapeMix", 1.0f}, {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}, {"msWidth", 1.0f}, {"distortionEnabled", 0.0f}, {"distortionDrive", 0.5f}, {"distortionTone", 0.5f}, {"multiplyDepth", 0.13f}, {"multiplyTone", 0.7f}, {"distortionBias", 0.0f}, {"distortionMode", 0.0f}, {"grainShape", 0.0f}, {"distortionAsym", 0.5f}, {"multiplyFeedback", 0.0f}, {"multiplyLfoShape", 0.0f}, {"multiplyTempoSync", 0.0f}, {"multiplyShimmer", 0.2f}, {"grainPitchQuant", 0.0f}, {"grainHarm", 0.0f}
        }},
        { "Vintage Ambience", {
            {"chainSlot0", 2.0f}, {"chainSlot1", 4.0f}, {"chainSlot2", 0.0f}, {"chainSlot3", 0.0f}, {"destroyEnabled", 0.0f}, {"granularEnabled", 1.0f}, {"multiplyEnabled", 0.0f}, {"tapeEnabled", 1.0f}, {"destroyFader", 19000.0f}, {"destroyBits", 12.0f}, {"destroyIn", 0.0f}, {"destroyOut", 0.0f}, {"destroyMix", 1.0f}, {"filterCutoff", 1.0f}, {"filterLfo", 0.0f}, {"grainCloud", 0.3f}, {"grainDrift", 0.3f}, {"grainSpace", 0.6f}, {"grainMorph", 0.52f}, {"grainSizeSync", 0.0f}, {"grainReverse", 0.0f}, {"grainMix", 0.55f}, {"grainFreeze", 0.0f}, {"chorusSpeed", 1.0f}, {"chorusMix", 0.0f}, {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.8f}, {"tapeDrive", 0.08f}, {"tapeWear", 0.11f}, {"tapeGlue", 0.5f}, {"tapeNoise", 0.03f}, {"tapeNoiseSpeed", 1.0f}, {"tapeOutput", 0.0f}, {"tapeMix", 1.0f}, {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}, {"msWidth", 1.2f}
        }},
        { "Cloud Tape Pad", {
            {"chainSlot0", 2.0f}, {"chainSlot1", 4.0f}, {"chainSlot2", 0.0f}, {"chainSlot3", 0.0f}, {"destroyEnabled", 0.0f}, {"granularEnabled", 1.0f}, {"multiplyEnabled", 0.0f}, {"tapeEnabled", 1.0f}, {"destroyFader", 19000.0f}, {"destroyBits", 12.0f}, {"destroyIn", 0.0f}, {"destroyOut", 0.0f}, {"destroyMix", 1.0f}, {"filterCutoff", 1.0f}, {"filterLfo", 0.0f}, {"grainCloud", 0.5f}, {"grainDrift", 0.1f}, {"grainSpace", 0.8f}, {"grainMorph", 0.5f}, {"grainSizeSync", 0.0f}, {"grainReverse", 0.0f}, {"grainMix", 0.7f}, {"grainFreeze", 0.0f}, {"chorusSpeed", 1.0f}, {"chorusMix", 0.0f}, {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.8f}, {"tapeDrive", 0.15f}, {"tapeWear", 0.2f}, {"tapeGlue", 0.6f}, {"tapeNoise", 0.06f}, {"tapeNoiseSpeed", 1.0f}, {"tapeOutput", 0.0f}, {"tapeMix", 1.0f}, {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}, {"msWidth", 1.0f}, {"distortionEnabled", 0.0f}, {"distortionDrive", 0.5f}, {"distortionTone", 0.5f}, {"multiplyDepth", 0.13f}, {"multiplyTone", 0.7f}, {"distortionBias", 0.0f}, {"distortionMode", 0.0f}, {"grainShape", 0.0f}, {"distortionAsym", 0.5f}, {"multiplyFeedback", 0.0f}, {"multiplyLfoShape", 0.0f}, {"multiplyTempoSync", 0.0f}, {"multiplyShimmer", 0.2f}, {"grainPitchQuant", 0.0f}, {"grainHarm", 0.0f}
        }},
        { "Reverse Tape", {
            {"chainSlot0", 2.0f}, {"chainSlot1", 4.0f}, {"chainSlot2", 0.0f}, {"chainSlot3", 0.0f}, {"destroyEnabled", 0.0f}, {"granularEnabled", 1.0f}, {"multiplyEnabled", 0.0f}, {"tapeEnabled", 1.0f}, {"destroyFader", 19000.0f}, {"destroyBits", 12.0f}, {"destroyIn", 0.0f}, {"destroyOut", 0.0f}, {"destroyMix", 1.0f}, {"filterCutoff", 1.0f}, {"filterLfo", 0.0f}, {"grainCloud", 0.4f}, {"grainDrift", 0.6f}, {"grainSpace", 0.4f}, {"grainMorph", 0.5f}, {"grainSizeSync", 0.0f}, {"grainReverse", 0.8f}, {"grainMix", 0.7f}, {"grainFreeze", 0.0f}, {"chorusSpeed", 1.0f}, {"chorusMix", 0.0f}, {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.8f}, {"tapeDrive", 0.18f}, {"tapeWear", 0.25f}, {"tapeGlue", 0.5f}, {"tapeNoise", 0.09f}, {"tapeNoiseSpeed", 1.0f}, {"tapeOutput", 0.0f}, {"tapeMix", 1.0f}, {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}, {"msWidth", 1.0f}, {"distortionEnabled", 0.0f}, {"distortionDrive", 0.5f}, {"distortionTone", 0.5f}, {"multiplyDepth", 0.13f}, {"multiplyTone", 0.7f}, {"distortionBias", 0.0f}, {"distortionMode", 0.0f}, {"grainShape", 0.0f}, {"distortionAsym", 0.5f}, {"multiplyFeedback", 0.0f}, {"multiplyLfoShape", 0.0f}, {"multiplyTempoSync", 0.0f}, {"multiplyShimmer", 0.2f}, {"grainPitchQuant", 0.0f}, {"grainHarm", 0.0f}
        }},
        { "Gritty Cloud", {
            {"chainSlot0", 1.0f}, {"chainSlot1", 2.0f}, {"chainSlot2", 0.0f}, {"chainSlot3", 0.0f}, {"destroyEnabled", 1.0f}, {"granularEnabled", 1.0f}, {"multiplyEnabled", 0.0f}, {"tapeEnabled", 0.0f}, {"destroyFader", 19000.0f}, {"destroyBits", 10.0f}, {"destroyIn", 4.0f}, {"destroyOut", -2.0f}, {"destroyMix", 1.0f}, {"filterCutoff", 0.8f}, {"filterLfo", 0.0f}, {"grainCloud", 0.5f}, {"grainDrift", 0.2f}, {"grainSpace", 0.5f}, {"grainMorph", 0.5f}, {"grainSizeSync", 0.0f}, {"grainReverse", 0.0f}, {"grainMix", 0.6f}, {"grainFreeze", 0.0f}, {"chorusSpeed", 1.0f}, {"chorusMix", 0.0f}, {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.8f}, {"tapeDrive", 0.02f}, {"tapeWear", 0.0f}, {"tapeGlue", 0.5f}, {"tapeNoise", 0.0f}, {"tapeNoiseSpeed", 1.0f}, {"tapeOutput", 0.0f}, {"tapeMix", 1.0f}, {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}, {"msWidth", 1.0f}, {"distortionEnabled", 0.0f}, {"distortionDrive", 0.5f}, {"distortionTone", 0.5f}, {"multiplyDepth", 0.13f}, {"multiplyTone", 0.7f}, {"distortionBias", 0.0f}, {"distortionMode", 0.0f}, {"grainShape", 0.0f}, {"distortionAsym", 0.5f}, {"multiplyFeedback", 0.0f}, {"multiplyLfoShape", 0.0f}, {"multiplyTempoSync", 0.0f}, {"multiplyShimmer", 0.2f}, {"grainPitchQuant", 0.0f}, {"grainHarm", 0.0f}
        }},
        { "Crushed Ambience", {
            {"chainSlot0", 1.0f}, {"chainSlot1", 2.0f}, {"chainSlot2", 0.0f}, {"chainSlot3", 0.0f}, {"destroyEnabled", 1.0f}, {"granularEnabled", 1.0f}, {"multiplyEnabled", 0.0f}, {"tapeEnabled", 0.0f}, {"destroyFader", 8000.0f}, {"destroyBits", 8.0f}, {"destroyIn", 0.0f}, {"destroyOut", 0.0f}, {"destroyMix", 1.0f}, {"filterCutoff", 0.5f}, {"filterLfo", 0.0f}, {"grainCloud", 0.7f}, {"grainDrift", 0.3f}, {"grainSpace", 0.7f}, {"grainMorph", 0.5f}, {"grainSizeSync", 0.0f}, {"grainReverse", 0.0f}, {"grainMix", 0.7f}, {"grainFreeze", 0.0f}, {"chorusSpeed", 1.0f}, {"chorusMix", 0.0f}, {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.8f}, {"tapeDrive", 0.02f}, {"tapeWear", 0.0f}, {"tapeGlue", 0.5f}, {"tapeNoise", 0.0f}, {"tapeNoiseSpeed", 1.0f}, {"tapeOutput", 0.0f}, {"tapeMix", 1.0f}, {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}, {"msWidth", 1.0f}, {"distortionEnabled", 0.0f}, {"distortionDrive", 0.5f}, {"distortionTone", 0.5f}, {"multiplyDepth", 0.13f}, {"multiplyTone", 0.7f}, {"distortionBias", 0.0f}, {"distortionMode", 0.0f}, {"grainShape", 0.0f}, {"distortionAsym", 0.5f}, {"multiplyFeedback", 0.0f}, {"multiplyLfoShape", 0.0f}, {"multiplyTempoSync", 0.0f}, {"multiplyShimmer", 0.2f}, {"grainPitchQuant", 0.0f}, {"grainHarm", 0.0f}
        }},
        { "Broken Shimmer", {
            {"chainSlot0", 1.0f}, {"chainSlot1", 2.0f}, {"chainSlot2", 0.0f}, {"chainSlot3", 0.0f}, {"destroyEnabled", 1.0f}, {"granularEnabled", 1.0f}, {"multiplyEnabled", 0.0f}, {"tapeEnabled", 0.0f}, {"destroyFader", 6000.0f}, {"destroyBits", 6.0f}, {"destroyIn", 6.0f}, {"destroyOut", 0.0f}, {"destroyMix", 1.0f}, {"filterCutoff", 0.9f}, {"filterLfo", 0.0f}, {"grainCloud", 0.8f}, {"grainDrift", 0.1f}, {"grainSpace", 0.8f}, {"grainMorph", 0.6f}, {"grainSizeSync", 0.0f}, {"grainReverse", 0.0f}, {"grainMix", 0.8f}, {"grainFreeze", 0.0f}, {"chorusSpeed", 1.0f}, {"chorusMix", 0.0f}, {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.8f}, {"tapeDrive", 0.02f}, {"tapeWear", 0.0f}, {"tapeGlue", 0.5f}, {"tapeNoise", 0.0f}, {"tapeNoiseSpeed", 1.0f}, {"tapeOutput", 0.0f}, {"tapeMix", 1.0f}, {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}, {"msWidth", 1.0f}, {"distortionEnabled", 0.0f}, {"distortionDrive", 0.5f}, {"distortionTone", 0.5f}, {"multiplyDepth", 0.13f}, {"multiplyTone", 0.7f}, {"distortionBias", 0.0f}, {"distortionMode", 0.0f}, {"grainShape", 0.0f}, {"distortionAsym", 0.5f}, {"multiplyFeedback", 0.0f}, {"multiplyLfoShape", 0.0f}, {"multiplyTempoSync", 0.0f}, {"multiplyShimmer", 0.2f}, {"grainPitchQuant", 0.0f}, {"grainHarm", 0.0f}
        }},
        { "SP-950 Granular", {
            {"chainSlot0", 1.0f}, {"chainSlot1", 2.0f}, {"chainSlot2", 0.0f}, {"chainSlot3", 0.0f}, {"destroyEnabled", 1.0f}, {"granularEnabled", 1.0f}, {"multiplyEnabled", 0.0f}, {"tapeEnabled", 0.0f}, {"destroyFader", 19000.0f}, {"destroyBits", 12.0f}, {"destroyIn", 0.0f}, {"destroyOut", 0.0f}, {"destroyMix", 1.0f}, {"filterCutoff", 0.85f}, {"filterLfo", 0.0f}, {"grainCloud", 0.4f}, {"grainDrift", 0.15f}, {"grainSpace", 0.4f}, {"grainMorph", 0.5f}, {"grainSizeSync", 0.0f}, {"grainReverse", 0.0f}, {"grainMix", 0.5f}, {"grainFreeze", 0.0f}, {"chorusSpeed", 1.0f}, {"chorusMix", 0.0f}, {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.8f}, {"tapeDrive", 0.02f}, {"tapeWear", 0.0f}, {"tapeGlue", 0.5f}, {"tapeNoise", 0.0f}, {"tapeNoiseSpeed", 1.0f}, {"tapeOutput", 0.0f}, {"tapeMix", 1.0f}, {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}, {"msWidth", 1.0f}, {"distortionEnabled", 0.0f}, {"distortionDrive", 0.5f}, {"distortionTone", 0.5f}, {"multiplyDepth", 0.13f}, {"multiplyTone", 0.7f}, {"distortionBias", 0.0f}, {"distortionMode", 0.0f}, {"grainShape", 0.0f}, {"distortionAsym", 0.5f}, {"multiplyFeedback", 0.0f}, {"multiplyLfoShape", 0.0f}, {"multiplyTempoSync", 0.0f}, {"multiplyShimmer", 0.2f}, {"grainPitchQuant", 0.0f}, {"grainHarm", 0.0f}
        }},
        { "Lo-Fi Pad", {
            {"chainSlot0", 1.0f}, {"chainSlot1", 2.0f}, {"chainSlot2", 0.0f}, {"chainSlot3", 0.0f}, {"destroyEnabled", 1.0f}, {"granularEnabled", 1.0f}, {"multiplyEnabled", 0.0f}, {"tapeEnabled", 0.0f}, {"destroyFader", 26040.0f}, {"destroyBits", 12.0f}, {"destroyIn", 2.0f}, {"destroyOut", 0.0f}, {"destroyMix", 1.0f}, {"filterCutoff", 0.55f}, {"filterLfo", 0.0f}, {"grainCloud", 0.6f}, {"grainDrift", 0.2f}, {"grainSpace", 0.6f}, {"grainMorph", 0.5f}, {"grainSizeSync", 0.0f}, {"grainReverse", 0.0f}, {"grainMix", 0.6f}, {"grainFreeze", 0.0f}, {"chorusSpeed", 1.0f}, {"chorusMix", 0.0f}, {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.8f}, {"tapeDrive", 0.02f}, {"tapeWear", 0.0f}, {"tapeGlue", 0.5f}, {"tapeNoise", 0.0f}, {"tapeNoiseSpeed", 1.0f}, {"tapeOutput", 0.0f}, {"tapeMix", 1.0f}, {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}, {"msWidth", 1.0f}, {"distortionEnabled", 0.0f}, {"distortionDrive", 0.5f}, {"distortionTone", 0.5f}, {"multiplyDepth", 0.13f}, {"multiplyTone", 0.7f}, {"distortionBias", 0.0f}, {"distortionMode", 0.0f}, {"grainShape", 0.0f}, {"distortionAsym", 0.5f}, {"multiplyFeedback", 0.0f}, {"multiplyLfoShape", 0.0f}, {"multiplyTempoSync", 0.0f}, {"multiplyShimmer", 0.2f}, {"grainPitchQuant", 0.0f}, {"grainHarm", 0.0f}
        }},
        { "Digital Fog", {
            {"chainSlot0", 1.0f}, {"chainSlot1", 2.0f}, {"chainSlot2", 0.0f}, {"chainSlot3", 0.0f}, {"destroyEnabled", 1.0f}, {"granularEnabled", 1.0f}, {"multiplyEnabled", 0.0f}, {"tapeEnabled", 0.0f}, {"destroyFader", 8000.0f}, {"destroyBits", 10.0f}, {"destroyIn", 0.0f}, {"destroyOut", 0.0f}, {"destroyMix", 1.0f}, {"filterCutoff", 0.7f}, {"filterLfo", 0.0f}, {"grainCloud", 0.3f}, {"grainDrift", 0.4f}, {"grainSpace", 0.55f}, {"grainMorph", 0.52f}, {"grainSizeSync", 0.0f}, {"grainReverse", 0.0f}, {"grainMix", 0.5f}, {"grainFreeze", 0.0f}, {"chorusSpeed", 1.0f}, {"chorusMix", 0.0f}, {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.8f}, {"tapeDrive", 0.02f}, {"tapeWear", 0.0f}, {"tapeGlue", 0.5f}, {"tapeNoise", 0.0f}, {"tapeNoiseSpeed", 1.0f}, {"tapeOutput", 0.0f}, {"tapeMix", 1.0f}, {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}, {"msWidth", 1.0f}, {"distortionEnabled", 0.0f}, {"distortionDrive", 0.5f}, {"distortionTone", 0.5f}, {"multiplyDepth", 0.13f}, {"multiplyTone", 0.7f}, {"distortionBias", 0.0f}, {"distortionMode", 0.0f}, {"grainShape", 0.0f}, {"distortionAsym", 0.5f}, {"multiplyFeedback", 0.0f}, {"multiplyLfoShape", 0.0f}, {"multiplyTempoSync", 0.0f}, {"multiplyShimmer", 0.2f}, {"grainPitchQuant", 0.0f}, {"grainHarm", 0.0f}
        }},
        { "Vintage Dream", {
            {"chainSlot0", 1.0f}, {"chainSlot1", 2.0f}, {"chainSlot2", 0.0f}, {"chainSlot3", 0.0f}, {"destroyEnabled", 1.0f}, {"granularEnabled", 1.0f}, {"multiplyEnabled", 0.0f}, {"tapeEnabled", 0.0f}, {"destroyFader", 26040.0f}, {"destroyBits", 12.0f}, {"destroyIn", 0.0f}, {"destroyOut", 0.0f}, {"destroyMix", 0.6f}, {"filterCutoff", 0.65f}, {"filterLfo", 0.0f}, {"grainCloud", 0.5f}, {"grainDrift", 0.2f}, {"grainSpace", 0.6f}, {"grainMorph", 0.5f}, {"grainSizeSync", 0.0f}, {"grainReverse", 0.0f}, {"grainMix", 0.55f}, {"grainFreeze", 0.0f}, {"chorusSpeed", 1.0f}, {"chorusMix", 0.0f}, {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.8f}, {"tapeDrive", 0.02f}, {"tapeWear", 0.0f}, {"tapeGlue", 0.5f}, {"tapeNoise", 0.0f}, {"tapeNoiseSpeed", 1.0f}, {"tapeOutput", 0.0f}, {"tapeMix", 1.0f}, {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}, {"msWidth", 1.2f}
        }},
        { "Glitch Cloud", {
            {"chainSlot0", 1.0f}, {"chainSlot1", 2.0f}, {"chainSlot2", 0.0f}, {"chainSlot3", 0.0f}, {"destroyEnabled", 1.0f}, {"granularEnabled", 1.0f}, {"multiplyEnabled", 0.0f}, {"tapeEnabled", 0.0f}, {"destroyFader", 8000.0f}, {"destroyBits", 6.0f}, {"destroyIn", 8.0f}, {"destroyOut", -4.0f}, {"destroyMix", 1.0f}, {"filterCutoff", 0.9f}, {"filterLfo", 0.0f}, {"grainCloud", 0.9f}, {"grainDrift", 0.7f}, {"grainSpace", 0.2f}, {"grainMorph", 0.55f}, {"grainSizeSync", 0.0f}, {"grainReverse", 0.0f}, {"grainMix", 0.8f}, {"grainFreeze", 0.0f}, {"chorusSpeed", 1.0f}, {"chorusMix", 0.0f}, {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.8f}, {"tapeDrive", 0.02f}, {"tapeWear", 0.0f}, {"tapeGlue", 0.5f}, {"tapeNoise", 0.0f}, {"tapeNoiseSpeed", 1.0f}, {"tapeOutput", 0.0f}, {"tapeMix", 1.0f}, {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}, {"msWidth", 1.0f}, {"distortionEnabled", 0.0f}, {"distortionDrive", 0.5f}, {"distortionTone", 0.5f}, {"multiplyDepth", 0.13f}, {"multiplyTone", 0.7f}, {"distortionBias", 0.0f}, {"distortionMode", 0.0f}, {"grainShape", 0.0f}, {"distortionAsym", 0.5f}, {"multiplyFeedback", 0.0f}, {"multiplyLfoShape", 0.0f}, {"multiplyTempoSync", 0.0f}, {"multiplyShimmer", 0.2f}, {"grainPitchQuant", 0.0f}, {"grainHarm", 0.0f}
        }},
        { "Full Lo-Fi Stack", {
            {"chainSlot0", 1.0f}, {"chainSlot1", 4.0f}, {"chainSlot2", 3.0f}, {"chainSlot3", 0.0f}, {"destroyEnabled", 1.0f}, {"granularEnabled", 0.0f}, {"multiplyEnabled", 1.0f}, {"tapeEnabled", 1.0f}, {"destroyFader", 19000.0f}, {"destroyBits", 12.0f}, {"destroyIn", 2.0f}, {"destroyOut", 0.0f}, {"destroyMix", 1.0f}, {"filterCutoff", 0.6f}, {"filterLfo", 0.0f}, {"grainCloud", 0.3f}, {"grainDrift", 0.2f}, {"grainSpace", 0.3f}, {"grainMorph", 0.5f}, {"grainSizeSync", 0.0f}, {"grainReverse", 0.0f}, {"grainMix", 0.0f}, {"grainFreeze", 0.0f}, {"chorusSpeed", 0.5f}, {"chorusMix", 0.3f}, {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.6f}, {"tapeDrive", 0.2f}, {"tapeWear", 0.25f}, {"tapeGlue", 0.5f}, {"tapeNoise", 0.1f}, {"tapeNoiseSpeed", 1.0f}, {"tapeOutput", 0.0f}, {"tapeMix", 1.0f}, {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}, {"msWidth", 1.0f}, {"distortionEnabled", 0.0f}, {"distortionDrive", 0.5f}, {"distortionTone", 0.5f}, {"multiplyDepth", 0.13f}, {"multiplyTone", 0.7f}, {"distortionBias", 0.0f}, {"distortionMode", 0.0f}, {"grainShape", 0.0f}, {"distortionAsym", 0.5f}, {"multiplyFeedback", 0.0f}, {"multiplyLfoShape", 0.0f}, {"multiplyTempoSync", 0.0f}, {"multiplyShimmer", 0.2f}, {"grainPitchQuant", 0.0f}, {"grainHarm", 0.0f}
        }},
        { "Textured Shimmer", {
            {"chainSlot0", 2.0f}, {"chainSlot1", 3.0f}, {"chainSlot2", 4.0f}, {"chainSlot3", 0.0f}, {"destroyEnabled", 0.0f}, {"granularEnabled", 1.0f}, {"multiplyEnabled", 1.0f}, {"tapeEnabled", 1.0f}, {"destroyFader", 19000.0f}, {"destroyBits", 12.0f}, {"destroyIn", 0.0f}, {"destroyOut", 0.0f}, {"destroyMix", 1.0f}, {"filterCutoff", 1.0f}, {"filterLfo", 0.0f}, {"grainCloud", 0.6f}, {"grainDrift", 0.15f}, {"grainSpace", 0.6f}, {"grainMorph", 0.6f}, {"grainSizeSync", 0.0f}, {"grainReverse", 0.0f}, {"grainMix", 0.7f}, {"grainFreeze", 0.0f}, {"chorusSpeed", 0.7f}, {"chorusMix", 0.4f}, {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.7f}, {"tapeDrive", 0.1f}, {"tapeWear", 0.12f}, {"tapeGlue", 0.5f}, {"tapeNoise", 0.03f}, {"tapeNoiseSpeed", 1.0f}, {"tapeOutput", 0.0f}, {"tapeMix", 1.0f}, {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}, {"msWidth", 1.0f}, {"distortionEnabled", 0.0f}, {"distortionDrive", 0.5f}, {"distortionTone", 0.5f}, {"multiplyDepth", 0.13f}, {"multiplyTone", 0.7f}, {"distortionBias", 0.0f}, {"distortionMode", 0.0f}, {"grainShape", 0.0f}, {"distortionAsym", 0.5f}, {"multiplyFeedback", 0.0f}, {"multiplyLfoShape", 0.0f}, {"multiplyTempoSync", 0.0f}, {"multiplyShimmer", 0.2f}, {"grainPitchQuant", 0.0f}, {"grainHarm", 0.0f}
        }},
        { "Gritty Ambience", {
            {"chainSlot0", 1.0f}, {"chainSlot1", 2.0f}, {"chainSlot2", 4.0f}, {"chainSlot3", 0.0f}, {"destroyEnabled", 1.0f}, {"granularEnabled", 1.0f}, {"multiplyEnabled", 0.0f}, {"tapeEnabled", 1.0f}, {"destroyFader", 19000.0f}, {"destroyBits", 10.0f}, {"destroyIn", 0.0f}, {"destroyOut", 0.0f}, {"destroyMix", 1.0f}, {"filterCutoff", 0.7f}, {"filterLfo", 0.0f}, {"grainCloud", 0.4f}, {"grainDrift", 0.2f}, {"grainSpace", 0.5f}, {"grainMorph", 0.5f}, {"grainSizeSync", 0.0f}, {"grainReverse", 0.0f}, {"grainMix", 0.6f}, {"grainFreeze", 0.0f}, {"chorusSpeed", 1.0f}, {"chorusMix", 0.0f}, {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.8f}, {"tapeDrive", 0.12f}, {"tapeWear", 0.14f}, {"tapeGlue", 0.5f}, {"tapeNoise", 0.04f}, {"tapeNoiseSpeed", 1.0f}, {"tapeOutput", 0.0f}, {"tapeMix", 1.0f}, {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}, {"msWidth", 1.0f}, {"distortionEnabled", 0.0f}, {"distortionDrive", 0.5f}, {"distortionTone", 0.5f}, {"multiplyDepth", 0.13f}, {"multiplyTone", 0.7f}, {"distortionBias", 0.0f}, {"distortionMode", 0.0f}, {"grainShape", 0.0f}, {"distortionAsym", 0.5f}, {"multiplyFeedback", 0.0f}, {"multiplyLfoShape", 0.0f}, {"multiplyTempoSync", 0.0f}, {"multiplyShimmer", 0.2f}, {"grainPitchQuant", 0.0f}, {"grainHarm", 0.0f}
        }},
        { "Classic Sampler Stack", {
            {"chainSlot0", 1.0f}, {"chainSlot1", 3.0f}, {"chainSlot2", 4.0f}, {"chainSlot3", 0.0f}, {"destroyEnabled", 1.0f}, {"granularEnabled", 0.0f}, {"multiplyEnabled", 1.0f}, {"tapeEnabled", 1.0f}, {"destroyFader", 19000.0f}, {"destroyBits", 12.0f}, {"destroyIn", 0.0f}, {"destroyOut", 0.0f}, {"destroyMix", 1.0f}, {"filterCutoff", 0.85f}, {"filterLfo", 0.0f}, {"grainCloud", 0.3f}, {"grainDrift", 0.2f}, {"grainSpace", 0.3f}, {"grainMorph", 0.5f}, {"grainSizeSync", 0.0f}, {"grainReverse", 0.0f}, {"grainMix", 0.0f}, {"grainFreeze", 0.0f}, {"chorusSpeed", 1.0f}, {"chorusMix", 0.3f}, {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.7f}, {"tapeDrive", 0.11f}, {"tapeWear", 0.13f}, {"tapeGlue", 0.5f}, {"tapeNoise", 0.04f}, {"tapeNoiseSpeed", 1.0f}, {"tapeOutput", 0.0f}, {"tapeMix", 1.0f}, {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}, {"msWidth", 1.0f}, {"distortionEnabled", 0.0f}, {"distortionDrive", 0.5f}, {"distortionTone", 0.5f}, {"multiplyDepth", 0.13f}, {"multiplyTone", 0.7f}, {"distortionBias", 0.0f}, {"distortionMode", 0.0f}, {"grainShape", 0.0f}, {"distortionAsym", 0.5f}, {"multiplyFeedback", 0.0f}, {"multiplyLfoShape", 0.0f}, {"multiplyTempoSync", 0.0f}, {"multiplyShimmer", 0.2f}, {"grainPitchQuant", 0.0f}, {"grainHarm", 0.0f}
        }},
        { "Space Choir", {
            {"chainSlot0", 2.0f}, {"chainSlot1", 3.0f}, {"chainSlot2", 4.0f}, {"chainSlot3", 0.0f}, {"destroyEnabled", 0.0f}, {"granularEnabled", 1.0f}, {"multiplyEnabled", 1.0f}, {"tapeEnabled", 1.0f}, {"destroyFader", 19000.0f}, {"destroyBits", 12.0f}, {"destroyIn", 0.0f}, {"destroyOut", 0.0f}, {"destroyMix", 1.0f}, {"filterCutoff", 1.0f}, {"filterLfo", 0.0f}, {"grainCloud", 0.7f}, {"grainDrift", 0.1f}, {"grainSpace", 0.8f}, {"grainMorph", 0.6f}, {"grainSizeSync", 0.0f}, {"grainReverse", 0.0f}, {"grainMix", 0.75f}, {"grainFreeze", 0.0f}, {"chorusSpeed", 0.3f}, {"chorusMix", 0.6f}, {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.4f}, {"tapeDrive", 0.1f}, {"tapeWear", 0.12f}, {"tapeGlue", 0.5f}, {"tapeNoise", 0.02f}, {"tapeNoiseSpeed", 1.0f}, {"tapeOutput", 0.0f}, {"tapeMix", 1.0f}, {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}, {"msWidth", 1.0f}, {"distortionEnabled", 0.0f}, {"distortionDrive", 0.5f}, {"distortionTone", 0.5f}, {"multiplyDepth", 0.13f}, {"multiplyTone", 0.7f}, {"distortionBias", 0.0f}, {"distortionMode", 0.0f}, {"grainShape", 0.0f}, {"distortionAsym", 0.5f}, {"multiplyFeedback", 0.0f}, {"multiplyLfoShape", 0.0f}, {"multiplyTempoSync", 0.0f}, {"multiplyShimmer", 0.2f}, {"grainPitchQuant", 0.0f}, {"grainHarm", 0.0f}
        }},
        { "Destroyed Ensemble", {
            {"chainSlot0", 1.0f}, {"chainSlot1", 2.0f}, {"chainSlot2", 3.0f}, {"chainSlot3", 0.0f}, {"destroyEnabled", 1.0f}, {"granularEnabled", 1.0f}, {"multiplyEnabled", 1.0f}, {"tapeEnabled", 0.0f}, {"destroyFader", 8000.0f}, {"destroyBits", 8.0f}, {"destroyIn", 3.0f}, {"destroyOut", 0.0f}, {"destroyMix", 1.0f}, {"filterCutoff", 0.7f}, {"filterLfo", 0.0f}, {"grainCloud", 0.5f}, {"grainDrift", 0.2f}, {"grainSpace", 0.4f}, {"grainMorph", 0.5f}, {"grainSizeSync", 0.0f}, {"grainReverse", 0.0f}, {"grainMix", 0.55f}, {"grainFreeze", 0.0f}, {"chorusSpeed", 1.5f}, {"chorusMix", 0.35f}, {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.7f}, {"tapeDrive", 0.02f}, {"tapeWear", 0.0f}, {"tapeGlue", 0.5f}, {"tapeNoise", 0.0f}, {"tapeNoiseSpeed", 1.0f}, {"tapeOutput", 0.0f}, {"tapeMix", 1.0f}, {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}, {"msWidth", 1.0f}, {"distortionEnabled", 0.0f}, {"distortionDrive", 0.5f}, {"distortionTone", 0.5f}, {"multiplyDepth", 0.13f}, {"multiplyTone", 0.7f}, {"distortionBias", 0.0f}, {"distortionMode", 0.0f}, {"grainShape", 0.0f}, {"distortionAsym", 0.5f}, {"multiplyFeedback", 0.0f}, {"multiplyLfoShape", 0.0f}, {"multiplyTempoSync", 0.0f}, {"multiplyShimmer", 0.2f}, {"grainPitchQuant", 0.0f}, {"grainHarm", 0.0f}
        }},
        { "Tape Cloud Chorus", {
            {"chainSlot0", 4.0f}, {"chainSlot1", 2.0f}, {"chainSlot2", 3.0f}, {"chainSlot3", 0.0f}, {"destroyEnabled", 0.0f}, {"granularEnabled", 1.0f}, {"multiplyEnabled", 1.0f}, {"tapeEnabled", 1.0f}, {"destroyFader", 19000.0f}, {"destroyBits", 12.0f}, {"destroyIn", 0.0f}, {"destroyOut", 0.0f}, {"destroyMix", 1.0f}, {"filterCutoff", 1.0f}, {"filterLfo", 0.0f}, {"grainCloud", 0.5f}, {"grainDrift", 0.2f}, {"grainSpace", 0.6f}, {"grainMorph", 0.5f}, {"grainSizeSync", 0.0f}, {"grainReverse", 0.0f}, {"grainMix", 0.6f}, {"grainFreeze", 0.0f}, {"chorusSpeed", 0.6f}, {"chorusMix", 0.4f}, {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.7f}, {"tapeDrive", 0.15f}, {"tapeWear", 0.2f}, {"tapeGlue", 0.5f}, {"tapeNoise", 0.05f}, {"tapeNoiseSpeed", 1.0f}, {"tapeOutput", 0.0f}, {"tapeMix", 1.0f}, {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}, {"msWidth", 1.0f}, {"distortionEnabled", 0.0f}, {"distortionDrive", 0.5f}, {"distortionTone", 0.5f}, {"multiplyDepth", 0.13f}, {"multiplyTone", 0.7f}, {"distortionBias", 0.0f}, {"distortionMode", 0.0f}, {"grainShape", 0.0f}, {"distortionAsym", 0.5f}, {"multiplyFeedback", 0.0f}, {"multiplyLfoShape", 0.0f}, {"multiplyTempoSync", 0.0f}, {"multiplyShimmer", 0.2f}, {"grainPitchQuant", 0.0f}, {"grainHarm", 0.0f}
        }},
        { "Atmospheric Lo-Fi", {
            {"chainSlot0", 1.0f}, {"chainSlot1", 4.0f}, {"chainSlot2", 2.0f}, {"chainSlot3", 0.0f}, {"destroyEnabled", 1.0f}, {"granularEnabled", 1.0f}, {"multiplyEnabled", 0.0f}, {"tapeEnabled", 1.0f}, {"destroyFader", 26040.0f}, {"destroyBits", 12.0f}, {"destroyIn", 2.0f}, {"destroyOut", 0.0f}, {"destroyMix", 1.0f}, {"filterCutoff", 0.5f}, {"filterLfo", 0.0f}, {"grainCloud", 0.4f}, {"grainDrift", 0.15f}, {"grainSpace", 0.5f}, {"grainMorph", 0.5f}, {"grainSizeSync", 0.0f}, {"grainReverse", 0.0f}, {"grainMix", 0.5f}, {"grainFreeze", 0.0f}, {"chorusSpeed", 1.0f}, {"chorusMix", 0.0f}, {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.8f}, {"tapeDrive", 0.2f}, {"tapeWear", 0.3f}, {"tapeGlue", 0.5f}, {"tapeNoise", 0.12f}, {"tapeNoiseSpeed", 1.0f}, {"tapeOutput", 0.0f}, {"tapeMix", 1.0f}, {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}, {"msWidth", 1.0f}, {"distortionEnabled", 0.0f}, {"distortionDrive", 0.5f}, {"distortionTone", 0.5f}, {"multiplyDepth", 0.13f}, {"multiplyTone", 0.7f}, {"distortionBias", 0.0f}, {"distortionMode", 0.0f}, {"grainShape", 0.0f}, {"distortionAsym", 0.5f}, {"multiplyFeedback", 0.0f}, {"multiplyLfoShape", 0.0f}, {"multiplyTempoSync", 0.0f}, {"multiplyShimmer", 0.2f}, {"grainPitchQuant", 0.0f}, {"grainHarm", 0.0f}
        }},
        { "Shimmer Tape Pad", {
            {"chainSlot0", 2.0f}, {"chainSlot1", 4.0f}, {"chainSlot2", 3.0f}, {"chainSlot3", 0.0f}, {"destroyEnabled", 0.0f}, {"granularEnabled", 1.0f}, {"multiplyEnabled", 1.0f}, {"tapeEnabled", 1.0f}, {"destroyFader", 19000.0f}, {"destroyBits", 12.0f}, {"destroyIn", 0.0f}, {"destroyOut", 0.0f}, {"destroyMix", 1.0f}, {"filterCutoff", 1.0f}, {"filterLfo", 0.0f}, {"grainCloud", 0.5f}, {"grainDrift", 0.08f}, {"grainSpace", 0.75f}, {"grainMorph", 0.62f}, {"grainSizeSync", 0.0f}, {"grainReverse", 0.0f}, {"grainMix", 0.7f}, {"grainFreeze", 0.0f}, {"chorusSpeed", 0.4f}, {"chorusMix", 0.45f}, {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.7f}, {"tapeDrive", 0.12f}, {"tapeWear", 0.15f}, {"tapeGlue", 0.5f}, {"tapeNoise", 0.04f}, {"tapeNoiseSpeed", 1.0f}, {"tapeOutput", 0.0f}, {"tapeMix", 1.0f}, {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}, {"msWidth", 1.0f}, {"distortionEnabled", 0.0f}, {"distortionDrive", 0.5f}, {"distortionTone", 0.5f}, {"multiplyDepth", 0.13f}, {"multiplyTone", 0.7f}, {"distortionBias", 0.0f}, {"distortionMode", 0.0f}, {"grainShape", 0.0f}, {"distortionAsym", 0.5f}, {"multiplyFeedback", 0.0f}, {"multiplyLfoShape", 0.0f}, {"multiplyTempoSync", 0.0f}, {"multiplyShimmer", 0.2f}, {"grainPitchQuant", 0.0f}, {"grainHarm", 0.0f}
        }},
        { "Broken Radio Dreams", {
            {"chainSlot0", 1.0f}, {"chainSlot1", 4.0f}, {"chainSlot2", 2.0f}, {"chainSlot3", 0.0f}, {"destroyEnabled", 1.0f}, {"granularEnabled", 1.0f}, {"multiplyEnabled", 0.0f}, {"tapeEnabled", 1.0f}, {"destroyFader", 8000.0f}, {"destroyBits", 8.0f}, {"destroyIn", 5.0f}, {"destroyOut", 0.0f}, {"destroyMix", 1.0f}, {"filterCutoff", 0.3f}, {"filterLfo", 0.2f}, {"grainCloud", 0.4f}, {"grainDrift", 0.3f}, {"grainSpace", 0.5f}, {"grainMorph", 0.5f}, {"grainSizeSync", 0.0f}, {"grainReverse", 0.0f}, {"grainMix", 0.5f}, {"grainFreeze", 0.0f}, {"chorusSpeed", 1.0f}, {"chorusMix", 0.0f}, {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.8f}, {"tapeDrive", 0.35f}, {"tapeWear", 0.48f}, {"tapeGlue", 0.35f}, {"tapeNoise", 0.3f}, {"tapeNoiseSpeed", 1.0f}, {"tapeOutput", 0.0f}, {"tapeMix", 1.0f}, {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}, {"msWidth", 1.0f}, {"distortionEnabled", 0.0f}, {"distortionDrive", 0.5f}, {"distortionTone", 0.5f}, {"multiplyDepth", 0.13f}, {"multiplyTone", 0.7f}, {"distortionBias", 0.0f}, {"distortionMode", 0.0f}, {"grainShape", 0.0f}, {"distortionAsym", 0.5f}, {"multiplyFeedback", 0.0f}, {"multiplyLfoShape", 0.0f}, {"multiplyTempoSync", 0.0f}, {"multiplyShimmer", 0.2f}, {"grainPitchQuant", 0.0f}, {"grainHarm", 0.0f}
        }},
        { "Lush Grain Ensemble", {
            {"chainSlot0", 2.0f}, {"chainSlot1", 3.0f}, {"chainSlot2", 4.0f}, {"chainSlot3", 0.0f}, {"destroyEnabled", 0.0f}, {"granularEnabled", 1.0f}, {"multiplyEnabled", 1.0f}, {"tapeEnabled", 1.0f}, {"destroyFader", 19000.0f}, {"destroyBits", 12.0f}, {"destroyIn", 0.0f}, {"destroyOut", 0.0f}, {"destroyMix", 1.0f}, {"filterCutoff", 1.0f}, {"filterLfo", 0.0f}, {"grainCloud", 0.6f}, {"grainDrift", 0.2f}, {"grainSpace", 0.7f}, {"grainMorph", 0.55f}, {"grainSizeSync", 0.0f}, {"grainReverse", 0.0f}, {"grainMix", 0.7f}, {"grainFreeze", 0.0f}, {"chorusSpeed", 0.5f}, {"chorusMix", 0.5f}, {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.5f}, {"tapeDrive", 0.1f}, {"tapeWear", 0.14f}, {"tapeGlue", 0.5f}, {"tapeNoise", 0.03f}, {"tapeNoiseSpeed", 1.0f}, {"tapeOutput", 0.0f}, {"tapeMix", 1.0f}, {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}, {"msWidth", 1.4f}
        }},
        { "Vintage Texture Stack", {
            {"chainSlot0", 1.0f}, {"chainSlot1", 2.0f}, {"chainSlot2", 4.0f}, {"chainSlot3", 0.0f}, {"destroyEnabled", 1.0f}, {"granularEnabled", 1.0f}, {"multiplyEnabled", 0.0f}, {"tapeEnabled", 1.0f}, {"destroyFader", 26040.0f}, {"destroyBits", 12.0f}, {"destroyIn", 1.0f}, {"destroyOut", 0.0f}, {"destroyMix", 1.0f}, {"filterCutoff", 0.65f}, {"filterLfo", 0.0f}, {"grainCloud", 0.35f}, {"grainDrift", 0.3f}, {"grainSpace", 0.45f}, {"grainMorph", 0.5f}, {"grainSizeSync", 0.0f}, {"grainReverse", 0.0f}, {"grainMix", 0.5f}, {"grainFreeze", 0.0f}, {"chorusSpeed", 1.0f}, {"chorusMix", 0.0f}, {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.8f}, {"tapeDrive", 0.15f}, {"tapeWear", 0.18f}, {"tapeGlue", 0.5f}, {"tapeNoise", 0.06f}, {"tapeNoiseSpeed", 1.0f}, {"tapeOutput", 0.0f}, {"tapeMix", 1.0f}, {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}, {"msWidth", 1.1f}
        }},
        { "Full Stack", {
            {"chainSlot0", 1.0f}, {"chainSlot1", 2.0f}, {"chainSlot2", 3.0f}, {"chainSlot3", 4.0f}, {"destroyEnabled", 1.0f}, {"granularEnabled", 1.0f}, {"multiplyEnabled", 1.0f}, {"tapeEnabled", 1.0f}, {"destroyFader", 19000.0f}, {"destroyBits", 12.0f}, {"destroyIn", 0.0f}, {"destroyOut", 0.0f}, {"destroyMix", 1.0f}, {"filterCutoff", 0.85f}, {"filterLfo", 0.0f}, {"grainCloud", 0.4f}, {"grainDrift", 0.15f}, {"grainSpace", 0.5f}, {"grainMorph", 0.5f}, {"grainSizeSync", 0.0f}, {"grainReverse", 0.0f}, {"grainMix", 0.5f}, {"grainFreeze", 0.0f}, {"chorusSpeed", 1.0f}, {"chorusMix", 0.3f}, {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.7f}, {"tapeDrive", 0.1f}, {"tapeWear", 0.12f}, {"tapeGlue", 0.5f}, {"tapeNoise", 0.03f}, {"tapeNoiseSpeed", 1.0f}, {"tapeOutput", 0.0f}, {"tapeMix", 1.0f}, {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}, {"msWidth", 1.0f}, {"distortionEnabled", 0.0f}, {"distortionDrive", 0.5f}, {"distortionTone", 0.5f}, {"multiplyDepth", 0.13f}, {"multiplyTone", 0.7f}, {"distortionBias", 0.0f}, {"distortionMode", 0.0f}, {"grainShape", 0.0f}, {"distortionAsym", 0.5f}, {"multiplyFeedback", 0.0f}, {"multiplyLfoShape", 0.0f}, {"multiplyTempoSync", 0.0f}, {"multiplyShimmer", 0.2f}, {"grainPitchQuant", 0.0f}, {"grainHarm", 0.0f}
        }},
        { "Lo-Fi Dream", {
            {"chainSlot0", 1.0f}, {"chainSlot1", 4.0f}, {"chainSlot2", 2.0f}, {"chainSlot3", 3.0f}, {"destroyEnabled", 1.0f}, {"granularEnabled", 1.0f}, {"multiplyEnabled", 1.0f}, {"tapeEnabled", 1.0f}, {"destroyFader", 19000.0f}, {"destroyBits", 12.0f}, {"destroyIn", 2.0f}, {"destroyOut", 0.0f}, {"destroyMix", 1.0f}, {"filterCutoff", 0.6f}, {"filterLfo", 0.0f}, {"grainCloud", 0.5f}, {"grainDrift", 0.15f}, {"grainSpace", 0.6f}, {"grainMorph", 0.5f}, {"grainSizeSync", 0.0f}, {"grainReverse", 0.0f}, {"grainMix", 0.6f}, {"grainFreeze", 0.0f}, {"chorusSpeed", 0.5f}, {"chorusMix", 0.35f}, {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.6f}, {"tapeDrive", 0.2f}, {"tapeWear", 0.25f}, {"tapeGlue", 0.5f}, {"tapeNoise", 0.1f}, {"tapeNoiseSpeed", 1.0f}, {"tapeOutput", 0.0f}, {"tapeMix", 1.0f}, {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}, {"msWidth", 1.0f}, {"distortionEnabled", 0.0f}, {"distortionDrive", 0.5f}, {"distortionTone", 0.5f}, {"multiplyDepth", 0.13f}, {"multiplyTone", 0.7f}, {"distortionBias", 0.0f}, {"distortionMode", 0.0f}, {"grainShape", 0.0f}, {"distortionAsym", 0.5f}, {"multiplyFeedback", 0.0f}, {"multiplyLfoShape", 0.0f}, {"multiplyTempoSync", 0.0f}, {"multiplyShimmer", 0.2f}, {"grainPitchQuant", 0.0f}, {"grainHarm", 0.0f}
        }},
        { "Total Texture", {
            {"chainSlot0", 1.0f}, {"chainSlot1", 2.0f}, {"chainSlot2", 4.0f}, {"chainSlot3", 3.0f}, {"destroyEnabled", 1.0f}, {"granularEnabled", 1.0f}, {"multiplyEnabled", 1.0f}, {"tapeEnabled", 1.0f}, {"destroyFader", 19000.0f}, {"destroyBits", 10.0f}, {"destroyIn", 0.0f}, {"destroyOut", 0.0f}, {"destroyMix", 1.0f}, {"filterCutoff", 0.8f}, {"filterLfo", 0.0f}, {"grainCloud", 0.4f}, {"grainDrift", 0.15f}, {"grainSpace", 0.5f}, {"grainMorph", 0.5f}, {"grainSizeSync", 0.0f}, {"grainReverse", 0.0f}, {"grainMix", 0.55f}, {"grainFreeze", 0.0f}, {"chorusSpeed", 0.8f}, {"chorusMix", 0.3f}, {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.7f}, {"tapeDrive", 0.12f}, {"tapeWear", 0.15f}, {"tapeGlue", 0.5f}, {"tapeNoise", 0.04f}, {"tapeNoiseSpeed", 1.0f}, {"tapeOutput", 0.0f}, {"tapeMix", 1.0f}, {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}, {"msWidth", 1.0f}, {"distortionEnabled", 0.0f}, {"distortionDrive", 0.5f}, {"distortionTone", 0.5f}, {"multiplyDepth", 0.13f}, {"multiplyTone", 0.7f}, {"distortionBias", 0.0f}, {"distortionMode", 0.0f}, {"grainShape", 0.0f}, {"distortionAsym", 0.5f}, {"multiplyFeedback", 0.0f}, {"multiplyLfoShape", 0.0f}, {"multiplyTempoSync", 0.0f}, {"multiplyShimmer", 0.2f}, {"grainPitchQuant", 0.0f}, {"grainHarm", 0.0f}
        }},
        { "Tape First", {
            {"chainSlot0", 4.0f}, {"chainSlot1", 1.0f}, {"chainSlot2", 2.0f}, {"chainSlot3", 3.0f}, {"destroyEnabled", 1.0f}, {"granularEnabled", 1.0f}, {"multiplyEnabled", 1.0f}, {"tapeEnabled", 1.0f}, {"destroyFader", 19000.0f}, {"destroyBits", 12.0f}, {"destroyIn", 2.0f}, {"destroyOut", 0.0f}, {"destroyMix", 1.0f}, {"filterCutoff", 0.75f}, {"filterLfo", 0.0f}, {"grainCloud", 0.4f}, {"grainDrift", 0.15f}, {"grainSpace", 0.5f}, {"grainMorph", 0.5f}, {"grainSizeSync", 0.0f}, {"grainReverse", 0.0f}, {"grainMix", 0.5f}, {"grainFreeze", 0.0f}, {"chorusSpeed", 0.7f}, {"chorusMix", 0.3f}, {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.7f}, {"tapeDrive", 0.1f}, {"tapeWear", 0.14f}, {"tapeGlue", 0.5f}, {"tapeNoise", 0.03f}, {"tapeNoiseSpeed", 1.0f}, {"tapeOutput", 0.0f}, {"tapeMix", 1.0f}, {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}, {"msWidth", 1.0f}, {"distortionEnabled", 0.0f}, {"distortionDrive", 0.5f}, {"distortionTone", 0.5f}, {"multiplyDepth", 0.13f}, {"multiplyTone", 0.7f}, {"distortionBias", 0.0f}, {"distortionMode", 0.0f}, {"grainShape", 0.0f}, {"distortionAsym", 0.5f}, {"multiplyFeedback", 0.0f}, {"multiplyLfoShape", 0.0f}, {"multiplyTempoSync", 0.0f}, {"multiplyShimmer", 0.2f}, {"grainPitchQuant", 0.0f}, {"grainHarm", 0.0f}
        }},
        { "Cinematic Grain", {
            {"chainSlot0", 2.0f}, {"chainSlot1", 4.0f}, {"chainSlot2", 1.0f}, {"chainSlot3", 3.0f}, {"destroyEnabled", 1.0f}, {"granularEnabled", 1.0f}, {"multiplyEnabled", 1.0f}, {"tapeEnabled", 1.0f}, {"destroyFader", 19000.0f}, {"destroyBits", 12.0f}, {"destroyIn", 0.0f}, {"destroyOut", 0.0f}, {"destroyMix", 0.3f}, {"filterCutoff", 0.9f}, {"filterLfo", 0.0f}, {"grainCloud", 0.4f}, {"grainDrift", 0.1f}, {"grainSpace", 0.8f}, {"grainMorph", 0.55f}, {"grainSizeSync", 0.0f}, {"grainReverse", 0.0f}, {"grainMix", 0.7f}, {"grainFreeze", 0.0f}, {"chorusSpeed", 0.3f}, {"chorusMix", 0.4f}, {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.5f}, {"tapeDrive", 0.12f}, {"tapeWear", 0.15f}, {"tapeGlue", 0.5f}, {"tapeNoise", 0.04f}, {"tapeNoiseSpeed", 1.0f}, {"tapeOutput", 0.0f}, {"tapeMix", 1.0f}, {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}, {"msWidth", 1.4f}
        }},
        { "Everything", {
            {"chainSlot0", 1.0f}, {"chainSlot1", 2.0f}, {"chainSlot2", 3.0f}, {"chainSlot3", 4.0f}, {"destroyEnabled", 1.0f}, {"granularEnabled", 1.0f}, {"multiplyEnabled", 1.0f}, {"tapeEnabled", 1.0f}, {"destroyFader", 8000.0f}, {"destroyBits", 8.0f}, {"destroyIn", 4.0f}, {"destroyOut", -2.0f}, {"destroyMix", 0.8f}, {"filterCutoff", 0.7f}, {"filterLfo", 0.0f}, {"grainCloud", 0.7f}, {"grainDrift", 0.3f}, {"grainSpace", 0.7f}, {"grainMorph", 0.6f}, {"grainSizeSync", 0.0f}, {"grainReverse", 0.0f}, {"grainMix", 0.7f}, {"grainFreeze", 0.0f}, {"chorusSpeed", 1.5f}, {"chorusMix", 0.5f}, {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.5f}, {"tapeDrive", 0.22f}, {"tapeWear", 0.28f}, {"tapeGlue", 0.6f}, {"tapeNoise", 0.1f}, {"tapeNoiseSpeed", 1.0f}, {"tapeOutput", 0.0f}, {"tapeMix", 1.0f}, {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}, {"msWidth", 1.3f}, {"distortionEnabled", 0.0f}, {"distortionDrive", 0.5f}, {"distortionTone", 0.5f}, {"multiplyDepth", 0.13f}, {"multiplyTone", 0.7f}, {"distortionBias", 0.0f}, {"distortionMode", 0.0f}, {"grainShape", 0.0f}, {"distortionAsym", 0.5f}, {"multiplyFeedback", 0.0f}, {"multiplyLfoShape", 0.0f}, {"multiplyTempoSync", 0.0f}, {"multiplyShimmer", 0.2f}, {"grainPitchQuant", 0.0f}, {"grainHarm", 0.0f}
        }},
    };
}
#endif

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new StardustProcessor();
}
