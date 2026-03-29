#include "PluginProcessor.h"
#include "PluginEditor.h"

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
        juce::NormalisableRange<float>(-12.0f, 12.0f, 0.1f), 0.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("destroyOut", 1), "Destroy Out",
        juce::NormalisableRange<float>(-12.0f, 12.0f, 0.1f), 0.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("destroyMix", 1), "Destroy Mix",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 1.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("destroyFader", 1), "Destroy Fader",
        juce::NormalisableRange<float>(0.0f, 5.0f, 0.01f), 2.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("destroyBits", 1), "Destroy Bits",
        juce::NormalisableRange<float>(4.0f, 24.0f, 0.5f), 12.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("destroyNoise", 1), "Destroy Noise",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.1f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("destroyJitter", 1), "Destroy Jitter",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("filterResonance", 1), "Filter Resonance",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("grainMix", 1), "Grain Mix",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("grainCloud", 1), "Grain Cloud",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.3f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("grainDrift", 1), "Grain Drift",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.2f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("grainSpace", 1), "Grain Space",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.3f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("grainMorph", 1), "Grain Morph",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.5f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("filterCutoff", 1), "Filter Cutoff",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 1.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("filterLfo", 1), "Filter LFO",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("chorusMix", 1), "Chorus Mix",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("chorusSpeed", 1), "Chorus Speed",
        juce::NormalisableRange<float>(0.1f, 5.0f, 0.01f, 0.5f), 1.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("multiplyPanOuter", 1), "Multiply Pan Outer",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 1.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("multiplyPanInner", 1), "Multiply Pan Inner",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.8f));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID("grainFreeze", 1), "Grain Freeze", false));
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID("grainHQ", 1), "Grain HQ", false));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID("grainSync", 1), "Grain Sync",
        juce::StringArray{"Off", "1/4", "1/8", "1/16", "1/32"}, 0));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID("grainSizeSync", 1), "Grain Size Sync",
        juce::StringArray{"Free", "1/1", "1/2", "1/4", "1/8", "1/16", "1/32",
                          "1/4.", "1/4T", "1/8.", "1/8T"}, 0));
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID("grainPitchSnap", 1), "Grain Pitch Snap", false));

    // Grain scatter (legacy — kept for preset compatibility; scatter uses grainDrift directly)
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("grainScatter", 1), "Grain Scatter",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("grainReverse", 1), "Grain Reverse",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("grainPosition", 1), "Grain Position",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 1.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("grainFeedback", 1), "Grain Feedback",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("grainVoices", 1), "Grain Voices",
        juce::NormalisableRange<float>(1.0f, 4.0f, 1.0f), 1.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("grainInterval", 1), "Grain Interval",
        juce::NormalisableRange<float>(1.0f, 24.0f, 0.5f), 7.0f));

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
    const juce::StringArray targetNames {"Off", "Cloud", "Scatter", "Reverse", "Position",
                                         "Space", "Morph", "Mix",
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
        juce::ParameterID("tapeWear", 1), "Tape Wear",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.14f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("tapeGlue", 1), "Tape Glue",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.48f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("tapeNoise", 1), "Tape Noise",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.07f));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID("tapeNoiseSpeed", 1), "Tape Noise Speed",
        juce::StringArray{"7.5 ips", "15 ips", "30 ips"}, 1));
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
        juce::ParameterID("granularEnabled", 1), "Granular Enabled", false));
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

    return { params.begin(), params.end() };
}

void StardustProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;

    // 2x oversampling (order=1 means 2^1 = 2x) for distortion + destroy
    oversampling = std::make_unique<juce::dsp::Oversampling<float>>(
        2, 1, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true);
    oversampling->initProcessing(static_cast<size_t>(samplesPerBlock));

    // Destroy DSP modules run at oversampled rate
    const double oversampledRate = sampleRate * 2.0;
    bitCrusher.prepare(oversampledRate, samplesPerBlock * 2);
    destroyDrive.prepare(oversampledRate, samplesPerBlock * 2);
    butterworthFilter.prepare(oversampledRate, samplesPerBlock * 2);

    // Granular + chorus run at original rate
    granularEngine.prepare(sampleRate, samplesPerBlock);
    chorusEngine.prepare(sampleRate, samplesPerBlock);
    tapeEngine.prepare(sampleRate, samplesPerBlock);

    // Pre-allocate with generous headroom for oversampled data
    // Use 8x to handle hosts that exceed the declared block size
    dryBuffer.setSize(2, samplesPerBlock * 8, false, true, true);
    masterDryBuffer.setSize(2, samplesPerBlock * 2, false, true, true);
    msWidthSmoothed.reset(sampleRate, 0.02);
    inputGainSmoothed.reset(sampleRate, 0.01);
    outputGainSmoothed.reset(sampleRate, 0.01);
    modMatrix.prepare(sampleRate);
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
    const float destroyNoiseVal = *apvts.getRawParameterValue("destroyNoise");
    const float destroyJitterVal = *apvts.getRawParameterValue("destroyJitter");
    const float filterResVal    = *apvts.getRawParameterValue("filterResonance");
    const float grainMixVal    = *apvts.getRawParameterValue("grainMix");
    const float grainCloud     = *apvts.getRawParameterValue("grainCloud");
    const float grainDrift     = *apvts.getRawParameterValue("grainDrift");
    const float grainSpace     = *apvts.getRawParameterValue("grainSpace");
    const float grainMorph     = *apvts.getRawParameterValue("grainMorph");

    // Update modulation matrix LFOs and routing
    for (int lfo = 0; lfo < 2; ++lfo)
    {
        const auto prefix = juce::String("modLfo") + juce::String(lfo + 1);
        modMatrix.setLFO(lfo,
            *apvts.getRawParameterValue(prefix + "Rate"),
            *apvts.getRawParameterValue(prefix + "Depth"),
            static_cast<int>(*apvts.getRawParameterValue(prefix + "Wave")),
            *apvts.getRawParameterValue(prefix + "Sync") >= 0.5f);
    }
    for (int slot = 0; slot < 4; ++slot)
    {
        const auto prefix = juce::String("modSlot") + juce::String(slot + 1);
        const int src = static_cast<int>(*apvts.getRawParameterValue(prefix + "Src")) - 1; // 0=Off→-1
        const int tgt = static_cast<int>(*apvts.getRawParameterValue(prefix + "Tgt")) - 1; // 0=Off→-1
        modMatrix.setSlot(slot, src, tgt, *apvts.getRawParameterValue(prefix + "Amt"));
    }
    // Feed envelope follower from input levels (average of L/R peak)
    modMatrix.setEnvFollower((inputLevelLeft.load(std::memory_order_relaxed)
                            + inputLevelRight.load(std::memory_order_relaxed)) * 0.5f);

    // Advance LFOs to block midpoint, then read modulated values (fix 1.1)
    const int halfBlock = numSamples / 2;
    for (int s = 0; s < halfBlock; ++s)
        modMatrix.processSample();

    // Apply modulation to macro values (modulate returns clamped 0-1)
    const float modCloud  = modMatrix.modulate(ModTarget::GrainCloud, grainCloud);
    const float modSpace  = modMatrix.modulate(ModTarget::GrainSpace, grainSpace);
    const float modMorph  = modMatrix.modulate(ModTarget::GrainMorph, grainMorph);
    const float modMix    = modMatrix.modulate(ModTarget::GrainMix, grainMixVal);

    // Modulate each parameter independently from its own APVTS value
    const float reverseRaw  = *apvts.getRawParameterValue("grainReverse");
    const float positionRaw = *apvts.getRawParameterValue("grainPosition");
    const float feedbackRaw = *apvts.getRawParameterValue("grainFeedback");
    const float modScatter  = modMatrix.modulate(ModTarget::GrainScatter, grainDrift);
    const float modReverse  = modMatrix.modulate(ModTarget::GrainReverse, reverseRaw);
    const float modPosition = modMatrix.modulate(ModTarget::GrainPosition, positionRaw);

    // Advance remaining LFO samples after reading midpoint values
    for (int s = halfBlock; s < numSamples; ++s)
        modMatrix.processSample();

    // Derive internal granular values from Amount macro (GRN Lite style)
    // Amount (Cloud) drives density (0.5→50/sec) and size (500→0.5ms) simultaneously
    const float grainDensityVal = 0.5f + modCloud * modCloud * 49.5f;
    float grainSizeVal          = 500.0f - modCloud * 499.5f;
    const float grainScatterVal = modScatter;  // Scatter knob directly controls position + pitch scatter
    const float stereoWidthVal  = 0.3f + modSpace * 0.7f;
    const float cutoffVal      = *apvts.getRawParameterValue("filterCutoff");
    const float chorusMixVal   = *apvts.getRawParameterValue("chorusMix");
    const bool destroyOn  = *apvts.getRawParameterValue("destroyEnabled") >= 0.5f;
    const bool granularOn = *apvts.getRawParameterValue("granularEnabled") >= 0.5f;
    const bool multiplyOn = *apvts.getRawParameterValue("multiplyEnabled") >= 0.5f;

    // If host exceeds our generous headroom (2x declared block size for
    // oversampling, 8x for dry buffer), skip oversampled processing rather
    // than allocating on the audio thread. Audio quality silently degrades
    // but no crash occurs.
    const bool dryBufferOk = dryBuffer.getNumSamples() >= numSamples * 4;

    // Oversampled processing for DESTROY section (SP-950)
    if (destroyOn && dryBufferOk)
    {
        juce::dsp::AudioBlock<float> block(buffer);
        auto oversampledBlock = oversampling->processSamplesUp(block);

        const auto osNumSamples = static_cast<int>(oversampledBlock.getNumSamples());
        const auto osNumChannels = static_cast<int>(oversampledBlock.getNumChannels());
        float* channelPtrs[2] = {
            oversampledBlock.getChannelPointer(0),
            osNumChannels > 1 ? oversampledBlock.getChannelPointer(1) : nullptr
        };
        juce::AudioBuffer<float> osBuffer(channelPtrs, osNumChannels, osNumSamples);

        // Save pre-destroy dry copy for wet/dry blend
            for (int ch = 0; ch < osNumChannels; ++ch)
                dryBuffer.copyFrom(ch, 0, osBuffer, ch, 0, osNumSamples);

            // Single fader → sample rate: left=4kHz (crush), right=52kHz (clean)
            static constexpr float kRpmRates[6] = { 4000.0f, 8000.0f, 19000.0f, 26040.0f, 45000.0f, 52080.0f };
            const float clampedFader = juce::jlimit(0.0f, 5.0f, destroyFaderVal);
            const int seg = juce::jlimit(0, 4, static_cast<int>(clampedFader));
            const float frac = clampedFader - static_cast<float>(seg);
            const float effectiveRate = kRpmRates[seg] + frac * (kRpmRates[seg + 1] - kRpmRates[seg]);

            // Input gain (dB) — drives signal into 12-bit converter
            destroyDrive.setInputDrive(destroyInVal);
            destroyDrive.processInput(osBuffer);

            // Bit depth + sample-rate reduction (variable bits, jitter-capable)
            bitCrusher.setBitDepth(destroyBitsVal);
            bitCrusher.setSampleRate(effectiveRate);
            bitCrusher.setJitter(destroyJitterVal);
            bitCrusher.process(osBuffer);

            // Low-pass filter (6th-order Butterworth, SP-950 style)
            const float filterLfoDepth = *apvts.getRawParameterValue("filterLfo");
            butterworthFilter.setCutoff(cutoffVal);
            butterworthFilter.setResonance(filterResVal);
            butterworthFilter.setLFO(1.5f, filterLfoDepth);
            butterworthFilter.process(osBuffer);

            // Output gain (dB)
            destroyDrive.setOutputColor(destroyOutVal);
            destroyDrive.processOutput(osBuffer);

            // Wet/dry blend (constant-power crossfade)
            if (destroyMixVal < 0.999f)
            {
                const float dryGain = std::cos(destroyMixVal * juce::MathConstants<float>::halfPi);
                const float wetGain = std::sin(destroyMixVal * juce::MathConstants<float>::halfPi);
                for (int ch = 0; ch < osNumChannels; ++ch)
                {
                    auto* wet = osBuffer.getWritePointer(ch);
                    const auto* dry = dryBuffer.getReadPointer(ch);
                    for (int i = 0; i < osNumSamples; ++i)
                        wet[i] = dry[i] * dryGain + wet[i] * wetGain;
                }
        }

        oversampling->processSamplesDown(block);

        // Noise floor injection — level controlled by destroyNoise knob (0=silent, 0.1=-60dB, 1.0=-40dB)
        if (destroyNoiseVal > 0.0f)
        {
            const float noiseLevel = destroyNoiseVal * 0.01f;
            for (int ch = 0; ch < numChannels; ++ch)
            {
                auto* data = buffer.getWritePointer(ch);
                for (int s = 0; s < numSamples; ++s)
                    data[s] += (random.nextFloat() * 2.0f - 1.0f) * noiseLevel;
            }
        }
    }

    // 2. GRANULAR section — macros derive all internal values
    if (granularOn)
    {
        // Use modulated values (mod matrix applied above)
        float pitchSemitones = (modMorph - 0.5f) * 24.0f;
        if (*apvts.getRawParameterValue("grainPitchSnap") >= 0.5f)
            pitchSemitones = std::round(pitchSemitones);
        granularEngine.setBasePitch(pitchSemitones);
        granularEngine.setCloud(modCloud);
        granularEngine.setPosition(modPosition);

        granularEngine.setFreeze(*apvts.getRawParameterValue("grainFreeze") >= 0.5f);
        granularEngine.setHQMode(*apvts.getRawParameterValue("grainHQ") >= 0.5f);

        // Tempo sync: query host transport
        {
            const int syncMode = static_cast<int>(*apvts.getRawParameterValue("grainSync"));
            double bpm = 120.0;
            double ppq = 0.0;
            if (syncMode > 0)
            {
                if (auto* playHead = getPlayHead())
                {
                    if (auto pos = playHead->getPosition())
                    {
                        bpm = pos->getBpm().orFallback(120.0);
                        ppq = pos->getPpqPosition().orFallback(0.0);
                    }
                }
            }
            granularEngine.setTempoSync(syncMode, bpm, ppq);
        }

        // BPM-synced grain size: override Amount-derived size with note division
        {
            const int sizeSyncMode = static_cast<int>(*apvts.getRawParameterValue("grainSizeSync"));
            if (sizeSyncMode > 0)
            {
                double bpm = 120.0;
                if (auto* ph = getPlayHead())
                    if (auto pos = ph->getPosition())
                        bpm = pos->getBpm().orFallback(120.0);
                // Divisions in beats (indices 0–10, backward-compatible: 0–6 unchanged)
                // Free, 1/1, 1/2, 1/4, 1/8, 1/16, 1/32, 1/4., 1/4T, 1/8., 1/8T
                static constexpr float kSizeDivBeats[] = {
                    0.0f, 4.0f, 2.0f, 1.0f, 0.5f, 0.25f, 0.125f,
                    1.5f, 2.0f/3.0f, 0.75f, 1.0f/3.0f
                };
                const float divisionMs = (kSizeDivBeats[sizeSyncMode] / (static_cast<float>(bpm) / 60.0f)) * 1000.0f;
                grainSizeVal = juce::jlimit(0.5f, 500.0f, divisionMs);
            }
        }

        // Space → texture + reverb wet (feedback now has its own knob)
        granularEngine.setFeedback(feedbackRaw);
        granularEngine.setTexture(modSpace);
        granularEngine.setSpace(modSpace);
        granularEngine.setDrift(modScatter);
        granularEngine.setReverse(modReverse);
        granularEngine.setVoices(static_cast<int>(std::round(
            *apvts.getRawParameterValue("grainVoices"))));
        granularEngine.setVoiceInterval(*apvts.getRawParameterValue("grainInterval"));

        granularEngine.setParameters(
            grainSizeVal, grainDensityVal, grainScatterVal,
            modMix, stereoWidthVal);
        granularEngine.process(buffer);
    }

    // 3. MULTIPLY section
    if (multiplyOn)
    {
        const float panOuter = *apvts.getRawParameterValue("multiplyPanOuter");
        const float panInner = *apvts.getRawParameterValue("multiplyPanInner");
        chorusEngine.setMix(chorusMixVal);
        chorusEngine.setSpeed(*apvts.getRawParameterValue("chorusSpeed"));
        chorusEngine.setPans(panOuter, panInner);
        chorusEngine.process(buffer);
    }

    // 4. TAPE section (wow + flutter + hiss)
    const bool tapeOn = *apvts.getRawParameterValue("tapeEnabled") >= 0.5f;

    // Reset state BEFORE processing when tape toggles on (fix 1.4)
    if (tapeOn != lastTapeOn)
    {
        if (tapeOn) tapeEngine.resetState();
        lastTapeOn = tapeOn;
        // Latency is always reported as max in prepareToPlay (fix 1.5)
    }

    if (tapeOn)
    {
        // Single “house” recipe — user trims with Drive / Wear / Glue / Noise / Mix / Output
        static constexpr float kWearDepth = 0.48f;
        const float driveN = modMatrix.modulate(ModTarget::TapeDrive, *apvts.getRawParameterValue("tapeDrive"));
        const float wear   = modMatrix.modulate(ModTarget::TapeWear, *apvts.getRawParameterValue("tapeWear"));
        const float mix    = modMatrix.modulate(ModTarget::TapeMix, *apvts.getRawParameterValue("tapeMix"));
        const float glue   = *apvts.getRawParameterValue("tapeGlue");
        const float noise  = *apvts.getRawParameterValue("tapeNoise");
        const float wobble = kWearDepth * wear;
        // Dynamic ratio: heavy flutter at low wear (erratic VHS character),
        // shifting toward wow dominance at high wear (smooth tape degradation).
        // At full wear the split is identical to the previous 30/70 ratio.
        const float flutterRatio = 0.50f - 0.20f * wear;  // 0.50 at wear=0, 0.30 at wear=1
        tapeEngine.setFlutter(wobble * flutterRatio);
        tapeEngine.setWow(wobble * (1.0f - flutterRatio));
        tapeEngine.setHiss(juce::jlimit(0.0f, 1.0f, noise));
        tapeEngine.setBias(juce::jlimit(0.0f, 1.0f, glue));
        tapeEngine.setDrive(juce::jlimit(0.0f, 1.0f, driveN));
        tapeEngine.setMix(mix);
        tapeEngine.setTapeOutputDb(*apvts.getRawParameterValue("tapeOutput"));
        tapeEngine.setWearTone(juce::jlimit(0.0f, 1.0f, wear));
        {
            static constexpr float kNoiseIps[] = { 7.5f, 15.0f, 30.0f };
            int noiseIx = 1;
            if (auto* pc = dynamic_cast<juce::AudioParameterChoice*>(apvts.getParameter("tapeNoiseSpeed")))
                noiseIx = juce::jlimit(0, 2, pc->getIndex());
            tapeEngine.setHissSpeedIps(kNoiseIps[noiseIx]);
        }
        tapeEngine.setSpeed(15.0f);
        tapeEngine.setFormulation(0);
        tapeEngine.setStandard(0);

        tapeEngine.process(buffer);
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

    // Output safety limiter — soft-knee tanh, transparent below 0.9 (fix 2.15)
    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto* data = buffer.getWritePointer(ch);
        for (int i = 0; i < numSamples; ++i)
        {
            const float s = data[i];
            const float absS = std::abs(s);
            if (absS > 0.9f)
            {
                const float t = (absS - 0.9f) / 0.1f;
                const float blend = std::min(1.0f, t);
                data[i] = s * (1.0f - blend) + std::tanh(s) * blend;
            }
        }
    }

    // Measure output levels
    outputLevelLeft.store(buffer.getMagnitude(0, 0, buffer.getNumSamples()));
    if (buffer.getNumChannels() > 1)
        outputLevelRight.store(buffer.getMagnitude(1, 0, buffer.getNumSamples()));
}

void StardustProcessor::setCurrentProgram(int index)
{
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
        loadingPreset.store(true);
        ignoreParamChanges.store(static_cast<int>(values.size()) * 2 + 10);
        currentPresetIndex.store(index);
    }

    for (const auto& [paramId, value] : values)
    {
        if (auto* param = apvts.getParameter(paramId))
            param->setValueNotifyingHost(param->convertTo0to1(value));
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
    }

    presetDirty.store(false);
    loadingPreset.store(false);
    presetLoadGrace.store(10); // skip dirty check for 10 frames (~330ms)
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
    auto xml = std::make_unique<juce::XmlElement>("StardustPreset");
    xml->setAttribute("name", name);
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
    if (bank.isNotEmpty())
    {
        dir = dir.getChildFile(bank);
        if (!dir.exists())
            dir.createDirectory();
    }

    auto file = dir.getChildFile(name + ".xml");
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
        Preset p = { fp.name, fp.values, true };
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
        file.copyFileTo(target.getChildFile(file.getFileName()));

    refreshPresets();
}

void StardustProcessor::deleteUserBank(const juce::String& bankName)
{
    if (bankName.isEmpty())
        return;
    auto dir = getUserPresetsDir().getChildFile(bankName);
    if (dir.isDirectory())
        dir.deleteRecursively();
    refreshPresets();
}

void StardustProcessor::renameUserBank(const juce::String& oldName, const juce::String& newName)
{
    if (oldName.isEmpty() || newName.isEmpty() || oldName == newName)
        return;
    auto dir = getUserPresetsDir();
    auto oldDir = dir.getChildFile(oldName);
    auto newDir = dir.getChildFile(newName);
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
    factoryPresets = {
        // ======== INIT (1) ========
        { "Init", {
            {"destroyFader", 2.0f}, {"destroyIn", 0.0f}, {"destroyOut", 0.0f}, {"destroyMix", 1.0f},
            {"filterLfo", 0.0f}, {"filterCutoff", 1.0f},
            {"grainMix", 0.0f}, {"grainCloud", 0.3f}, {"grainDrift", 0.2f}, {"grainSpace", 0.3f}, {"grainMorph", 0.5f},
            {"grainFreeze", 0.0f}, {"chorusSpeed", 1.0f}, {"chorusMix", 0.0f},
            {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.8f},
                        {"tapeDrive", 0.17f}, {"tapeWear", 0.14f}, {"tapeGlue", 0.48f}, {"tapeNoise", 0.07f}, {"tapeNoiseSpeed", 1.0f}, {"tapeOutput", 0.0f}, {"tapeMix", 1.0f},
            {"tapeEnabled", 0.0f}, {"destroyEnabled", 0.0f}, {"granularEnabled", 0.0f}, {"multiplyEnabled", 0.0f},
            {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}, {"msWidth", 1.0f}
        }},

        // ======== DESTROY ONLY (2-13) ========
        { "SP-950 Classic", {
            {"destroyFader", 2.0f}, {"destroyIn", 0.0f}, {"destroyOut", 0.0f}, {"destroyMix", 1.0f},
            {"filterLfo", 0.0f}, {"filterCutoff", 0.85f},
            {"grainMix", 0.0f}, {"grainCloud", 0.3f}, {"grainDrift", 0.2f}, {"grainSpace", 0.3f}, {"grainMorph", 0.5f},
            {"grainFreeze", 0.0f}, {"chorusSpeed", 1.0f}, {"chorusMix", 0.0f},
            {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.8f},
                        {"tapeDrive", 0.02f}, {"tapeWear", 0.0f}, {"tapeGlue", 0.5f}, {"tapeNoise", 0.0f}, {"tapeNoiseSpeed", 1.0f}, {"tapeOutput", 0.0f}, {"tapeMix", 1.0f},
            {"tapeEnabled", 0.0f}, {"destroyEnabled", 1.0f}, {"granularEnabled", 0.0f}, {"multiplyEnabled", 0.0f},
            {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}, {"msWidth", 1.0f}
        }},
        { "SP-950 Scorched", {
            {"destroyFader", 2.0f}, {"destroyIn", 10.0f}, {"destroyOut", -4.0f}, {"destroyMix", 1.0f},
            {"filterLfo", 0.0f}, {"filterCutoff", 0.7f},
            {"grainMix", 0.0f}, {"grainCloud", 0.3f}, {"grainDrift", 0.2f}, {"grainSpace", 0.3f}, {"grainMorph", 0.5f},
            {"grainFreeze", 0.0f}, {"chorusSpeed", 1.0f}, {"chorusMix", 0.0f},
            {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.8f},
                        {"tapeDrive", 0.02f}, {"tapeWear", 0.0f}, {"tapeGlue", 0.5f}, {"tapeNoise", 0.0f}, {"tapeNoiseSpeed", 1.0f}, {"tapeOutput", 0.0f}, {"tapeMix", 1.0f},
            {"tapeEnabled", 0.0f}, {"destroyEnabled", 1.0f}, {"granularEnabled", 0.0f}, {"multiplyEnabled", 0.0f},
            {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}, {"msWidth", 1.0f}
        }},
        { "33 RPM Haze", {
            {"destroyFader", 3.0f}, {"destroyIn", 0.0f}, {"destroyOut", 0.0f}, {"destroyMix", 1.0f},
            {"filterLfo", 0.0f}, {"filterCutoff", 0.55f},
            {"grainMix", 0.0f}, {"grainCloud", 0.3f}, {"grainDrift", 0.2f}, {"grainSpace", 0.3f}, {"grainMorph", 0.5f},
            {"grainFreeze", 0.0f}, {"chorusSpeed", 1.0f}, {"chorusMix", 0.0f},
            {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.8f},
                        {"tapeDrive", 0.02f}, {"tapeWear", 0.0f}, {"tapeGlue", 0.5f}, {"tapeNoise", 0.0f}, {"tapeNoiseSpeed", 1.0f}, {"tapeOutput", 0.0f}, {"tapeMix", 1.0f},
            {"tapeEnabled", 0.0f}, {"destroyEnabled", 1.0f}, {"granularEnabled", 0.0f}, {"multiplyEnabled", 0.0f},
            {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}, {"msWidth", 1.0f}
        }},
        { "78 RPM Shellac", {
            {"destroyFader", 0.0f}, {"destroyIn", 2.0f}, {"destroyOut", 0.0f}, {"destroyMix", 1.0f},
            {"filterLfo", 0.0f}, {"filterCutoff", 0.95f},
            {"grainMix", 0.0f}, {"grainCloud", 0.3f}, {"grainDrift", 0.2f}, {"grainSpace", 0.3f}, {"grainMorph", 0.5f},
            {"grainFreeze", 0.0f}, {"chorusSpeed", 1.0f}, {"chorusMix", 0.0f},
            {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.8f},
                        {"tapeDrive", 0.02f}, {"tapeWear", 0.0f}, {"tapeGlue", 0.5f}, {"tapeNoise", 0.0f}, {"tapeNoiseSpeed", 1.0f}, {"tapeOutput", 0.0f}, {"tapeMix", 1.0f},
            {"tapeEnabled", 0.0f}, {"destroyEnabled", 1.0f}, {"granularEnabled", 0.0f}, {"multiplyEnabled", 0.0f},
            {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}, {"msWidth", 1.0f}
        }},
        { "ADC Overdrive", {
            {"destroyFader", 2.0f}, {"destroyIn", 12.0f}, {"destroyOut", -6.0f}, {"destroyMix", 1.0f},
            {"filterLfo", 0.0f}, {"filterCutoff", 0.9f},
            {"grainMix", 0.0f}, {"grainCloud", 0.3f}, {"grainDrift", 0.2f}, {"grainSpace", 0.3f}, {"grainMorph", 0.5f},
            {"grainFreeze", 0.0f}, {"chorusSpeed", 1.0f}, {"chorusMix", 0.0f},
            {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.8f},
                        {"tapeDrive", 0.02f}, {"tapeWear", 0.0f}, {"tapeGlue", 0.5f}, {"tapeNoise", 0.0f}, {"tapeNoiseSpeed", 1.0f}, {"tapeOutput", 0.0f}, {"tapeMix", 1.0f},
            {"tapeEnabled", 0.0f}, {"destroyEnabled", 1.0f}, {"granularEnabled", 0.0f}, {"multiplyEnabled", 0.0f},
            {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}, {"msWidth", 1.0f}
        }},
        { "Muffled Sampler", {
            {"destroyFader", 2.0f}, {"destroyIn", 0.0f}, {"destroyOut", 0.0f}, {"destroyMix", 1.0f},
            {"filterLfo", 0.0f}, {"filterCutoff", 0.25f},
            {"grainMix", 0.0f}, {"grainCloud", 0.3f}, {"grainDrift", 0.2f}, {"grainSpace", 0.3f}, {"grainMorph", 0.5f},
            {"grainFreeze", 0.0f}, {"chorusSpeed", 1.0f}, {"chorusMix", 0.0f},
            {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.8f},
                        {"tapeDrive", 0.02f}, {"tapeWear", 0.0f}, {"tapeGlue", 0.5f}, {"tapeNoise", 0.0f}, {"tapeNoiseSpeed", 1.0f}, {"tapeOutput", 0.0f}, {"tapeMix", 1.0f},
            {"tapeEnabled", 0.0f}, {"destroyEnabled", 1.0f}, {"granularEnabled", 0.0f}, {"multiplyEnabled", 0.0f},
            {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}, {"msWidth", 1.0f}
        }},
        { "Telephone Line", {
            {"destroyFader", 1.0f}, {"destroyIn", 4.0f}, {"destroyOut", -2.0f}, {"destroyMix", 1.0f},
            {"filterLfo", 0.0f}, {"filterCutoff", 0.2f},
            {"grainMix", 0.0f}, {"grainCloud", 0.3f}, {"grainDrift", 0.2f}, {"grainSpace", 0.3f}, {"grainMorph", 0.5f},
            {"grainFreeze", 0.0f}, {"chorusSpeed", 1.0f}, {"chorusMix", 0.0f},
            {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.8f},
                        {"tapeDrive", 0.02f}, {"tapeWear", 0.0f}, {"tapeGlue", 0.5f}, {"tapeNoise", 0.0f}, {"tapeNoiseSpeed", 1.0f}, {"tapeOutput", 0.0f}, {"tapeMix", 1.0f},
            {"tapeEnabled", 0.0f}, {"destroyEnabled", 1.0f}, {"granularEnabled", 0.0f}, {"multiplyEnabled", 0.0f},
            {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}, {"msWidth", 1.0f}
        }},
        { "Digital Grit", {
            {"destroyFader", 1.0f}, {"destroyIn", 6.0f}, {"destroyOut", -3.0f}, {"destroyMix", 1.0f},
            {"filterLfo", 0.0f}, {"filterCutoff", 0.75f},
            {"grainMix", 0.0f}, {"grainCloud", 0.3f}, {"grainDrift", 0.2f}, {"grainSpace", 0.3f}, {"grainMorph", 0.5f},
            {"grainFreeze", 0.0f}, {"chorusSpeed", 1.0f}, {"chorusMix", 0.0f},
            {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.8f},
                        {"tapeDrive", 0.02f}, {"tapeWear", 0.0f}, {"tapeGlue", 0.5f}, {"tapeNoise", 0.0f}, {"tapeNoiseSpeed", 1.0f}, {"tapeOutput", 0.0f}, {"tapeMix", 1.0f},
            {"tapeEnabled", 0.0f}, {"destroyEnabled", 1.0f}, {"granularEnabled", 0.0f}, {"multiplyEnabled", 0.0f},
            {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}, {"msWidth", 1.0f}
        }},
        { "Warm Vinyl", {
            {"destroyFader", 3.0f}, {"destroyIn", 0.0f}, {"destroyOut", 2.0f}, {"destroyMix", 0.7f},
            {"filterLfo", 0.0f}, {"filterCutoff", 0.6f},
            {"grainMix", 0.0f}, {"grainCloud", 0.3f}, {"grainDrift", 0.2f}, {"grainSpace", 0.3f}, {"grainMorph", 0.5f},
            {"grainFreeze", 0.0f}, {"chorusSpeed", 1.0f}, {"chorusMix", 0.0f},
            {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.8f},
                        {"tapeDrive", 0.02f}, {"tapeWear", 0.0f}, {"tapeGlue", 0.5f}, {"tapeNoise", 0.0f}, {"tapeNoiseSpeed", 1.0f}, {"tapeOutput", 0.0f}, {"tapeMix", 1.0f},
            {"tapeEnabled", 0.0f}, {"destroyEnabled", 1.0f}, {"granularEnabled", 0.0f}, {"multiplyEnabled", 0.0f},
            {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}, {"msWidth", 1.0f}
        }},
        { "Lo-Fi Radio", {
            {"destroyFader", 2.0f}, {"destroyIn", 3.0f}, {"destroyOut", 0.0f}, {"destroyMix", 1.0f},
            {"filterLfo", 0.15f}, {"filterCutoff", 0.4f},
            {"grainMix", 0.0f}, {"grainCloud", 0.3f}, {"grainDrift", 0.2f}, {"grainSpace", 0.3f}, {"grainMorph", 0.5f},
            {"grainFreeze", 0.0f}, {"chorusSpeed", 1.0f}, {"chorusMix", 0.0f},
            {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.8f},
                        {"tapeDrive", 0.02f}, {"tapeWear", 0.0f}, {"tapeGlue", 0.5f}, {"tapeNoise", 0.0f}, {"tapeNoiseSpeed", 1.0f}, {"tapeOutput", 0.0f}, {"tapeMix", 1.0f},
            {"tapeEnabled", 0.0f}, {"destroyEnabled", 1.0f}, {"granularEnabled", 0.0f}, {"multiplyEnabled", 0.0f},
            {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}, {"msWidth", 1.0f}
        }},
        { "8-Bit Nostalgia", {
            {"destroyFader", 0.5f}, {"destroyIn", 3.0f}, {"destroyOut", -1.0f}, {"destroyMix", 1.0f},
            {"filterLfo", 0.0f}, {"filterCutoff", 0.65f},
            {"grainMix", 0.0f}, {"grainCloud", 0.3f}, {"grainDrift", 0.2f}, {"grainSpace", 0.3f}, {"grainMorph", 0.5f},
            {"grainFreeze", 0.0f}, {"chorusSpeed", 1.0f}, {"chorusMix", 0.0f},
            {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.8f},
                        {"tapeDrive", 0.02f}, {"tapeWear", 0.0f}, {"tapeGlue", 0.5f}, {"tapeNoise", 0.0f}, {"tapeNoiseSpeed", 1.0f}, {"tapeOutput", 0.0f}, {"tapeMix", 1.0f},
            {"tapeEnabled", 0.0f}, {"destroyEnabled", 1.0f}, {"granularEnabled", 0.0f}, {"multiplyEnabled", 0.0f},
            {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}, {"msWidth", 1.0f}
        }},
        { "Subtle Color", {
            {"destroyFader", 2.0f}, {"destroyIn", 0.0f}, {"destroyOut", 0.0f}, {"destroyMix", 0.35f},
            {"filterLfo", 0.0f}, {"filterCutoff", 0.9f},
            {"grainMix", 0.0f}, {"grainCloud", 0.3f}, {"grainDrift", 0.2f}, {"grainSpace", 0.3f}, {"grainMorph", 0.5f},
            {"grainFreeze", 0.0f}, {"chorusSpeed", 1.0f}, {"chorusMix", 0.0f},
            {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.8f},
                        {"tapeDrive", 0.02f}, {"tapeWear", 0.0f}, {"tapeGlue", 0.5f}, {"tapeNoise", 0.0f}, {"tapeNoiseSpeed", 1.0f}, {"tapeOutput", 0.0f}, {"tapeMix", 1.0f},
            {"tapeEnabled", 0.0f}, {"destroyEnabled", 1.0f}, {"granularEnabled", 0.0f}, {"multiplyEnabled", 0.0f},
            {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}, {"msWidth", 1.0f}
        }},

        // ======== GRANULAR ONLY (14-25) ========
        { "Morning Fog", {
            {"destroyFader", 2.0f}, {"destroyIn", 0.0f}, {"destroyOut", 0.0f}, {"destroyMix", 1.0f},
            {"filterLfo", 0.0f}, {"filterCutoff", 1.0f},
            {"grainMix", 0.6f}, {"grainCloud", 0.2f}, {"grainDrift", 0.1f}, {"grainSpace", 0.5f}, {"grainMorph", 0.5f},
            {"grainFreeze", 0.0f}, {"chorusSpeed", 1.0f}, {"chorusMix", 0.0f},
            {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.8f},
                        {"tapeDrive", 0.02f}, {"tapeWear", 0.0f}, {"tapeGlue", 0.5f}, {"tapeNoise", 0.0f}, {"tapeNoiseSpeed", 1.0f}, {"tapeOutput", 0.0f}, {"tapeMix", 1.0f},
            {"tapeEnabled", 0.0f}, {"destroyEnabled", 0.0f}, {"granularEnabled", 1.0f}, {"multiplyEnabled", 0.0f},
            {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}, {"msWidth", 1.0f}
        }},
        { "Shimmer Pad", {
            {"destroyFader", 2.0f}, {"destroyIn", 0.0f}, {"destroyOut", 0.0f}, {"destroyMix", 1.0f},
            {"filterLfo", 0.0f}, {"filterCutoff", 1.0f},
            {"grainMix", 0.75f}, {"grainCloud", 0.8f}, {"grainDrift", 0.15f}, {"grainSpace", 0.7f}, {"grainMorph", 0.62f},
            {"grainFreeze", 0.0f}, {"chorusSpeed", 1.0f}, {"chorusMix", 0.0f},
            {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.8f},
                        {"tapeDrive", 0.02f}, {"tapeWear", 0.0f}, {"tapeGlue", 0.5f}, {"tapeNoise", 0.0f}, {"tapeNoiseSpeed", 1.0f}, {"tapeOutput", 0.0f}, {"tapeMix", 1.0f},
            {"tapeEnabled", 0.0f}, {"destroyEnabled", 0.0f}, {"granularEnabled", 1.0f}, {"multiplyEnabled", 0.0f},
            {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}, {"msWidth", 1.3f}
        }},
        { "Frozen Cathedral", {
            {"destroyFader", 2.0f}, {"destroyIn", 0.0f}, {"destroyOut", 0.0f}, {"destroyMix", 1.0f},
            {"filterLfo", 0.0f}, {"filterCutoff", 1.0f},
            {"grainMix", 1.0f}, {"grainCloud", 0.4f}, {"grainDrift", 0.05f}, {"grainSpace", 0.9f}, {"grainMorph", 0.5f},
            {"grainFreeze", 1.0f}, {"chorusSpeed", 1.0f}, {"chorusMix", 0.0f},
            {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.8f},
                        {"tapeDrive", 0.02f}, {"tapeWear", 0.0f}, {"tapeGlue", 0.5f}, {"tapeNoise", 0.0f}, {"tapeNoiseSpeed", 1.0f}, {"tapeOutput", 0.0f}, {"tapeMix", 1.0f},
            {"tapeEnabled", 0.0f}, {"destroyEnabled", 0.0f}, {"granularEnabled", 1.0f}, {"multiplyEnabled", 0.0f},
            {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}, {"msWidth", 1.5f}
        }},
        { "Scattered Glass", {
            {"destroyFader", 2.0f}, {"destroyIn", 0.0f}, {"destroyOut", 0.0f}, {"destroyMix", 1.0f},
            {"filterLfo", 0.0f}, {"filterCutoff", 1.0f},
            {"grainMix", 0.8f}, {"grainCloud", 0.9f}, {"grainDrift", 0.8f}, {"grainSpace", 0.4f}, {"grainMorph", 0.5f},
            {"grainFreeze", 0.0f}, {"chorusSpeed", 1.0f}, {"chorusMix", 0.0f},
            {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.8f},
                        {"tapeDrive", 0.02f}, {"tapeWear", 0.0f}, {"tapeGlue", 0.5f}, {"tapeNoise", 0.0f}, {"tapeNoiseSpeed", 1.0f}, {"tapeOutput", 0.0f}, {"tapeMix", 1.0f},
            {"tapeEnabled", 0.0f}, {"destroyEnabled", 0.0f}, {"granularEnabled", 1.0f}, {"multiplyEnabled", 0.0f},
            {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}, {"msWidth", 1.2f}
        }},
        { "Pitch Cloud Up", {
            {"destroyFader", 2.0f}, {"destroyIn", 0.0f}, {"destroyOut", 0.0f}, {"destroyMix", 1.0f},
            {"filterLfo", 0.0f}, {"filterCutoff", 1.0f},
            {"grainMix", 0.7f}, {"grainCloud", 0.6f}, {"grainDrift", 0.2f}, {"grainSpace", 0.5f}, {"grainMorph", 0.75f},
            {"grainFreeze", 0.0f}, {"chorusSpeed", 1.0f}, {"chorusMix", 0.0f},
            {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.8f},
                        {"tapeDrive", 0.02f}, {"tapeWear", 0.0f}, {"tapeGlue", 0.5f}, {"tapeNoise", 0.0f}, {"tapeNoiseSpeed", 1.0f}, {"tapeOutput", 0.0f}, {"tapeMix", 1.0f},
            {"tapeEnabled", 0.0f}, {"destroyEnabled", 0.0f}, {"granularEnabled", 1.0f}, {"multiplyEnabled", 0.0f},
            {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}, {"msWidth", 1.0f}
        }},
        { "Pitch Cloud Down", {
            {"destroyFader", 2.0f}, {"destroyIn", 0.0f}, {"destroyOut", 0.0f}, {"destroyMix", 1.0f},
            {"filterLfo", 0.0f}, {"filterCutoff", 1.0f},
            {"grainMix", 0.7f}, {"grainCloud", 0.6f}, {"grainDrift", 0.2f}, {"grainSpace", 0.5f}, {"grainMorph", 0.25f},
            {"grainFreeze", 0.0f}, {"chorusSpeed", 1.0f}, {"chorusMix", 0.0f},
            {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.8f},
                        {"tapeDrive", 0.02f}, {"tapeWear", 0.0f}, {"tapeGlue", 0.5f}, {"tapeNoise", 0.0f}, {"tapeNoiseSpeed", 1.0f}, {"tapeOutput", 0.0f}, {"tapeMix", 1.0f},
            {"tapeEnabled", 0.0f}, {"destroyEnabled", 0.0f}, {"granularEnabled", 1.0f}, {"multiplyEnabled", 0.0f},
            {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}, {"msWidth", 1.0f}
        }},
        { "Ambient Dust", {
            {"destroyFader", 2.0f}, {"destroyIn", 0.0f}, {"destroyOut", 0.0f}, {"destroyMix", 1.0f},
            {"filterLfo", 0.0f}, {"filterCutoff", 1.0f},
            {"grainMix", 0.5f}, {"grainCloud", 0.5f}, {"grainDrift", 0.4f}, {"grainSpace", 0.65f}, {"grainMorph", 0.52f},
            {"grainFreeze", 0.0f}, {"chorusSpeed", 1.0f}, {"chorusMix", 0.0f},
            {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.8f},
                        {"tapeDrive", 0.02f}, {"tapeWear", 0.0f}, {"tapeGlue", 0.5f}, {"tapeNoise", 0.0f}, {"tapeNoiseSpeed", 1.0f}, {"tapeOutput", 0.0f}, {"tapeMix", 1.0f},
            {"tapeEnabled", 0.0f}, {"destroyEnabled", 0.0f}, {"granularEnabled", 1.0f}, {"multiplyEnabled", 0.0f},
            {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}, {"msWidth", 1.4f}
        }},
        { "Dense Swarm", {
            {"destroyFader", 2.0f}, {"destroyIn", 0.0f}, {"destroyOut", 0.0f}, {"destroyMix", 1.0f},
            {"filterLfo", 0.0f}, {"filterCutoff", 1.0f},
            {"grainMix", 0.85f}, {"grainCloud", 1.0f}, {"grainDrift", 0.6f}, {"grainSpace", 0.3f}, {"grainMorph", 0.5f},
            {"grainFreeze", 0.0f}, {"chorusSpeed", 1.0f}, {"chorusMix", 0.0f},
            {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.8f},
                        {"tapeDrive", 0.02f}, {"tapeWear", 0.0f}, {"tapeGlue", 0.5f}, {"tapeNoise", 0.0f}, {"tapeNoiseSpeed", 1.0f}, {"tapeOutput", 0.0f}, {"tapeMix", 1.0f},
            {"tapeEnabled", 0.0f}, {"destroyEnabled", 0.0f}, {"granularEnabled", 1.0f}, {"multiplyEnabled", 0.0f},
            {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}, {"msWidth", 1.0f}
        }},
        { "Sparse Droplets", {
            {"destroyFader", 2.0f}, {"destroyIn", 0.0f}, {"destroyOut", 0.0f}, {"destroyMix", 1.0f},
            {"filterLfo", 0.0f}, {"filterCutoff", 1.0f},
            {"grainMix", 0.65f}, {"grainCloud", 0.05f}, {"grainDrift", 0.3f}, {"grainSpace", 0.6f}, {"grainMorph", 0.55f},
            {"grainFreeze", 0.0f}, {"chorusSpeed", 1.0f}, {"chorusMix", 0.0f},
            {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.8f},
                        {"tapeDrive", 0.02f}, {"tapeWear", 0.0f}, {"tapeGlue", 0.5f}, {"tapeNoise", 0.0f}, {"tapeNoiseSpeed", 1.0f}, {"tapeOutput", 0.0f}, {"tapeMix", 1.0f},
            {"tapeEnabled", 0.0f}, {"destroyEnabled", 0.0f}, {"granularEnabled", 1.0f}, {"multiplyEnabled", 0.0f},
            {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}, {"msWidth", 1.0f}
        }},
        { "Reverse Drift", {
            {"destroyFader", 2.0f}, {"destroyIn", 0.0f}, {"destroyOut", 0.0f}, {"destroyMix", 1.0f},
            {"filterLfo", 0.0f}, {"filterCutoff", 1.0f},
            {"grainMix", 0.7f}, {"grainCloud", 0.35f}, {"grainDrift", 0.9f}, {"grainSpace", 0.45f}, {"grainMorph", 0.5f},
            {"grainFreeze", 0.0f}, {"chorusSpeed", 1.0f}, {"chorusMix", 0.0f},
            {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.8f},
                        {"tapeDrive", 0.02f}, {"tapeWear", 0.0f}, {"tapeGlue", 0.5f}, {"tapeNoise", 0.0f}, {"tapeNoiseSpeed", 1.0f}, {"tapeOutput", 0.0f}, {"tapeMix", 1.0f},
            {"tapeEnabled", 0.0f}, {"destroyEnabled", 0.0f}, {"granularEnabled", 1.0f}, {"multiplyEnabled", 0.0f},
            {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}, {"msWidth", 1.0f}
        }},
        { "Octave Halo", {
            {"destroyFader", 2.0f}, {"destroyIn", 0.0f}, {"destroyOut", 0.0f}, {"destroyMix", 1.0f},
            {"filterLfo", 0.0f}, {"filterCutoff", 1.0f},
            {"grainMix", 0.8f}, {"grainCloud", 0.7f}, {"grainDrift", 0.1f}, {"grainSpace", 0.8f}, {"grainMorph", 1.0f},
            {"grainFreeze", 0.0f}, {"chorusSpeed", 1.0f}, {"chorusMix", 0.0f},
            {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.8f},
                        {"tapeDrive", 0.02f}, {"tapeWear", 0.0f}, {"tapeGlue", 0.5f}, {"tapeNoise", 0.0f}, {"tapeNoiseSpeed", 1.0f}, {"tapeOutput", 0.0f}, {"tapeMix", 1.0f},
            {"tapeEnabled", 0.0f}, {"destroyEnabled", 0.0f}, {"granularEnabled", 1.0f}, {"multiplyEnabled", 0.0f},
            {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}, {"msWidth", 1.6f}
        }},
        { "Grain Whisper", {
            {"destroyFader", 2.0f}, {"destroyIn", 0.0f}, {"destroyOut", 0.0f}, {"destroyMix", 1.0f},
            {"filterLfo", 0.0f}, {"filterCutoff", 1.0f},
            {"grainMix", 0.4f}, {"grainCloud", 0.15f}, {"grainDrift", 0.25f}, {"grainSpace", 0.35f}, {"grainMorph", 0.48f},
            {"grainFreeze", 0.0f}, {"chorusSpeed", 1.0f}, {"chorusMix", 0.0f},
            {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.8f},
                        {"tapeDrive", 0.02f}, {"tapeWear", 0.0f}, {"tapeGlue", 0.5f}, {"tapeNoise", 0.0f}, {"tapeNoiseSpeed", 1.0f}, {"tapeOutput", 0.0f}, {"tapeMix", 1.0f},
            {"tapeEnabled", 0.0f}, {"destroyEnabled", 0.0f}, {"granularEnabled", 1.0f}, {"multiplyEnabled", 0.0f},
            {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}, {"msWidth", 1.0f}
        }},

        // ======== TAPE ONLY (26-37) ========
        { "Reel To Reel", {
            {"destroyFader", 2.0f}, {"destroyIn", 0.0f}, {"destroyOut", 0.0f}, {"destroyMix", 1.0f},
            {"filterLfo", 0.0f}, {"filterCutoff", 1.0f},
            {"grainMix", 0.0f}, {"grainCloud", 0.3f}, {"grainDrift", 0.2f}, {"grainSpace", 0.3f}, {"grainMorph", 0.5f},
            {"grainFreeze", 0.0f}, {"chorusSpeed", 1.0f}, {"chorusMix", 0.0f},
            {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.8f},
                        {"tapeDrive", 0.12f}, {"tapeWear", 0.15f}, {"tapeGlue", 0.5f}, {"tapeNoise", 0.05f}, {"tapeNoiseSpeed", 1.0f}, {"tapeOutput", 0.0f}, {"tapeMix", 1.0f},
            {"tapeEnabled", 1.0f}, {"destroyEnabled", 0.0f}, {"granularEnabled", 0.0f}, {"multiplyEnabled", 0.0f},
            {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}, {"msWidth", 1.0f}
        }},
        { "Tape Warmth", {
            {"destroyFader", 2.0f}, {"destroyIn", 0.0f}, {"destroyOut", 0.0f}, {"destroyMix", 1.0f},
            {"filterLfo", 0.0f}, {"filterCutoff", 1.0f},
            {"grainMix", 0.0f}, {"grainCloud", 0.3f}, {"grainDrift", 0.2f}, {"grainSpace", 0.3f}, {"grainMorph", 0.5f},
            {"grainFreeze", 0.0f}, {"chorusSpeed", 1.0f}, {"chorusMix", 0.0f},
            {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.8f},
                        {"tapeDrive", 0.11f}, {"tapeWear", 0.13f}, {"tapeGlue", 0.55f}, {"tapeNoise", 0.04f}, {"tapeNoiseSpeed", 1.0f}, {"tapeOutput", 0.0f}, {"tapeMix", 1.0f},
            {"tapeEnabled", 1.0f}, {"destroyEnabled", 0.0f}, {"granularEnabled", 0.0f}, {"multiplyEnabled", 0.0f},
            {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}, {"msWidth", 1.0f}
        }},
        { "Wobble Machine", {
            {"destroyFader", 2.0f}, {"destroyIn", 0.0f}, {"destroyOut", 0.0f}, {"destroyMix", 1.0f},
            {"filterLfo", 0.0f}, {"filterCutoff", 1.0f},
            {"grainMix", 0.0f}, {"grainCloud", 0.3f}, {"grainDrift", 0.2f}, {"grainSpace", 0.3f}, {"grainMorph", 0.5f},
            {"grainFreeze", 0.0f}, {"chorusSpeed", 1.0f}, {"chorusMix", 0.0f},
            {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.8f},
                        {"tapeDrive", 0.27f}, {"tapeWear", 0.36f}, {"tapeGlue", 0.5f}, {"tapeNoise", 0.18f}, {"tapeNoiseSpeed", 1.0f}, {"tapeOutput", 0.0f}, {"tapeMix", 1.0f},
            {"tapeEnabled", 1.0f}, {"destroyEnabled", 0.0f}, {"granularEnabled", 0.0f}, {"multiplyEnabled", 0.0f},
            {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}, {"msWidth", 1.0f}
        }},
        { "Old Cassette", {
            {"destroyFader", 2.0f}, {"destroyIn", 0.0f}, {"destroyOut", 0.0f}, {"destroyMix", 1.0f},
            {"filterLfo", 0.0f}, {"filterCutoff", 1.0f},
            {"grainMix", 0.0f}, {"grainCloud", 0.3f}, {"grainDrift", 0.2f}, {"grainSpace", 0.3f}, {"grainMorph", 0.5f},
            {"grainFreeze", 0.0f}, {"chorusSpeed", 1.0f}, {"chorusMix", 0.0f},
            {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.8f},
                        {"tapeDrive", 0.32f}, {"tapeWear", 0.43f}, {"tapeGlue", 0.4f}, {"tapeNoise", 0.25f}, {"tapeNoiseSpeed", 1.0f}, {"tapeOutput", 0.0f}, {"tapeMix", 1.0f},
            {"tapeEnabled", 1.0f}, {"destroyEnabled", 0.0f}, {"granularEnabled", 0.0f}, {"multiplyEnabled", 0.0f},
            {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}, {"msWidth", 1.0f}
        }},
        { "Tape Saturation", {
            {"destroyFader", 2.0f}, {"destroyIn", 0.0f}, {"destroyOut", 0.0f}, {"destroyMix", 1.0f},
            {"filterLfo", 0.0f}, {"filterCutoff", 1.0f},
            {"grainMix", 0.0f}, {"grainCloud", 0.3f}, {"grainDrift", 0.2f}, {"grainSpace", 0.3f}, {"grainMorph", 0.5f},
            {"grainFreeze", 0.0f}, {"chorusSpeed", 1.0f}, {"chorusMix", 0.0f},
            {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.8f},
                        {"tapeDrive", 0.23f}, {"tapeWear", 0.32f}, {"tapeGlue", 0.7f}, {"tapeNoise", 0.14f}, {"tapeNoiseSpeed", 1.0f}, {"tapeOutput", 0.0f}, {"tapeMix", 1.0f},
            {"tapeEnabled", 1.0f}, {"destroyEnabled", 0.0f}, {"granularEnabled", 0.0f}, {"multiplyEnabled", 0.0f},
            {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}, {"msWidth", 1.0f}
        }},
        { "Mastering Glue", {
            {"destroyFader", 2.0f}, {"destroyIn", 0.0f}, {"destroyOut", 0.0f}, {"destroyMix", 1.0f},
            {"filterLfo", 0.0f}, {"filterCutoff", 1.0f},
            {"grainMix", 0.0f}, {"grainCloud", 0.3f}, {"grainDrift", 0.2f}, {"grainSpace", 0.3f}, {"grainMorph", 0.5f},
            {"grainFreeze", 0.0f}, {"chorusSpeed", 1.0f}, {"chorusMix", 0.0f},
            {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.8f},
                        {"tapeDrive", 0.1f}, {"tapeWear", 0.16f}, {"tapeGlue", 0.5f}, {"tapeNoise", 0.03f}, {"tapeNoiseSpeed", 1.0f}, {"tapeOutput", 0.0f}, {"tapeMix", 0.5f},
            {"tapeEnabled", 1.0f}, {"destroyEnabled", 0.0f}, {"granularEnabled", 0.0f}, {"multiplyEnabled", 0.0f},
            {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}, {"msWidth", 1.0f}
        }},
        { "30 IPS Punch", {
            {"destroyFader", 2.0f}, {"destroyIn", 0.0f}, {"destroyOut", 0.0f}, {"destroyMix", 1.0f},
            {"filterLfo", 0.0f}, {"filterCutoff", 1.0f},
            {"grainMix", 0.0f}, {"grainCloud", 0.3f}, {"grainDrift", 0.2f}, {"grainSpace", 0.3f}, {"grainMorph", 0.5f},
            {"grainFreeze", 0.0f}, {"chorusSpeed", 1.0f}, {"chorusMix", 0.0f},
            {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.8f},
                        {"tapeDrive", 0.13f}, {"tapeWear", 0.18f}, {"tapeGlue", 0.5f}, {"tapeNoise", 0.05f}, {"tapeNoiseSpeed", 1.0f}, {"tapeOutput", 0.0f}, {"tapeMix", 1.0f},
            {"tapeEnabled", 1.0f}, {"destroyEnabled", 0.0f}, {"granularEnabled", 0.0f}, {"multiplyEnabled", 0.0f},
            {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}, {"msWidth", 1.0f}
        }},
        { "Ampex Vintage", {
            {"destroyFader", 2.0f}, {"destroyIn", 0.0f}, {"destroyOut", 0.0f}, {"destroyMix", 1.0f},
            {"filterLfo", 0.0f}, {"filterCutoff", 1.0f},
            {"grainMix", 0.0f}, {"grainCloud", 0.3f}, {"grainDrift", 0.2f}, {"grainSpace", 0.3f}, {"grainMorph", 0.5f},
            {"grainFreeze", 0.0f}, {"chorusSpeed", 1.0f}, {"chorusMix", 0.0f},
            {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.8f},
                        {"tapeDrive", 0.12f}, {"tapeWear", 0.14f}, {"tapeGlue", 0.45f}, {"tapeNoise", 0.04f}, {"tapeNoiseSpeed", 1.0f}, {"tapeOutput", 0.0f}, {"tapeMix", 1.0f},
            {"tapeEnabled", 1.0f}, {"destroyEnabled", 0.0f}, {"granularEnabled", 0.0f}, {"multiplyEnabled", 0.0f},
            {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}, {"msWidth", 1.0f}
        }},
        { "SM900 Clarity", {
            {"destroyFader", 2.0f}, {"destroyIn", 0.0f}, {"destroyOut", 0.0f}, {"destroyMix", 1.0f},
            {"filterLfo", 0.0f}, {"filterCutoff", 1.0f},
            {"grainMix", 0.0f}, {"grainCloud", 0.3f}, {"grainDrift", 0.2f}, {"grainSpace", 0.3f}, {"grainMorph", 0.5f},
            {"grainFreeze", 0.0f}, {"chorusSpeed", 1.0f}, {"chorusMix", 0.0f},
            {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.8f},
                        {"tapeDrive", 0.03f}, {"tapeWear", 0.06f}, {"tapeGlue", 0.5f}, {"tapeNoise", 0.0f}, {"tapeNoiseSpeed", 1.0f}, {"tapeOutput", 0.0f}, {"tapeMix", 1.0f},
            {"tapeEnabled", 1.0f}, {"destroyEnabled", 0.0f}, {"granularEnabled", 0.0f}, {"multiplyEnabled", 0.0f},
            {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}, {"msWidth", 1.0f}
        }},
        { "IEC Broadcast", {
            {"destroyFader", 2.0f}, {"destroyIn", 0.0f}, {"destroyOut", 0.0f}, {"destroyMix", 1.0f},
            {"filterLfo", 0.0f}, {"filterCutoff", 1.0f},
            {"grainMix", 0.0f}, {"grainCloud", 0.3f}, {"grainDrift", 0.2f}, {"grainSpace", 0.3f}, {"grainMorph", 0.5f},
            {"grainFreeze", 0.0f}, {"chorusSpeed", 1.0f}, {"chorusMix", 0.0f},
            {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.8f},
                        {"tapeDrive", 0.07f}, {"tapeWear", 0.1f}, {"tapeGlue", 0.5f}, {"tapeNoise", 0.02f}, {"tapeNoiseSpeed", 1.0f}, {"tapeOutput", 0.0f}, {"tapeMix", 1.0f},
            {"tapeEnabled", 1.0f}, {"destroyEnabled", 0.0f}, {"granularEnabled", 0.0f}, {"multiplyEnabled", 0.0f},
            {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}, {"msWidth", 1.0f}
        }},
        { "Hiss Machine", {
            {"destroyFader", 2.0f}, {"destroyIn", 0.0f}, {"destroyOut", 0.0f}, {"destroyMix", 1.0f},
            {"filterLfo", 0.0f}, {"filterCutoff", 1.0f},
            {"grainMix", 0.0f}, {"grainCloud", 0.3f}, {"grainDrift", 0.2f}, {"grainSpace", 0.3f}, {"grainMorph", 0.5f},
            {"grainFreeze", 0.0f}, {"chorusSpeed", 1.0f}, {"chorusMix", 0.0f},
            {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.8f},
                        {"tapeDrive", 0.2f}, {"tapeWear", 0.26f}, {"tapeGlue", 0.35f}, {"tapeNoise", 0.11f}, {"tapeNoiseSpeed", 1.0f}, {"tapeOutput", 0.0f}, {"tapeMix", 1.0f},
            {"tapeEnabled", 1.0f}, {"destroyEnabled", 0.0f}, {"granularEnabled", 0.0f}, {"multiplyEnabled", 0.0f},
            {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}, {"msWidth", 1.0f}
        }},
        { "Tape Overdrive", {
            {"destroyFader", 2.0f}, {"destroyIn", 0.0f}, {"destroyOut", 0.0f}, {"destroyMix", 1.0f},
            {"filterLfo", 0.0f}, {"filterCutoff", 1.0f},
            {"grainMix", 0.0f}, {"grainCloud", 0.3f}, {"grainDrift", 0.2f}, {"grainSpace", 0.3f}, {"grainMorph", 0.5f},
            {"grainFreeze", 0.0f}, {"chorusSpeed", 1.0f}, {"chorusMix", 0.0f},
            {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.8f},
                        {"tapeDrive", 0.33f}, {"tapeWear", 0.44f}, {"tapeGlue", 0.8f}, {"tapeNoise", 0.26f}, {"tapeNoiseSpeed", 1.0f}, {"tapeOutput", 0.0f}, {"tapeMix", 1.0f},
            {"tapeEnabled", 1.0f}, {"destroyEnabled", 0.0f}, {"granularEnabled", 0.0f}, {"multiplyEnabled", 0.0f},
            {"inputGain", 3.0f}, {"outputGain", -3.0f}, {"masterMix", 1.0f}, {"msWidth", 1.0f}
        }},

        // ======== MULTIPLY ONLY (38-45) ========
        { "Classic Chorus", {
            {"destroyFader", 2.0f}, {"destroyIn", 0.0f}, {"destroyOut", 0.0f}, {"destroyMix", 1.0f},
            {"filterLfo", 0.0f}, {"filterCutoff", 1.0f},
            {"grainMix", 0.0f}, {"grainCloud", 0.3f}, {"grainDrift", 0.2f}, {"grainSpace", 0.3f}, {"grainMorph", 0.5f},
            {"grainFreeze", 0.0f}, {"chorusSpeed", 1.0f}, {"chorusMix", 0.5f},
            {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.8f},
                        {"tapeDrive", 0.02f}, {"tapeWear", 0.0f}, {"tapeGlue", 0.5f}, {"tapeNoise", 0.0f}, {"tapeNoiseSpeed", 1.0f}, {"tapeOutput", 0.0f}, {"tapeMix", 1.0f},
            {"tapeEnabled", 0.0f}, {"destroyEnabled", 0.0f}, {"granularEnabled", 0.0f}, {"multiplyEnabled", 1.0f},
            {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}, {"msWidth", 1.0f}
        }},
        { "Slow Ensemble", {
            {"destroyFader", 2.0f}, {"destroyIn", 0.0f}, {"destroyOut", 0.0f}, {"destroyMix", 1.0f},
            {"filterLfo", 0.0f}, {"filterCutoff", 1.0f},
            {"grainMix", 0.0f}, {"grainCloud", 0.3f}, {"grainDrift", 0.2f}, {"grainSpace", 0.3f}, {"grainMorph", 0.5f},
            {"grainFreeze", 0.0f}, {"chorusSpeed", 0.3f}, {"chorusMix", 0.6f},
            {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.6f},
                        {"tapeDrive", 0.02f}, {"tapeWear", 0.0f}, {"tapeGlue", 0.5f}, {"tapeNoise", 0.0f}, {"tapeNoiseSpeed", 1.0f}, {"tapeOutput", 0.0f}, {"tapeMix", 1.0f},
            {"tapeEnabled", 0.0f}, {"destroyEnabled", 0.0f}, {"granularEnabled", 0.0f}, {"multiplyEnabled", 1.0f},
            {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}, {"msWidth", 1.2f}
        }},
        { "Fast Shimmer", {
            {"destroyFader", 2.0f}, {"destroyIn", 0.0f}, {"destroyOut", 0.0f}, {"destroyMix", 1.0f},
            {"filterLfo", 0.0f}, {"filterCutoff", 1.0f},
            {"grainMix", 0.0f}, {"grainCloud", 0.3f}, {"grainDrift", 0.2f}, {"grainSpace", 0.3f}, {"grainMorph", 0.5f},
            {"grainFreeze", 0.0f}, {"chorusSpeed", 4.0f}, {"chorusMix", 0.4f},
            {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.8f},
                        {"tapeDrive", 0.02f}, {"tapeWear", 0.0f}, {"tapeGlue", 0.5f}, {"tapeNoise", 0.0f}, {"tapeNoiseSpeed", 1.0f}, {"tapeOutput", 0.0f}, {"tapeMix", 1.0f},
            {"tapeEnabled", 0.0f}, {"destroyEnabled", 0.0f}, {"granularEnabled", 0.0f}, {"multiplyEnabled", 1.0f},
            {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}, {"msWidth", 1.0f}
        }},
        { "Wide Stereo Wash", {
            {"destroyFader", 2.0f}, {"destroyIn", 0.0f}, {"destroyOut", 0.0f}, {"destroyMix", 1.0f},
            {"filterLfo", 0.0f}, {"filterCutoff", 1.0f},
            {"grainMix", 0.0f}, {"grainCloud", 0.3f}, {"grainDrift", 0.2f}, {"grainSpace", 0.3f}, {"grainMorph", 0.5f},
            {"grainFreeze", 0.0f}, {"chorusSpeed", 0.8f}, {"chorusMix", 0.7f},
            {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.3f},
                        {"tapeDrive", 0.02f}, {"tapeWear", 0.0f}, {"tapeGlue", 0.5f}, {"tapeNoise", 0.0f}, {"tapeNoiseSpeed", 1.0f}, {"tapeOutput", 0.0f}, {"tapeMix", 1.0f},
            {"tapeEnabled", 0.0f}, {"destroyEnabled", 0.0f}, {"granularEnabled", 0.0f}, {"multiplyEnabled", 1.0f},
            {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}, {"msWidth", 1.5f}
        }},
        { "Tight Doubles", {
            {"destroyFader", 2.0f}, {"destroyIn", 0.0f}, {"destroyOut", 0.0f}, {"destroyMix", 1.0f},
            {"filterLfo", 0.0f}, {"filterCutoff", 1.0f},
            {"grainMix", 0.0f}, {"grainCloud", 0.3f}, {"grainDrift", 0.2f}, {"grainSpace", 0.3f}, {"grainMorph", 0.5f},
            {"grainFreeze", 0.0f}, {"chorusSpeed", 1.5f}, {"chorusMix", 0.35f},
            {"multiplyPanOuter", 0.5f}, {"multiplyPanInner", 0.4f},
                        {"tapeDrive", 0.02f}, {"tapeWear", 0.0f}, {"tapeGlue", 0.5f}, {"tapeNoise", 0.0f}, {"tapeNoiseSpeed", 1.0f}, {"tapeOutput", 0.0f}, {"tapeMix", 1.0f},
            {"tapeEnabled", 0.0f}, {"destroyEnabled", 0.0f}, {"granularEnabled", 0.0f}, {"multiplyEnabled", 1.0f},
            {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}, {"msWidth", 1.0f}
        }},
        { "Vibrato Voice", {
            {"destroyFader", 2.0f}, {"destroyIn", 0.0f}, {"destroyOut", 0.0f}, {"destroyMix", 1.0f},
            {"filterLfo", 0.0f}, {"filterCutoff", 1.0f},
            {"grainMix", 0.0f}, {"grainCloud", 0.3f}, {"grainDrift", 0.2f}, {"grainSpace", 0.3f}, {"grainMorph", 0.5f},
            {"grainFreeze", 0.0f}, {"chorusSpeed", 5.0f}, {"chorusMix", 0.3f},
            {"multiplyPanOuter", 0.7f}, {"multiplyPanInner", 0.7f},
                        {"tapeDrive", 0.02f}, {"tapeWear", 0.0f}, {"tapeGlue", 0.5f}, {"tapeNoise", 0.0f}, {"tapeNoiseSpeed", 1.0f}, {"tapeOutput", 0.0f}, {"tapeMix", 1.0f},
            {"tapeEnabled", 0.0f}, {"destroyEnabled", 0.0f}, {"granularEnabled", 0.0f}, {"multiplyEnabled", 1.0f},
            {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}, {"msWidth", 1.0f}
        }},
        { "Mono Thicken", {
            {"destroyFader", 2.0f}, {"destroyIn", 0.0f}, {"destroyOut", 0.0f}, {"destroyMix", 1.0f},
            {"filterLfo", 0.0f}, {"filterCutoff", 1.0f},
            {"grainMix", 0.0f}, {"grainCloud", 0.3f}, {"grainDrift", 0.2f}, {"grainSpace", 0.3f}, {"grainMorph", 0.5f},
            {"grainFreeze", 0.0f}, {"chorusSpeed", 0.6f}, {"chorusMix", 0.45f},
            {"multiplyPanOuter", 0.2f}, {"multiplyPanInner", 0.1f},
                        {"tapeDrive", 0.02f}, {"tapeWear", 0.0f}, {"tapeGlue", 0.5f}, {"tapeNoise", 0.0f}, {"tapeNoiseSpeed", 1.0f}, {"tapeOutput", 0.0f}, {"tapeMix", 1.0f},
            {"tapeEnabled", 0.0f}, {"destroyEnabled", 0.0f}, {"granularEnabled", 0.0f}, {"multiplyEnabled", 1.0f},
            {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}, {"msWidth", 0.8f}
        }},
        { "Detuned Spread", {
            {"destroyFader", 2.0f}, {"destroyIn", 0.0f}, {"destroyOut", 0.0f}, {"destroyMix", 1.0f},
            {"filterLfo", 0.0f}, {"filterCutoff", 1.0f},
            {"grainMix", 0.0f}, {"grainCloud", 0.3f}, {"grainDrift", 0.2f}, {"grainSpace", 0.3f}, {"grainMorph", 0.5f},
            {"grainFreeze", 0.0f}, {"chorusSpeed", 0.15f}, {"chorusMix", 0.8f},
            {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 1.0f},
                        {"tapeDrive", 0.02f}, {"tapeWear", 0.0f}, {"tapeGlue", 0.5f}, {"tapeNoise", 0.0f}, {"tapeNoiseSpeed", 1.0f}, {"tapeOutput", 0.0f}, {"tapeMix", 1.0f},
            {"tapeEnabled", 0.0f}, {"destroyEnabled", 0.0f}, {"granularEnabled", 0.0f}, {"multiplyEnabled", 1.0f},
            {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}, {"msWidth", 1.8f}
        }},

        // ======== DESTROY + TAPE (46-55) ========
        { "Dusty Cassette", {
            {"destroyFader", 2.0f}, {"destroyIn", 2.0f}, {"destroyOut", 0.0f}, {"destroyMix", 0.8f},
            {"filterLfo", 0.0f}, {"filterCutoff", 0.65f},
            {"grainMix", 0.0f}, {"grainCloud", 0.3f}, {"grainDrift", 0.2f}, {"grainSpace", 0.3f}, {"grainMorph", 0.5f},
            {"grainFreeze", 0.0f}, {"chorusSpeed", 1.0f}, {"chorusMix", 0.0f},
            {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.8f},
                        {"tapeDrive", 0.23f}, {"tapeWear", 0.31f}, {"tapeGlue", 0.45f}, {"tapeNoise", 0.14f}, {"tapeNoiseSpeed", 1.0f}, {"tapeOutput", 0.0f}, {"tapeMix", 1.0f},
            {"tapeEnabled", 1.0f}, {"destroyEnabled", 1.0f}, {"granularEnabled", 0.0f}, {"multiplyEnabled", 0.0f},
            {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}, {"msWidth", 1.0f}
        }},
        { "Sampler Through Tape", {
            {"destroyFader", 2.0f}, {"destroyIn", 0.0f}, {"destroyOut", 0.0f}, {"destroyMix", 1.0f},
            {"filterLfo", 0.0f}, {"filterCutoff", 0.8f},
            {"grainMix", 0.0f}, {"grainCloud", 0.3f}, {"grainDrift", 0.2f}, {"grainSpace", 0.3f}, {"grainMorph", 0.5f},
            {"grainFreeze", 0.0f}, {"chorusSpeed", 1.0f}, {"chorusMix", 0.0f},
            {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.8f},
                        {"tapeDrive", 0.11f}, {"tapeWear", 0.13f}, {"tapeGlue", 0.5f}, {"tapeNoise", 0.04f}, {"tapeNoiseSpeed", 1.0f}, {"tapeOutput", 0.0f}, {"tapeMix", 1.0f},
            {"tapeEnabled", 1.0f}, {"destroyEnabled", 1.0f}, {"granularEnabled", 0.0f}, {"multiplyEnabled", 0.0f},
            {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}, {"msWidth", 1.0f}
        }},
        { "VHS Memories", {
            {"destroyFader", 2.0f}, {"destroyIn", 3.0f}, {"destroyOut", -1.0f}, {"destroyMix", 0.9f},
            {"filterLfo", 0.1f}, {"filterCutoff", 0.5f},
            {"grainMix", 0.0f}, {"grainCloud", 0.3f}, {"grainDrift", 0.2f}, {"grainSpace", 0.3f}, {"grainMorph", 0.5f},
            {"grainFreeze", 0.0f}, {"chorusSpeed", 1.0f}, {"chorusMix", 0.0f},
            {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.8f},
                        {"tapeDrive", 0.28f}, {"tapeWear", 0.38f}, {"tapeGlue", 0.4f}, {"tapeNoise", 0.2f}, {"tapeNoiseSpeed", 1.0f}, {"tapeOutput", 0.0f}, {"tapeMix", 1.0f},
            {"tapeEnabled", 1.0f}, {"destroyEnabled", 1.0f}, {"granularEnabled", 0.0f}, {"multiplyEnabled", 0.0f},
            {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}, {"msWidth", 1.0f}
        }},
        { "Vinyl Through Reel", {
            {"destroyFader", 3.0f}, {"destroyIn", 0.0f}, {"destroyOut", 1.0f}, {"destroyMix", 0.75f},
            {"filterLfo", 0.0f}, {"filterCutoff", 0.6f},
            {"grainMix", 0.0f}, {"grainCloud", 0.3f}, {"grainDrift", 0.2f}, {"grainSpace", 0.3f}, {"grainMorph", 0.5f},
            {"grainFreeze", 0.0f}, {"chorusSpeed", 1.0f}, {"chorusMix", 0.0f},
            {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.8f},
                        {"tapeDrive", 0.12f}, {"tapeWear", 0.14f}, {"tapeGlue", 0.5f}, {"tapeNoise", 0.04f}, {"tapeNoiseSpeed", 1.0f}, {"tapeOutput", 0.0f}, {"tapeMix", 1.0f},
            {"tapeEnabled", 1.0f}, {"destroyEnabled", 1.0f}, {"granularEnabled", 0.0f}, {"multiplyEnabled", 0.0f},
            {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}, {"msWidth", 1.0f}
        }},
        { "Broken Walkman", {
            {"destroyFader", 1.5f}, {"destroyIn", 5.0f}, {"destroyOut", -3.0f}, {"destroyMix", 1.0f},
            {"filterLfo", 0.2f}, {"filterCutoff", 0.35f},
            {"grainMix", 0.0f}, {"grainCloud", 0.3f}, {"grainDrift", 0.2f}, {"grainSpace", 0.3f}, {"grainMorph", 0.5f},
            {"grainFreeze", 0.0f}, {"chorusSpeed", 1.0f}, {"chorusMix", 0.0f},
            {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.8f},
                        {"tapeDrive", 0.46f}, {"tapeWear", 0.63f}, {"tapeGlue", 0.3f}, {"tapeNoise", 0.49f}, {"tapeNoiseSpeed", 1.0f}, {"tapeOutput", 0.0f}, {"tapeMix", 1.0f},
            {"tapeEnabled", 1.0f}, {"destroyEnabled", 1.0f}, {"granularEnabled", 0.0f}, {"multiplyEnabled", 0.0f},
            {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}, {"msWidth", 1.0f}
        }},
        { "Warm Sampler Dub", {
            {"destroyFader", 2.0f}, {"destroyIn", 0.0f}, {"destroyOut", 2.0f}, {"destroyMix", 0.6f},
            {"filterLfo", 0.0f}, {"filterCutoff", 0.7f},
            {"grainMix", 0.0f}, {"grainCloud", 0.3f}, {"grainDrift", 0.2f}, {"grainSpace", 0.3f}, {"grainMorph", 0.5f},
            {"grainFreeze", 0.0f}, {"chorusSpeed", 1.0f}, {"chorusMix", 0.0f},
            {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.8f},
                        {"tapeDrive", 0.16f}, {"tapeWear", 0.22f}, {"tapeGlue", 0.6f}, {"tapeNoise", 0.07f}, {"tapeNoiseSpeed", 1.0f}, {"tapeOutput", 0.0f}, {"tapeMix", 1.0f},
            {"tapeEnabled", 1.0f}, {"destroyEnabled", 1.0f}, {"granularEnabled", 0.0f}, {"multiplyEnabled", 0.0f},
            {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}, {"msWidth", 1.0f}
        }},
        { "Lo-Fi Beat Tape", {
            {"destroyFader", 2.0f}, {"destroyIn", 4.0f}, {"destroyOut", -2.0f}, {"destroyMix", 0.85f},
            {"filterLfo", 0.0f}, {"filterCutoff", 0.55f},
            {"grainMix", 0.0f}, {"grainCloud", 0.3f}, {"grainDrift", 0.2f}, {"grainSpace", 0.3f}, {"grainMorph", 0.5f},
            {"grainFreeze", 0.0f}, {"chorusSpeed", 1.0f}, {"chorusMix", 0.0f},
            {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.8f},
                        {"tapeDrive", 0.19f}, {"tapeWear", 0.24f}, {"tapeGlue", 0.5f}, {"tapeNoise", 0.09f}, {"tapeNoiseSpeed", 1.0f}, {"tapeOutput", 0.0f}, {"tapeMix", 1.0f},
            {"tapeEnabled", 1.0f}, {"destroyEnabled", 1.0f}, {"granularEnabled", 0.0f}, {"multiplyEnabled", 0.0f},
            {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}, {"msWidth", 1.0f}
        }},
        { "Crushed Tape Drone", {
            {"destroyFader", 0.5f}, {"destroyIn", 8.0f}, {"destroyOut", -5.0f}, {"destroyMix", 1.0f},
            {"filterLfo", 0.0f}, {"filterCutoff", 0.45f},
            {"grainMix", 0.0f}, {"grainCloud", 0.3f}, {"grainDrift", 0.2f}, {"grainSpace", 0.3f}, {"grainMorph", 0.5f},
            {"grainFreeze", 0.0f}, {"chorusSpeed", 1.0f}, {"chorusMix", 0.0f},
            {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.8f},
                        {"tapeDrive", 0.33f}, {"tapeWear", 0.44f}, {"tapeGlue", 0.65f}, {"tapeNoise", 0.26f}, {"tapeNoiseSpeed", 1.0f}, {"tapeOutput", 0.0f}, {"tapeMix", 1.0f},
            {"tapeEnabled", 1.0f}, {"destroyEnabled", 1.0f}, {"granularEnabled", 0.0f}, {"multiplyEnabled", 0.0f},
            {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}, {"msWidth", 1.0f}
        }},
        { "Subtle Degradation", {
            {"destroyFader", 2.0f}, {"destroyIn", 0.0f}, {"destroyOut", 0.0f}, {"destroyMix", 0.4f},
            {"filterLfo", 0.0f}, {"filterCutoff", 0.85f},
            {"grainMix", 0.0f}, {"grainCloud", 0.3f}, {"grainDrift", 0.2f}, {"grainSpace", 0.3f}, {"grainMorph", 0.5f},
            {"grainFreeze", 0.0f}, {"chorusSpeed", 1.0f}, {"chorusMix", 0.0f},
            {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.8f},
                        {"tapeDrive", 0.07f}, {"tapeWear", 0.12f}, {"tapeGlue", 0.5f}, {"tapeNoise", 0.02f}, {"tapeNoiseSpeed", 1.0f}, {"tapeOutput", 0.0f}, {"tapeMix", 0.6f},
            {"tapeEnabled", 1.0f}, {"destroyEnabled", 1.0f}, {"granularEnabled", 0.0f}, {"multiplyEnabled", 0.0f},
            {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}, {"msWidth", 1.0f}
        }},
        { "78 RPM Wax Cylinder", {
            {"destroyFader", 0.0f}, {"destroyIn", 2.0f}, {"destroyOut", 0.0f}, {"destroyMix", 1.0f},
            {"filterLfo", 0.05f}, {"filterCutoff", 0.3f},
            {"grainMix", 0.0f}, {"grainCloud", 0.3f}, {"grainDrift", 0.2f}, {"grainSpace", 0.3f}, {"grainMorph", 0.5f},
            {"grainFreeze", 0.0f}, {"chorusSpeed", 1.0f}, {"chorusMix", 0.0f},
            {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.8f},
                        {"tapeDrive", 0.37f}, {"tapeWear", 0.5f}, {"tapeGlue", 0.35f}, {"tapeNoise", 0.33f}, {"tapeNoiseSpeed", 1.0f}, {"tapeOutput", 0.0f}, {"tapeMix", 1.0f},
            {"tapeEnabled", 1.0f}, {"destroyEnabled", 1.0f}, {"granularEnabled", 0.0f}, {"multiplyEnabled", 0.0f},
            {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}, {"msWidth", 0.7f}
        }},

        // ======== GRANULAR + MULTIPLY (56-63) ========
        { "Shimmer Ensemble", {
            {"destroyFader", 2.0f}, {"destroyIn", 0.0f}, {"destroyOut", 0.0f}, {"destroyMix", 1.0f},
            {"filterLfo", 0.0f}, {"filterCutoff", 1.0f},
            {"grainMix", 0.6f}, {"grainCloud", 0.5f}, {"grainDrift", 0.15f}, {"grainSpace", 0.6f}, {"grainMorph", 0.6f},
            {"grainFreeze", 0.0f}, {"chorusSpeed", 0.7f}, {"chorusMix", 0.5f},
            {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.6f},
                        {"tapeDrive", 0.02f}, {"tapeWear", 0.0f}, {"tapeGlue", 0.5f}, {"tapeNoise", 0.0f}, {"tapeNoiseSpeed", 1.0f}, {"tapeOutput", 0.0f}, {"tapeMix", 1.0f},
            {"tapeEnabled", 0.0f}, {"destroyEnabled", 0.0f}, {"granularEnabled", 1.0f}, {"multiplyEnabled", 1.0f},
            {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}, {"msWidth", 1.4f}
        }},
        { "Wide Cloud Layer", {
            {"destroyFader", 2.0f}, {"destroyIn", 0.0f}, {"destroyOut", 0.0f}, {"destroyMix", 1.0f},
            {"filterLfo", 0.0f}, {"filterCutoff", 1.0f},
            {"grainMix", 0.75f}, {"grainCloud", 0.7f}, {"grainDrift", 0.3f}, {"grainSpace", 0.7f}, {"grainMorph", 0.5f},
            {"grainFreeze", 0.0f}, {"chorusSpeed", 0.4f}, {"chorusMix", 0.6f},
            {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.4f},
                        {"tapeDrive", 0.02f}, {"tapeWear", 0.0f}, {"tapeGlue", 0.5f}, {"tapeNoise", 0.0f}, {"tapeNoiseSpeed", 1.0f}, {"tapeOutput", 0.0f}, {"tapeMix", 1.0f},
            {"tapeEnabled", 0.0f}, {"destroyEnabled", 0.0f}, {"granularEnabled", 1.0f}, {"multiplyEnabled", 1.0f},
            {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}, {"msWidth", 1.7f}
        }},
        { "Frozen Chorus", {
            {"destroyFader", 2.0f}, {"destroyIn", 0.0f}, {"destroyOut", 0.0f}, {"destroyMix", 1.0f},
            {"filterLfo", 0.0f}, {"filterCutoff", 1.0f},
            {"grainMix", 1.0f}, {"grainCloud", 0.3f}, {"grainDrift", 0.05f}, {"grainSpace", 0.85f}, {"grainMorph", 0.5f},
            {"grainFreeze", 1.0f}, {"chorusSpeed", 0.5f}, {"chorusMix", 0.55f},
            {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.7f},
                        {"tapeDrive", 0.02f}, {"tapeWear", 0.0f}, {"tapeGlue", 0.5f}, {"tapeNoise", 0.0f}, {"tapeNoiseSpeed", 1.0f}, {"tapeOutput", 0.0f}, {"tapeMix", 1.0f},
            {"tapeEnabled", 0.0f}, {"destroyEnabled", 0.0f}, {"granularEnabled", 1.0f}, {"multiplyEnabled", 1.0f},
            {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}, {"msWidth", 1.5f}
        }},
        { "Scatter Voices", {
            {"destroyFader", 2.0f}, {"destroyIn", 0.0f}, {"destroyOut", 0.0f}, {"destroyMix", 1.0f},
            {"filterLfo", 0.0f}, {"filterCutoff", 1.0f},
            {"grainMix", 0.7f}, {"grainCloud", 0.85f}, {"grainDrift", 0.7f}, {"grainSpace", 0.35f}, {"grainMorph", 0.5f},
            {"grainFreeze", 0.0f}, {"chorusSpeed", 1.2f}, {"chorusMix", 0.4f},
            {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.5f},
                        {"tapeDrive", 0.02f}, {"tapeWear", 0.0f}, {"tapeGlue", 0.5f}, {"tapeNoise", 0.0f}, {"tapeNoiseSpeed", 1.0f}, {"tapeOutput", 0.0f}, {"tapeMix", 1.0f},
            {"tapeEnabled", 0.0f}, {"destroyEnabled", 0.0f}, {"granularEnabled", 1.0f}, {"multiplyEnabled", 1.0f},
            {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}, {"msWidth", 1.3f}
        }},
        { "Ambient Multiplier", {
            {"destroyFader", 2.0f}, {"destroyIn", 0.0f}, {"destroyOut", 0.0f}, {"destroyMix", 1.0f},
            {"filterLfo", 0.0f}, {"filterCutoff", 1.0f},
            {"grainMix", 0.5f}, {"grainCloud", 0.4f}, {"grainDrift", 0.2f}, {"grainSpace", 0.75f}, {"grainMorph", 0.55f},
            {"grainFreeze", 0.0f}, {"chorusSpeed", 0.2f}, {"chorusMix", 0.7f},
            {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.3f},
                        {"tapeDrive", 0.02f}, {"tapeWear", 0.0f}, {"tapeGlue", 0.5f}, {"tapeNoise", 0.0f}, {"tapeNoiseSpeed", 1.0f}, {"tapeOutput", 0.0f}, {"tapeMix", 1.0f},
            {"tapeEnabled", 0.0f}, {"destroyEnabled", 0.0f}, {"granularEnabled", 1.0f}, {"multiplyEnabled", 1.0f},
            {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}, {"msWidth", 1.6f}
        }},
        { "Pitched Swirl", {
            {"destroyFader", 2.0f}, {"destroyIn", 0.0f}, {"destroyOut", 0.0f}, {"destroyMix", 1.0f},
            {"filterLfo", 0.0f}, {"filterCutoff", 1.0f},
            {"grainMix", 0.65f}, {"grainCloud", 0.6f}, {"grainDrift", 0.25f}, {"grainSpace", 0.5f}, {"grainMorph", 0.72f},
            {"grainFreeze", 0.0f}, {"chorusSpeed", 2.5f}, {"chorusMix", 0.45f},
            {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.8f},
                        {"tapeDrive", 0.02f}, {"tapeWear", 0.0f}, {"tapeGlue", 0.5f}, {"tapeNoise", 0.0f}, {"tapeNoiseSpeed", 1.0f}, {"tapeOutput", 0.0f}, {"tapeMix", 1.0f},
            {"tapeEnabled", 0.0f}, {"destroyEnabled", 0.0f}, {"granularEnabled", 1.0f}, {"multiplyEnabled", 1.0f},
            {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}, {"msWidth", 1.2f}
        }},
        { "Grain Nebula", {
            {"destroyFader", 2.0f}, {"destroyIn", 0.0f}, {"destroyOut", 0.0f}, {"destroyMix", 1.0f},
            {"filterLfo", 0.0f}, {"filterCutoff", 1.0f},
            {"grainMix", 0.85f}, {"grainCloud", 0.95f}, {"grainDrift", 0.5f}, {"grainSpace", 0.8f}, {"grainMorph", 0.45f},
            {"grainFreeze", 0.0f}, {"chorusSpeed", 0.3f}, {"chorusMix", 0.65f},
            {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.5f},
                        {"tapeDrive", 0.02f}, {"tapeWear", 0.0f}, {"tapeGlue", 0.5f}, {"tapeNoise", 0.0f}, {"tapeNoiseSpeed", 1.0f}, {"tapeOutput", 0.0f}, {"tapeMix", 1.0f},
            {"tapeEnabled", 0.0f}, {"destroyEnabled", 0.0f}, {"granularEnabled", 1.0f}, {"multiplyEnabled", 1.0f},
            {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}, {"msWidth", 2.0f}
        }},
        { "Suboctave Bloom", {
            {"destroyFader", 2.0f}, {"destroyIn", 0.0f}, {"destroyOut", 0.0f}, {"destroyMix", 1.0f},
            {"filterLfo", 0.0f}, {"filterCutoff", 1.0f},
            {"grainMix", 0.7f}, {"grainCloud", 0.3f}, {"grainDrift", 0.1f}, {"grainSpace", 0.6f}, {"grainMorph", 0.0f},
            {"grainFreeze", 0.0f}, {"chorusSpeed", 0.6f}, {"chorusMix", 0.5f},
            {"multiplyPanOuter", 0.8f}, {"multiplyPanInner", 0.6f},
                        {"tapeDrive", 0.02f}, {"tapeWear", 0.0f}, {"tapeGlue", 0.5f}, {"tapeNoise", 0.0f}, {"tapeNoiseSpeed", 1.0f}, {"tapeOutput", 0.0f}, {"tapeMix", 1.0f},
            {"tapeEnabled", 0.0f}, {"destroyEnabled", 0.0f}, {"granularEnabled", 1.0f}, {"multiplyEnabled", 1.0f},
            {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}, {"msWidth", 1.3f}
        }},

        // ======== GRANULAR + TAPE (64-71) ========
        { "Grain Through Tape", {
            {"destroyFader", 2.0f}, {"destroyIn", 0.0f}, {"destroyOut", 0.0f}, {"destroyMix", 1.0f},
            {"filterLfo", 0.0f}, {"filterCutoff", 1.0f},
            {"grainMix", 0.6f}, {"grainCloud", 0.4f}, {"grainDrift", 0.2f}, {"grainSpace", 0.4f}, {"grainMorph", 0.5f},
            {"grainFreeze", 0.0f}, {"chorusSpeed", 1.0f}, {"chorusMix", 0.0f},
            {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.8f},
                        {"tapeDrive", 0.1f}, {"tapeWear", 0.12f}, {"tapeGlue", 0.5f}, {"tapeNoise", 0.03f}, {"tapeNoiseSpeed", 1.0f}, {"tapeOutput", 0.0f}, {"tapeMix", 1.0f},
            {"tapeEnabled", 1.0f}, {"destroyEnabled", 0.0f}, {"granularEnabled", 1.0f}, {"multiplyEnabled", 0.0f},
            {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}, {"msWidth", 1.0f}
        }},
        { "Organic Texture", {
            {"destroyFader", 2.0f}, {"destroyIn", 0.0f}, {"destroyOut", 0.0f}, {"destroyMix", 1.0f},
            {"filterLfo", 0.0f}, {"filterCutoff", 1.0f},
            {"grainMix", 0.5f}, {"grainCloud", 0.35f}, {"grainDrift", 0.35f}, {"grainSpace", 0.5f}, {"grainMorph", 0.48f},
            {"grainFreeze", 0.0f}, {"chorusSpeed", 1.0f}, {"chorusMix", 0.0f},
            {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.8f},
                        {"tapeDrive", 0.12f}, {"tapeWear", 0.14f}, {"tapeGlue", 0.55f}, {"tapeNoise", 0.04f}, {"tapeNoiseSpeed", 1.0f}, {"tapeOutput", 0.0f}, {"tapeMix", 1.0f},
            {"tapeEnabled", 1.0f}, {"destroyEnabled", 0.0f}, {"granularEnabled", 1.0f}, {"multiplyEnabled", 0.0f},
            {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}, {"msWidth", 1.2f}
        }},
        { "Frozen On Tape", {
            {"destroyFader", 2.0f}, {"destroyIn", 0.0f}, {"destroyOut", 0.0f}, {"destroyMix", 1.0f},
            {"filterLfo", 0.0f}, {"filterCutoff", 1.0f},
            {"grainMix", 1.0f}, {"grainCloud", 0.25f}, {"grainDrift", 0.05f}, {"grainSpace", 0.8f}, {"grainMorph", 0.5f},
            {"grainFreeze", 1.0f}, {"chorusSpeed", 1.0f}, {"chorusMix", 0.0f},
            {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.8f},
                        {"tapeDrive", 0.16f}, {"tapeWear", 0.2f}, {"tapeGlue", 0.5f}, {"tapeNoise", 0.07f}, {"tapeNoiseSpeed", 1.0f}, {"tapeOutput", 0.0f}, {"tapeMix", 1.0f},
            {"tapeEnabled", 1.0f}, {"destroyEnabled", 0.0f}, {"granularEnabled", 1.0f}, {"multiplyEnabled", 0.0f},
            {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}, {"msWidth", 1.0f}
        }},
        { "Warped Shimmer", {
            {"destroyFader", 2.0f}, {"destroyIn", 0.0f}, {"destroyOut", 0.0f}, {"destroyMix", 1.0f},
            {"filterLfo", 0.0f}, {"filterCutoff", 1.0f},
            {"grainMix", 0.7f}, {"grainCloud", 0.65f}, {"grainDrift", 0.15f}, {"grainSpace", 0.65f}, {"grainMorph", 0.65f},
            {"grainFreeze", 0.0f}, {"chorusSpeed", 1.0f}, {"chorusMix", 0.0f},
            {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.8f},
                        {"tapeDrive", 0.13f}, {"tapeWear", 0.18f}, {"tapeGlue", 0.5f}, {"tapeNoise", 0.05f}, {"tapeNoiseSpeed", 1.0f}, {"tapeOutput", 0.0f}, {"tapeMix", 1.0f},
            {"tapeEnabled", 1.0f}, {"destroyEnabled", 0.0f}, {"granularEnabled", 1.0f}, {"multiplyEnabled", 0.0f},
            {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}, {"msWidth", 1.3f}
        }},
        { "Tape Scatter", {
            {"destroyFader", 2.0f}, {"destroyIn", 0.0f}, {"destroyOut", 0.0f}, {"destroyMix", 1.0f},
            {"filterLfo", 0.0f}, {"filterCutoff", 1.0f},
            {"grainMix", 0.8f}, {"grainCloud", 0.8f}, {"grainDrift", 0.75f}, {"grainSpace", 0.3f}, {"grainMorph", 0.5f},
            {"grainFreeze", 0.0f}, {"chorusSpeed", 1.0f}, {"chorusMix", 0.0f},
            {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.8f},
                        {"tapeDrive", 0.12f}, {"tapeWear", 0.14f}, {"tapeGlue", 0.5f}, {"tapeNoise", 0.04f}, {"tapeNoiseSpeed", 1.0f}, {"tapeOutput", 0.0f}, {"tapeMix", 1.0f},
            {"tapeEnabled", 1.0f}, {"destroyEnabled", 0.0f}, {"granularEnabled", 1.0f}, {"multiplyEnabled", 0.0f},
            {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}, {"msWidth", 1.0f}
        }},
        { "Vintage Ambience", {
            {"destroyFader", 2.0f}, {"destroyIn", 0.0f}, {"destroyOut", 0.0f}, {"destroyMix", 1.0f},
            {"filterLfo", 0.0f}, {"filterCutoff", 1.0f},
            {"grainMix", 0.45f}, {"grainCloud", 0.3f}, {"grainDrift", 0.15f}, {"grainSpace", 0.7f}, {"grainMorph", 0.52f},
            {"grainFreeze", 0.0f}, {"chorusSpeed", 1.0f}, {"chorusMix", 0.0f},
            {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.8f},
                        {"tapeDrive", 0.12f}, {"tapeWear", 0.14f}, {"tapeGlue", 0.45f}, {"tapeNoise", 0.04f}, {"tapeNoiseSpeed", 1.0f}, {"tapeOutput", 0.0f}, {"tapeMix", 1.0f},
            {"tapeEnabled", 1.0f}, {"destroyEnabled", 0.0f}, {"granularEnabled", 1.0f}, {"multiplyEnabled", 0.0f},
            {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}, {"msWidth", 1.0f}
        }},
        { "Saturated Clouds", {
            {"destroyFader", 2.0f}, {"destroyIn", 0.0f}, {"destroyOut", 0.0f}, {"destroyMix", 1.0f},
            {"filterLfo", 0.0f}, {"filterCutoff", 1.0f},
            {"grainMix", 0.7f}, {"grainCloud", 0.55f}, {"grainDrift", 0.2f}, {"grainSpace", 0.55f}, {"grainMorph", 0.5f},
            {"grainFreeze", 0.0f}, {"chorusSpeed", 1.0f}, {"chorusMix", 0.0f},
            {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.8f},
                        {"tapeDrive", 0.2f}, {"tapeWear", 0.28f}, {"tapeGlue", 0.65f}, {"tapeNoise", 0.11f}, {"tapeNoiseSpeed", 1.0f}, {"tapeOutput", 0.0f}, {"tapeMix", 1.0f},
            {"tapeEnabled", 1.0f}, {"destroyEnabled", 0.0f}, {"granularEnabled", 1.0f}, {"multiplyEnabled", 0.0f},
            {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}, {"msWidth", 1.0f}
        }},
        { "Wobble Drone", {
            {"destroyFader", 2.0f}, {"destroyIn", 0.0f}, {"destroyOut", 0.0f}, {"destroyMix", 1.0f},
            {"filterLfo", 0.0f}, {"filterCutoff", 1.0f},
            {"grainMix", 0.9f}, {"grainCloud", 0.2f}, {"grainDrift", 0.1f}, {"grainSpace", 0.85f}, {"grainMorph", 0.35f},
            {"grainFreeze", 0.0f}, {"chorusSpeed", 1.0f}, {"chorusMix", 0.0f},
            {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.8f},
                        {"tapeDrive", 0.23f}, {"tapeWear", 0.3f}, {"tapeGlue", 0.5f}, {"tapeNoise", 0.14f}, {"tapeNoiseSpeed", 1.0f}, {"tapeOutput", 0.0f}, {"tapeMix", 1.0f},
            {"tapeEnabled", 1.0f}, {"destroyEnabled", 0.0f}, {"granularEnabled", 1.0f}, {"multiplyEnabled", 0.0f},
            {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}, {"msWidth", 1.0f}
        }},

        // ======== DESTROY + GRANULAR (72-79) ========
        { "Crushed Grains", {
            {"destroyFader", 2.0f}, {"destroyIn", 4.0f}, {"destroyOut", -2.0f}, {"destroyMix", 0.8f},
            {"filterLfo", 0.0f}, {"filterCutoff", 0.7f},
            {"grainMix", 0.65f}, {"grainCloud", 0.5f}, {"grainDrift", 0.3f}, {"grainSpace", 0.4f}, {"grainMorph", 0.5f},
            {"grainFreeze", 0.0f}, {"chorusSpeed", 1.0f}, {"chorusMix", 0.0f},
            {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.8f},
                        {"tapeDrive", 0.02f}, {"tapeWear", 0.0f}, {"tapeGlue", 0.5f}, {"tapeNoise", 0.0f}, {"tapeNoiseSpeed", 1.0f}, {"tapeOutput", 0.0f}, {"tapeMix", 1.0f},
            {"tapeEnabled", 0.0f}, {"destroyEnabled", 1.0f}, {"granularEnabled", 1.0f}, {"multiplyEnabled", 0.0f},
            {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}, {"msWidth", 1.0f}
        }},
        { "Bit-Reduced Texture", {
            {"destroyFader", 1.0f}, {"destroyIn", 6.0f}, {"destroyOut", -3.0f}, {"destroyMix", 1.0f},
            {"filterLfo", 0.0f}, {"filterCutoff", 0.6f},
            {"grainMix", 0.7f}, {"grainCloud", 0.7f}, {"grainDrift", 0.4f}, {"grainSpace", 0.5f}, {"grainMorph", 0.5f},
            {"grainFreeze", 0.0f}, {"chorusSpeed", 1.0f}, {"chorusMix", 0.0f},
            {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.8f},
                        {"tapeDrive", 0.02f}, {"tapeWear", 0.0f}, {"tapeGlue", 0.5f}, {"tapeNoise", 0.0f}, {"tapeNoiseSpeed", 1.0f}, {"tapeOutput", 0.0f}, {"tapeMix", 1.0f},
            {"tapeEnabled", 0.0f}, {"destroyEnabled", 1.0f}, {"granularEnabled", 1.0f}, {"multiplyEnabled", 0.0f},
            {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}, {"msWidth", 1.0f}
        }},
        { "Glitch Garden", {
            {"destroyFader", 0.5f}, {"destroyIn", 8.0f}, {"destroyOut", -4.0f}, {"destroyMix", 1.0f},
            {"filterLfo", 0.0f}, {"filterCutoff", 0.8f},
            {"grainMix", 0.8f}, {"grainCloud", 0.9f}, {"grainDrift", 0.85f}, {"grainSpace", 0.2f}, {"grainMorph", 0.5f},
            {"grainFreeze", 0.0f}, {"chorusSpeed", 1.0f}, {"chorusMix", 0.0f},
            {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.8f},
                        {"tapeDrive", 0.02f}, {"tapeWear", 0.0f}, {"tapeGlue", 0.5f}, {"tapeNoise", 0.0f}, {"tapeNoiseSpeed", 1.0f}, {"tapeOutput", 0.0f}, {"tapeMix", 1.0f},
            {"tapeEnabled", 0.0f}, {"destroyEnabled", 1.0f}, {"granularEnabled", 1.0f}, {"multiplyEnabled", 0.0f},
            {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}, {"msWidth", 1.0f}
        }},
        { "Sampled Shimmer", {
            {"destroyFader", 2.0f}, {"destroyIn", 0.0f}, {"destroyOut", 0.0f}, {"destroyMix", 0.7f},
            {"filterLfo", 0.0f}, {"filterCutoff", 0.8f},
            {"grainMix", 0.6f}, {"grainCloud", 0.5f}, {"grainDrift", 0.15f}, {"grainSpace", 0.6f}, {"grainMorph", 0.65f},
            {"grainFreeze", 0.0f}, {"chorusSpeed", 1.0f}, {"chorusMix", 0.0f},
            {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.8f},
                        {"tapeDrive", 0.02f}, {"tapeWear", 0.0f}, {"tapeGlue", 0.5f}, {"tapeNoise", 0.0f}, {"tapeNoiseSpeed", 1.0f}, {"tapeOutput", 0.0f}, {"tapeMix", 1.0f},
            {"tapeEnabled", 0.0f}, {"destroyEnabled", 1.0f}, {"granularEnabled", 1.0f}, {"multiplyEnabled", 0.0f},
            {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}, {"msWidth", 1.2f}
        }},
        { "Destroyed Freeze", {
            {"destroyFader", 2.0f}, {"destroyIn", 3.0f}, {"destroyOut", -1.0f}, {"destroyMix", 1.0f},
            {"filterLfo", 0.0f}, {"filterCutoff", 0.5f},
            {"grainMix", 1.0f}, {"grainCloud", 0.3f}, {"grainDrift", 0.05f}, {"grainSpace", 0.7f}, {"grainMorph", 0.5f},
            {"grainFreeze", 1.0f}, {"chorusSpeed", 1.0f}, {"chorusMix", 0.0f},
            {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.8f},
                        {"tapeDrive", 0.02f}, {"tapeWear", 0.0f}, {"tapeGlue", 0.5f}, {"tapeNoise", 0.0f}, {"tapeNoiseSpeed", 1.0f}, {"tapeOutput", 0.0f}, {"tapeMix", 1.0f},
            {"tapeEnabled", 0.0f}, {"destroyEnabled", 1.0f}, {"granularEnabled", 1.0f}, {"multiplyEnabled", 0.0f},
            {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}, {"msWidth", 1.0f}
        }},
        { "Lo-Fi Ambient", {
            {"destroyFader", 3.0f}, {"destroyIn", 0.0f}, {"destroyOut", 1.0f}, {"destroyMix", 0.55f},
            {"filterLfo", 0.0f}, {"filterCutoff", 0.55f},
            {"grainMix", 0.55f}, {"grainCloud", 0.4f}, {"grainDrift", 0.2f}, {"grainSpace", 0.65f}, {"grainMorph", 0.5f},
            {"grainFreeze", 0.0f}, {"chorusSpeed", 1.0f}, {"chorusMix", 0.0f},
            {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.8f},
                        {"tapeDrive", 0.02f}, {"tapeWear", 0.0f}, {"tapeGlue", 0.5f}, {"tapeNoise", 0.0f}, {"tapeNoiseSpeed", 1.0f}, {"tapeOutput", 0.0f}, {"tapeMix", 1.0f},
            {"tapeEnabled", 0.0f}, {"destroyEnabled", 1.0f}, {"granularEnabled", 1.0f}, {"multiplyEnabled", 0.0f},
            {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}, {"msWidth", 1.3f}
        }},
        { "Digital Decay", {
            {"destroyFader", 1.5f}, {"destroyIn", 5.0f}, {"destroyOut", -2.0f}, {"destroyMix", 0.9f},
            {"filterLfo", 0.1f}, {"filterCutoff", 0.45f},
            {"grainMix", 0.75f}, {"grainCloud", 0.6f}, {"grainDrift", 0.5f}, {"grainSpace", 0.35f}, {"grainMorph", 0.5f},
            {"grainFreeze", 0.0f}, {"chorusSpeed", 1.0f}, {"chorusMix", 0.0f},
            {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.8f},
                        {"tapeDrive", 0.02f}, {"tapeWear", 0.0f}, {"tapeGlue", 0.5f}, {"tapeNoise", 0.0f}, {"tapeNoiseSpeed", 1.0f}, {"tapeOutput", 0.0f}, {"tapeMix", 1.0f},
            {"tapeEnabled", 0.0f}, {"destroyEnabled", 1.0f}, {"granularEnabled", 1.0f}, {"multiplyEnabled", 0.0f},
            {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}, {"msWidth", 1.0f}
        }},
        { "Mangled Octaves", {
            {"destroyFader", 0.0f}, {"destroyIn", 7.0f}, {"destroyOut", -4.0f}, {"destroyMix", 1.0f},
            {"filterLfo", 0.0f}, {"filterCutoff", 0.75f},
            {"grainMix", 0.8f}, {"grainCloud", 0.45f}, {"grainDrift", 0.3f}, {"grainSpace", 0.4f}, {"grainMorph", 1.0f},
            {"grainFreeze", 0.0f}, {"chorusSpeed", 1.0f}, {"chorusMix", 0.0f},
            {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.8f},
                        {"tapeDrive", 0.02f}, {"tapeWear", 0.0f}, {"tapeGlue", 0.5f}, {"tapeNoise", 0.0f}, {"tapeNoiseSpeed", 1.0f}, {"tapeOutput", 0.0f}, {"tapeMix", 1.0f},
            {"tapeEnabled", 0.0f}, {"destroyEnabled", 1.0f}, {"granularEnabled", 1.0f}, {"multiplyEnabled", 0.0f},
            {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}, {"msWidth", 1.0f}
        }},

        // ======== FULL CHAIN (80-91) ========
        { "Gentle Warmth", {
            {"destroyFader", 2.0f}, {"destroyIn", 0.0f}, {"destroyOut", 0.0f}, {"destroyMix", 0.3f},
            {"filterLfo", 0.0f}, {"filterCutoff", 0.9f},
            {"grainMix", 0.2f}, {"grainCloud", 0.3f}, {"grainDrift", 0.1f}, {"grainSpace", 0.3f}, {"grainMorph", 0.5f},
            {"grainFreeze", 0.0f}, {"chorusSpeed", 0.8f}, {"chorusMix", 0.2f},
            {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.8f},
                        {"tapeDrive", 0.07f}, {"tapeWear", 0.12f}, {"tapeGlue", 0.5f}, {"tapeNoise", 0.02f}, {"tapeNoiseSpeed", 1.0f}, {"tapeOutput", 0.0f}, {"tapeMix", 0.5f},
            {"tapeEnabled", 1.0f}, {"destroyEnabled", 1.0f}, {"granularEnabled", 1.0f}, {"multiplyEnabled", 1.0f},
            {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}, {"msWidth", 1.1f}
        }},
        { "Stardust", {
            {"destroyFader", 2.0f}, {"destroyIn", 2.0f}, {"destroyOut", 0.0f}, {"destroyMix", 0.6f},
            {"filterLfo", 0.05f}, {"filterCutoff", 0.75f},
            {"grainMix", 0.5f}, {"grainCloud", 0.5f}, {"grainDrift", 0.25f}, {"grainSpace", 0.6f}, {"grainMorph", 0.55f},
            {"grainFreeze", 0.0f}, {"chorusSpeed", 0.6f}, {"chorusMix", 0.4f},
            {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.6f},
                        {"tapeDrive", 0.1f}, {"tapeWear", 0.12f}, {"tapeGlue", 0.5f}, {"tapeNoise", 0.03f}, {"tapeNoiseSpeed", 1.0f}, {"tapeOutput", 0.0f}, {"tapeMix", 0.8f},
            {"tapeEnabled", 1.0f}, {"destroyEnabled", 1.0f}, {"granularEnabled", 1.0f}, {"multiplyEnabled", 1.0f},
            {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}, {"msWidth", 1.3f}
        }},
        { "Cosmic Debris", {
            {"destroyFader", 1.0f}, {"destroyIn", 5.0f}, {"destroyOut", -3.0f}, {"destroyMix", 0.8f},
            {"filterLfo", 0.15f}, {"filterCutoff", 0.55f},
            {"grainMix", 0.7f}, {"grainCloud", 0.75f}, {"grainDrift", 0.6f}, {"grainSpace", 0.5f}, {"grainMorph", 0.5f},
            {"grainFreeze", 0.0f}, {"chorusSpeed", 1.5f}, {"chorusMix", 0.35f},
            {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.5f},
                        {"tapeDrive", 0.19f}, {"tapeWear", 0.24f}, {"tapeGlue", 0.45f}, {"tapeNoise", 0.09f}, {"tapeNoiseSpeed", 1.0f}, {"tapeOutput", 0.0f}, {"tapeMix", 1.0f},
            {"tapeEnabled", 1.0f}, {"destroyEnabled", 1.0f}, {"granularEnabled", 1.0f}, {"multiplyEnabled", 1.0f},
            {"inputGain", 0.0f}, {"outputGain", -2.0f}, {"masterMix", 1.0f}, {"msWidth", 1.5f}
        }},
        { "Sunken Cathedral", {
            {"destroyFader", 3.0f}, {"destroyIn", 0.0f}, {"destroyOut", 1.0f}, {"destroyMix", 0.5f},
            {"filterLfo", 0.0f}, {"filterCutoff", 0.5f},
            {"grainMix", 0.8f}, {"grainCloud", 0.35f}, {"grainDrift", 0.1f}, {"grainSpace", 0.9f}, {"grainMorph", 0.5f},
            {"grainFreeze", 0.0f}, {"chorusSpeed", 0.3f}, {"chorusMix", 0.5f},
            {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.4f},
                        {"tapeDrive", 0.09f}, {"tapeWear", 0.1f}, {"tapeGlue", 0.5f}, {"tapeNoise", 0.02f}, {"tapeNoiseSpeed", 1.0f}, {"tapeOutput", 0.0f}, {"tapeMix", 1.0f},
            {"tapeEnabled", 1.0f}, {"destroyEnabled", 1.0f}, {"granularEnabled", 1.0f}, {"multiplyEnabled", 1.0f},
            {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}, {"msWidth", 1.6f}
        }},
        { "Total Annihilation", {
            {"destroyFader", 0.0f}, {"destroyIn", 12.0f}, {"destroyOut", -6.0f}, {"destroyMix", 1.0f},
            {"filterLfo", 0.3f}, {"filterCutoff", 0.35f},
            {"grainMix", 1.0f}, {"grainCloud", 1.0f}, {"grainDrift", 1.0f}, {"grainSpace", 0.2f}, {"grainMorph", 0.5f},
            {"grainFreeze", 0.0f}, {"chorusSpeed", 4.5f}, {"chorusMix", 0.8f},
            {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 1.0f},
                        {"tapeDrive", 0.54f}, {"tapeWear", 0.75f}, {"tapeGlue", 0.2f}, {"tapeNoise", 0.67f}, {"tapeNoiseSpeed", 1.0f}, {"tapeOutput", 0.0f}, {"tapeMix", 1.0f},
            {"tapeEnabled", 1.0f}, {"destroyEnabled", 1.0f}, {"granularEnabled", 1.0f}, {"multiplyEnabled", 1.0f},
            {"inputGain", 6.0f}, {"outputGain", -6.0f}, {"masterMix", 1.0f}, {"msWidth", 2.0f}
        }},
        { "Lucid Dream", {
            {"destroyFader", 2.0f}, {"destroyIn", 0.0f}, {"destroyOut", 0.0f}, {"destroyMix", 0.4f},
            {"filterLfo", 0.08f}, {"filterCutoff", 0.7f},
            {"grainMix", 0.6f}, {"grainCloud", 0.45f}, {"grainDrift", 0.2f}, {"grainSpace", 0.7f}, {"grainMorph", 0.58f},
            {"grainFreeze", 0.0f}, {"chorusSpeed", 0.4f}, {"chorusMix", 0.45f},
            {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.5f},
                        {"tapeDrive", 0.06f}, {"tapeWear", 0.09f}, {"tapeGlue", 0.5f}, {"tapeNoise", 0.01f}, {"tapeNoiseSpeed", 1.0f}, {"tapeOutput", 0.0f}, {"tapeMix", 0.7f},
            {"tapeEnabled", 1.0f}, {"destroyEnabled", 1.0f}, {"granularEnabled", 1.0f}, {"multiplyEnabled", 1.0f},
            {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}, {"msWidth", 1.4f}
        }},
        { "Midnight Transmission", {
            {"destroyFader", 2.0f}, {"destroyIn", 3.0f}, {"destroyOut", -1.0f}, {"destroyMix", 0.75f},
            {"filterLfo", 0.12f}, {"filterCutoff", 0.45f},
            {"grainMix", 0.45f}, {"grainCloud", 0.55f}, {"grainDrift", 0.35f}, {"grainSpace", 0.5f}, {"grainMorph", 0.5f},
            {"grainFreeze", 0.0f}, {"chorusSpeed", 1.0f}, {"chorusMix", 0.3f},
            {"multiplyPanOuter", 0.8f}, {"multiplyPanInner", 0.6f},
                        {"tapeDrive", 0.21f}, {"tapeWear", 0.28f}, {"tapeGlue", 0.4f}, {"tapeNoise", 0.12f}, {"tapeNoiseSpeed", 1.0f}, {"tapeOutput", 0.0f}, {"tapeMix", 1.0f},
            {"tapeEnabled", 1.0f}, {"destroyEnabled", 1.0f}, {"granularEnabled", 1.0f}, {"multiplyEnabled", 1.0f},
            {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}, {"msWidth", 1.0f}
        }},
        { "Parallel Universe", {
            {"destroyFader", 2.0f}, {"destroyIn", 0.0f}, {"destroyOut", 0.0f}, {"destroyMix", 0.5f},
            {"filterLfo", 0.0f}, {"filterCutoff", 0.85f},
            {"grainMix", 0.7f}, {"grainCloud", 0.6f}, {"grainDrift", 0.3f}, {"grainSpace", 0.75f}, {"grainMorph", 0.7f},
            {"grainFreeze", 0.0f}, {"chorusSpeed", 0.5f}, {"chorusMix", 0.55f},
            {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.3f},
                        {"tapeDrive", 0.09f}, {"tapeWear", 0.12f}, {"tapeGlue", 0.5f}, {"tapeNoise", 0.03f}, {"tapeNoiseSpeed", 1.0f}, {"tapeOutput", 0.0f}, {"tapeMix", 0.75f},
            {"tapeEnabled", 1.0f}, {"destroyEnabled", 1.0f}, {"granularEnabled", 1.0f}, {"multiplyEnabled", 1.0f},
            {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}, {"msWidth", 1.8f}
        }},
        { "Faded Photograph", {
            {"destroyFader", 3.0f}, {"destroyIn", 0.0f}, {"destroyOut", 0.0f}, {"destroyMix", 0.45f},
            {"filterLfo", 0.0f}, {"filterCutoff", 0.5f},
            {"grainMix", 0.35f}, {"grainCloud", 0.3f}, {"grainDrift", 0.15f}, {"grainSpace", 0.5f}, {"grainMorph", 0.5f},
            {"grainFreeze", 0.0f}, {"chorusSpeed", 0.7f}, {"chorusMix", 0.25f},
            {"multiplyPanOuter", 0.9f}, {"multiplyPanInner", 0.7f},
                        {"tapeDrive", 0.11f}, {"tapeWear", 0.14f}, {"tapeGlue", 0.45f}, {"tapeNoise", 0.04f}, {"tapeNoiseSpeed", 1.0f}, {"tapeOutput", 0.0f}, {"tapeMix", 0.8f},
            {"tapeEnabled", 1.0f}, {"destroyEnabled", 1.0f}, {"granularEnabled", 1.0f}, {"multiplyEnabled", 1.0f},
            {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}, {"msWidth", 1.0f}
        }},
        { "Black Hole", {
            {"destroyFader", 2.0f}, {"destroyIn", 6.0f}, {"destroyOut", -3.0f}, {"destroyMix", 0.7f},
            {"filterLfo", 0.2f}, {"filterCutoff", 0.4f},
            {"grainMix", 0.9f}, {"grainCloud", 0.8f}, {"grainDrift", 0.7f}, {"grainSpace", 0.9f}, {"grainMorph", 0.3f},
            {"grainFreeze", 0.0f}, {"chorusSpeed", 0.2f}, {"chorusMix", 0.7f},
            {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.3f},
                        {"tapeDrive", 0.34f}, {"tapeWear", 0.46f}, {"tapeGlue", 0.6f}, {"tapeNoise", 0.29f}, {"tapeNoiseSpeed", 1.0f}, {"tapeOutput", 0.0f}, {"tapeMix", 1.0f},
            {"tapeEnabled", 1.0f}, {"destroyEnabled", 1.0f}, {"granularEnabled", 1.0f}, {"multiplyEnabled", 1.0f},
            {"inputGain", 0.0f}, {"outputGain", -3.0f}, {"masterMix", 1.0f}, {"msWidth", 2.0f}
        }},
        { "Everything On", {
            {"destroyFader", 2.0f}, {"destroyIn", 4.0f}, {"destroyOut", -2.0f}, {"destroyMix", 0.8f},
            {"filterLfo", 0.15f}, {"filterCutoff", 0.65f},
            {"grainMix", 0.5f}, {"grainCloud", 0.6f}, {"grainDrift", 0.5f}, {"grainSpace", 0.6f}, {"grainMorph", 0.5f},
            {"grainFreeze", 0.0f}, {"chorusSpeed", 0.8f}, {"chorusMix", 0.35f},
            {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.4f},
                        {"tapeDrive", 0.19f}, {"tapeWear", 0.24f}, {"tapeGlue", 0.5f}, {"tapeNoise", 0.09f}, {"tapeNoiseSpeed", 1.0f}, {"tapeOutput", 0.0f}, {"tapeMix", 1.0f},
            {"tapeEnabled", 1.0f}, {"destroyEnabled", 1.0f}, {"granularEnabled", 1.0f}, {"multiplyEnabled", 1.0f},
            {"inputGain", 0.0f}, {"outputGain", -2.0f}, {"masterMix", 1.0f}, {"msWidth", 1.5f}
        }},
        { "Studio Polish", {
            {"destroyFader", 2.0f}, {"destroyIn", 0.0f}, {"destroyOut", 0.0f}, {"destroyMix", 0.25f},
            {"filterLfo", 0.0f}, {"filterCutoff", 0.95f},
            {"grainMix", 0.15f}, {"grainCloud", 0.2f}, {"grainDrift", 0.05f}, {"grainSpace", 0.25f}, {"grainMorph", 0.5f},
            {"grainFreeze", 0.0f}, {"chorusSpeed", 0.9f}, {"chorusMix", 0.15f},
            {"multiplyPanOuter", 0.8f}, {"multiplyPanInner", 0.6f},
                        {"tapeDrive", 0.08f}, {"tapeWear", 0.13f}, {"tapeGlue", 0.5f}, {"tapeNoise", 0.02f}, {"tapeNoiseSpeed", 1.0f}, {"tapeOutput", 0.0f}, {"tapeMix", 0.4f},
            {"tapeEnabled", 1.0f}, {"destroyEnabled", 1.0f}, {"granularEnabled", 1.0f}, {"multiplyEnabled", 1.0f},
            {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}, {"msWidth", 1.05f}
        }},

        // ======== MODULATED (92-100) ========
        { "Breathing Filter", {
            {"destroyFader", 2.0f}, {"destroyIn", 2.0f}, {"destroyOut", 0.0f}, {"destroyMix", 1.0f},
            {"filterLfo", 0.0f}, {"filterCutoff", 0.6f},
            {"grainMix", 0.0f}, {"grainCloud", 0.3f}, {"grainDrift", 0.2f}, {"grainSpace", 0.3f}, {"grainMorph", 0.5f},
            {"grainFreeze", 0.0f}, {"chorusSpeed", 1.0f}, {"chorusMix", 0.0f},
            {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.8f},
                        {"tapeDrive", 0.02f}, {"tapeWear", 0.0f}, {"tapeGlue", 0.5f}, {"tapeNoise", 0.0f}, {"tapeNoiseSpeed", 1.0f}, {"tapeOutput", 0.0f}, {"tapeMix", 1.0f},
            {"tapeEnabled", 0.0f}, {"destroyEnabled", 1.0f}, {"granularEnabled", 0.0f}, {"multiplyEnabled", 0.0f},
            {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}, {"msWidth", 1.0f},
            {"modLfo1Rate", 0.3f}, {"modLfo1Depth", 0.7f}, {"modLfo1Wave", 0.0f}, {"modLfo1Sync", 0.0f},
            {"modSlot1Src", 1.0f}, {"modSlot1Tgt", 1.0f}, {"modSlot1Amt", 0.6f}
        }},
        { "Evolving Texture", {
            {"destroyFader", 2.0f}, {"destroyIn", 0.0f}, {"destroyOut", 0.0f}, {"destroyMix", 1.0f},
            {"filterLfo", 0.0f}, {"filterCutoff", 1.0f},
            {"grainMix", 0.65f}, {"grainCloud", 0.5f}, {"grainDrift", 0.3f}, {"grainSpace", 0.6f}, {"grainMorph", 0.5f},
            {"grainFreeze", 0.0f}, {"chorusSpeed", 1.0f}, {"chorusMix", 0.0f},
            {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.8f},
                        {"tapeDrive", 0.02f}, {"tapeWear", 0.0f}, {"tapeGlue", 0.5f}, {"tapeNoise", 0.0f}, {"tapeNoiseSpeed", 1.0f}, {"tapeOutput", 0.0f}, {"tapeMix", 1.0f},
            {"tapeEnabled", 0.0f}, {"destroyEnabled", 0.0f}, {"granularEnabled", 1.0f}, {"multiplyEnabled", 0.0f},
            {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}, {"msWidth", 1.2f},
            {"modLfo1Rate", 0.08f}, {"modLfo1Depth", 0.6f}, {"modLfo1Wave", 0.0f}, {"modLfo1Sync", 0.0f},
            {"modLfo2Rate", 0.13f}, {"modLfo2Depth", 0.4f}, {"modLfo2Wave", 1.0f}, {"modLfo2Sync", 0.0f},
            {"modSlot1Src", 1.0f}, {"modSlot1Tgt", 1.0f}, {"modSlot1Amt", 0.5f},
            {"modSlot2Src", 2.0f}, {"modSlot2Tgt", 5.0f}, {"modSlot2Amt", 0.4f}
        }},
        { "Rhythmic Crush", {
            {"destroyFader", 2.0f}, {"destroyIn", 4.0f}, {"destroyOut", -2.0f}, {"destroyMix", 1.0f},
            {"filterLfo", 0.0f}, {"filterCutoff", 0.7f},
            {"grainMix", 0.0f}, {"grainCloud", 0.3f}, {"grainDrift", 0.2f}, {"grainSpace", 0.3f}, {"grainMorph", 0.5f},
            {"grainFreeze", 0.0f}, {"chorusSpeed", 1.0f}, {"chorusMix", 0.0f},
            {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.8f},
                        {"tapeDrive", 0.02f}, {"tapeWear", 0.0f}, {"tapeGlue", 0.5f}, {"tapeNoise", 0.0f}, {"tapeNoiseSpeed", 1.0f}, {"tapeOutput", 0.0f}, {"tapeMix", 1.0f},
            {"tapeEnabled", 0.0f}, {"destroyEnabled", 1.0f}, {"granularEnabled", 0.0f}, {"multiplyEnabled", 0.0f},
            {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}, {"msWidth", 1.0f},
            {"modLfo1Rate", 3.0f}, {"modLfo1Depth", 0.8f}, {"modLfo1Wave", 2.0f}, {"modLfo1Sync", 1.0f},
            {"modSlot1Src", 1.0f}, {"modSlot1Tgt", 7.0f}, {"modSlot1Amt", 0.7f}
        }},
        { "Tape Drift LFO", {
            {"destroyFader", 2.0f}, {"destroyIn", 0.0f}, {"destroyOut", 0.0f}, {"destroyMix", 1.0f},
            {"filterLfo", 0.0f}, {"filterCutoff", 1.0f},
            {"grainMix", 0.0f}, {"grainCloud", 0.3f}, {"grainDrift", 0.2f}, {"grainSpace", 0.3f}, {"grainMorph", 0.5f},
            {"grainFreeze", 0.0f}, {"chorusSpeed", 1.0f}, {"chorusMix", 0.0f},
            {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.8f},
                        {"tapeDrive", 0.15f}, {"tapeWear", 0.19f}, {"tapeGlue", 0.5f}, {"tapeNoise", 0.06f}, {"tapeNoiseSpeed", 1.0f}, {"tapeOutput", 0.0f}, {"tapeMix", 1.0f},
            {"tapeEnabled", 1.0f}, {"destroyEnabled", 0.0f}, {"granularEnabled", 0.0f}, {"multiplyEnabled", 0.0f},
            {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}, {"msWidth", 1.0f},
            {"modLfo1Rate", 0.15f}, {"modLfo1Depth", 0.5f}, {"modLfo1Wave", 0.0f}, {"modLfo1Sync", 0.0f},
            {"modSlot1Src", 1.0f}, {"modSlot1Tgt", 8.0f}, {"modSlot1Amt", 0.4f},
            {"modSlot2Src", 1.0f}, {"modSlot2Tgt", 8.0f}, {"modSlot2Amt", 0.3f}
        }},
        { "Morphing Clouds", {
            {"destroyFader", 2.0f}, {"destroyIn", 0.0f}, {"destroyOut", 0.0f}, {"destroyMix", 1.0f},
            {"filterLfo", 0.0f}, {"filterCutoff", 1.0f},
            {"grainMix", 0.75f}, {"grainCloud", 0.6f}, {"grainDrift", 0.2f}, {"grainSpace", 0.65f}, {"grainMorph", 0.5f},
            {"grainFreeze", 0.0f}, {"chorusSpeed", 1.0f}, {"chorusMix", 0.0f},
            {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.8f},
                        {"tapeDrive", 0.02f}, {"tapeWear", 0.0f}, {"tapeGlue", 0.5f}, {"tapeNoise", 0.0f}, {"tapeNoiseSpeed", 1.0f}, {"tapeOutput", 0.0f}, {"tapeMix", 1.0f},
            {"tapeEnabled", 0.0f}, {"destroyEnabled", 0.0f}, {"granularEnabled", 1.0f}, {"multiplyEnabled", 0.0f},
            {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}, {"msWidth", 1.3f},
            {"modLfo1Rate", 0.05f}, {"modLfo1Depth", 0.8f}, {"modLfo1Wave", 0.0f}, {"modLfo1Sync", 0.0f},
            {"modLfo2Rate", 0.12f}, {"modLfo2Depth", 0.5f}, {"modLfo2Wave", 1.0f}, {"modLfo2Sync", 0.0f},
            {"modSlot1Src", 1.0f}, {"modSlot1Tgt", 6.0f}, {"modSlot1Amt", 0.6f},
            {"modSlot2Src", 2.0f}, {"modSlot2Tgt", 1.0f}, {"modSlot2Amt", 0.4f}
        }},
        { "Random Scatter", {
            {"destroyFader", 2.0f}, {"destroyIn", 0.0f}, {"destroyOut", 0.0f}, {"destroyMix", 1.0f},
            {"filterLfo", 0.0f}, {"filterCutoff", 1.0f},
            {"grainMix", 0.7f}, {"grainCloud", 0.7f}, {"grainDrift", 0.5f}, {"grainSpace", 0.4f}, {"grainMorph", 0.5f},
            {"grainFreeze", 0.0f}, {"chorusSpeed", 1.0f}, {"chorusMix", 0.0f},
            {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.8f},
                        {"tapeDrive", 0.02f}, {"tapeWear", 0.0f}, {"tapeGlue", 0.5f}, {"tapeNoise", 0.0f}, {"tapeNoiseSpeed", 1.0f}, {"tapeOutput", 0.0f}, {"tapeMix", 1.0f},
            {"tapeEnabled", 0.0f}, {"destroyEnabled", 0.0f}, {"granularEnabled", 1.0f}, {"multiplyEnabled", 0.0f},
            {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}, {"msWidth", 1.0f},
            {"modLfo1Rate", 2.0f}, {"modLfo1Depth", 0.7f}, {"modLfo1Wave", 3.0f}, {"modLfo1Sync", 0.0f},
            {"modSlot1Src", 1.0f}, {"modSlot1Tgt", 2.0f}, {"modSlot1Amt", 0.8f},
            {"modSlot2Src", 1.0f}, {"modSlot2Tgt", 4.0f}, {"modSlot2Amt", 0.5f}
        }},
        { "Envelope Follower Tape", {
            {"destroyFader", 2.0f}, {"destroyIn", 0.0f}, {"destroyOut", 0.0f}, {"destroyMix", 1.0f},
            {"filterLfo", 0.0f}, {"filterCutoff", 1.0f},
            {"grainMix", 0.0f}, {"grainCloud", 0.3f}, {"grainDrift", 0.2f}, {"grainSpace", 0.3f}, {"grainMorph", 0.5f},
            {"grainFreeze", 0.0f}, {"chorusSpeed", 1.0f}, {"chorusMix", 0.0f},
            {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.8f},
                        {"tapeDrive", 0.06f}, {"tapeWear", 0.05f}, {"tapeGlue", 0.5f}, {"tapeNoise", 0.01f}, {"tapeNoiseSpeed", 1.0f}, {"tapeOutput", 0.0f}, {"tapeMix", 1.0f},
            {"tapeEnabled", 1.0f}, {"destroyEnabled", 0.0f}, {"granularEnabled", 0.0f}, {"multiplyEnabled", 0.0f},
            {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}, {"msWidth", 1.0f},
            {"modSlot1Src", 3.0f}, {"modSlot1Tgt", 8.0f}, {"modSlot1Amt", 0.5f},
            {"modSlot2Src", 3.0f}, {"modSlot2Tgt", 9.0f}, {"modSlot2Amt", -0.3f}
        }},
        { "Dual LFO Cosmos", {
            {"destroyFader", 2.0f}, {"destroyIn", 2.0f}, {"destroyOut", 0.0f}, {"destroyMix", 0.6f},
            {"filterLfo", 0.0f}, {"filterCutoff", 0.7f},
            {"grainMix", 0.6f}, {"grainCloud", 0.5f}, {"grainDrift", 0.25f}, {"grainSpace", 0.65f}, {"grainMorph", 0.5f},
            {"grainFreeze", 0.0f}, {"chorusSpeed", 0.5f}, {"chorusMix", 0.4f},
            {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.5f},
                        {"tapeDrive", 0.1f}, {"tapeWear", 0.12f}, {"tapeGlue", 0.5f}, {"tapeNoise", 0.03f}, {"tapeNoiseSpeed", 1.0f}, {"tapeOutput", 0.0f}, {"tapeMix", 0.8f},
            {"tapeEnabled", 1.0f}, {"destroyEnabled", 1.0f}, {"granularEnabled", 1.0f}, {"multiplyEnabled", 1.0f},
            {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}, {"msWidth", 1.5f},
            {"modLfo1Rate", 0.1f}, {"modLfo1Depth", 0.6f}, {"modLfo1Wave", 0.0f}, {"modLfo1Sync", 0.0f},
            {"modLfo2Rate", 0.33f}, {"modLfo2Depth", 0.5f}, {"modLfo2Wave", 1.0f}, {"modLfo2Sync", 0.0f},
            {"modSlot1Src", 1.0f}, {"modSlot1Tgt", 5.0f}, {"modSlot1Amt", 0.4f},
            {"modSlot2Src", 2.0f}, {"modSlot2Tgt", 6.0f}, {"modSlot2Amt", 0.3f},
            {"modSlot3Src", 1.0f}, {"modSlot3Tgt", 8.0f}, {"modSlot3Amt", 0.25f},
            {"modSlot4Src", 2.0f}, {"modSlot4Tgt", 10.0f}, {"modSlot4Amt", 0.3f}
        }},
        { "Living Machine", {
            {"destroyFader", 2.0f}, {"destroyIn", 3.0f}, {"destroyOut", -1.0f}, {"destroyMix", 0.7f},
            {"filterLfo", 0.0f}, {"filterCutoff", 0.65f},
            {"grainMix", 0.5f}, {"grainCloud", 0.45f}, {"grainDrift", 0.3f}, {"grainSpace", 0.5f}, {"grainMorph", 0.5f},
            {"grainFreeze", 0.0f}, {"chorusSpeed", 0.8f}, {"chorusMix", 0.35f},
            {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.6f},
                        {"tapeDrive", 0.16f}, {"tapeWear", 0.2f}, {"tapeGlue", 0.5f}, {"tapeNoise", 0.07f}, {"tapeNoiseSpeed", 1.0f}, {"tapeOutput", 0.0f}, {"tapeMix", 1.0f},
            {"tapeEnabled", 1.0f}, {"destroyEnabled", 1.0f}, {"granularEnabled", 1.0f}, {"multiplyEnabled", 1.0f},
            {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}, {"msWidth", 1.3f},
            {"modLfo1Rate", 0.2f}, {"modLfo1Depth", 0.5f}, {"modLfo1Wave", 0.0f}, {"modLfo1Sync", 0.0f},
            {"modLfo2Rate", 1.5f}, {"modLfo2Depth", 0.3f}, {"modLfo2Wave", 3.0f}, {"modLfo2Sync", 0.0f},
            {"modSlot1Src", 1.0f}, {"modSlot1Tgt", 7.0f}, {"modSlot1Amt", 0.35f},
            {"modSlot2Src", 3.0f}, {"modSlot2Tgt", 8.0f}, {"modSlot2Amt", 0.4f},
            {"modSlot3Src", 2.0f}, {"modSlot3Tgt", 2.0f}, {"modSlot3Amt", 0.3f}
        }}
    };
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new StardustProcessor();
}
