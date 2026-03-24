#include "PluginProcessor.h"
#include "PluginEditor.h"

StardustProcessor::StardustProcessor()
    : AudioProcessor(BusesProperties()
          .withInput("Input", juce::AudioChannelSet::stereo(), true)
          .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "Parameters", createParameterLayout())
{
    initFactoryPresets();
}

juce::AudioProcessorValueTreeState::ParameterLayout StardustProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("bitDepth", 1), "Bit Depth",
        juce::NormalisableRange<float>(4.0f, 16.0f, 0.1f), 12.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("sampleRate", 1), "Sample Rate",
        juce::NormalisableRange<float>(4000.0f, 48000.0f, 1.0f, 0.5f), 26040.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("grainMix", 1), "Grain Mix",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("grainDensity", 1), "Grain Density",
        juce::NormalisableRange<float>(1.0f, 20.0f, 1.0f), 4.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("grainSize", 1), "Grain Size",
        juce::NormalisableRange<float>(5.0f, 100.0f, 0.1f, 0.5f), 30.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("grainScatter", 1), "Grain Scatter",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.2f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("grainTune", 1), "Grain Tune",
        juce::NormalisableRange<float>(-24.0f, 24.0f, 0.1f), 0.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("stereoWidth", 1), "Stereo Width",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("filterCutoff", 1), "Filter Cutoff",
        juce::NormalisableRange<float>(0.0f, 99.0f, 1.0f), 99.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("drive", 1), "Drive",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("tone", 1), "Tone",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.5f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("mix", 1), "Mix",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 1.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("chorusMix", 1), "Chorus Mix",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("multiplyPanOuter", 1), "Multiply Pan Outer",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 1.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("multiplyPanInner", 1), "Multiply Pan Inner",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.8f));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID("distortionEnabled", 1), "Distortion Enabled", true));
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID("destroyEnabled", 1), "Destroy Enabled", true));
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID("granularEnabled", 1), "Granular Enabled", true));
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID("multiplyEnabled", 1), "Multiply Enabled", true));

    return { params.begin(), params.end() };
}

void StardustProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    saturation.prepare(sampleRate, samplesPerBlock);
    bitCrusher.prepare(sampleRate, samplesPerBlock);
    granularEngine.prepare(sampleRate, samplesPerBlock);
    butterworthFilter.prepare(sampleRate, samplesPerBlock);
    chorusEngine.prepare(sampleRate, samplesPerBlock);

    dryBuffer.setSize(2, samplesPerBlock, false, true, true);
    setLatencySamples(0);
}

void StardustProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                     juce::MidiBuffer& /*midiMessages*/)
{
    juce::ScopedNoDenormals noDenormals;

    const auto totalNumInputChannels = getTotalNumInputChannels();
    const auto totalNumOutputChannels = getTotalNumOutputChannels();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear(i, 0, buffer.getNumSamples());

    // Measure input levels
    inputLevelLeft.store(buffer.getMagnitude(0, 0, buffer.getNumSamples()));
    if (buffer.getNumChannels() > 1)
        inputLevelRight.store(buffer.getMagnitude(1, 0, buffer.getNumSamples()));

    const auto numSamples = buffer.getNumSamples();
    const auto numChannels = buffer.getNumChannels();

    // Read parameters
    const float bitDepthVal    = *apvts.getRawParameterValue("bitDepth");
    const float sampleRateVal  = *apvts.getRawParameterValue("sampleRate");
    const float grainMixVal    = *apvts.getRawParameterValue("grainMix");
    const float grainDensityVal = *apvts.getRawParameterValue("grainDensity");
    const float grainSizeVal   = *apvts.getRawParameterValue("grainSize");
    const float grainScatterVal = *apvts.getRawParameterValue("grainScatter");
    const float grainTuneVal   = *apvts.getRawParameterValue("grainTune");
    const float stereoWidthVal = *apvts.getRawParameterValue("stereoWidth");
    const float cutoffVal      = *apvts.getRawParameterValue("filterCutoff");
    const float driveVal       = *apvts.getRawParameterValue("drive");
    const float mixVal         = *apvts.getRawParameterValue("mix");
    const float chorusMixVal   = *apvts.getRawParameterValue("chorusMix");
    const float toneVal        = *apvts.getRawParameterValue("tone");
    const bool distortionOn = *apvts.getRawParameterValue("distortionEnabled") >= 0.5f;
    const bool destroyOn  = *apvts.getRawParameterValue("destroyEnabled") >= 0.5f;
    const bool granularOn = *apvts.getRawParameterValue("granularEnabled") >= 0.5f;
    const bool multiplyOn = *apvts.getRawParameterValue("multiplyEnabled") >= 0.5f;

    // 1. DISTORTION section (DRIVE + TONE)
    if (distortionOn && driveVal > 0.001f)
    {
        saturation.setInputGain(driveVal * 6.0f);
        saturation.setDrive(driveVal);
        saturation.processInput(buffer);

        // TONE: 1-pole low-pass (0=1kHz dark, 0.5=10kHz neutral, 1=20kHz bright)
        const float toneFreq = 1000.0f * std::pow(20.0f, toneVal);
        const float toneAlpha = 1.0f - std::exp(-2.0f * juce::MathConstants<float>::pi * toneFreq
                                                  / static_cast<float>(currentSampleRate));
        for (int i = 0; i < numSamples; ++i)
        {
            auto* dataL = buffer.getWritePointer(0);
            toneStateL += toneAlpha * (dataL[i] - toneStateL);
            dataL[i] = toneStateL;

            if (numChannels > 1)
            {
                auto* dataR = buffer.getWritePointer(1);
                toneStateR += toneAlpha * (dataR[i] - toneStateR);
                dataR[i] = toneStateR;
            }
        }

        saturation.setOutputGain(-driveVal * 6.0f);
        saturation.processOutput(buffer);
    }

    // 2. DESTROY section (BITS, RATE, CUTOFF, PITCH, MIX)
    if (destroyOn && mixVal > 0.001f)
    {
        // Save pre-destroy copy for blending
        if (dryBuffer.getNumChannels() < numChannels || dryBuffer.getNumSamples() < numSamples)
            dryBuffer.setSize(numChannels, numSamples, false, false, true);
        for (int ch = 0; ch < numChannels; ++ch)
            dryBuffer.copyFrom(ch, 0, buffer, ch, 0, numSamples);

        const float pitchRatio = std::pow(2.0f, grainTuneVal / 12.0f);
        const float effectiveRate = sampleRateVal * pitchRatio;
        bitCrusher.setBitDepth(bitDepthVal);
        bitCrusher.setSampleRate(effectiveRate);
        bitCrusher.process(buffer);

        butterworthFilter.setCutoff(cutoffVal);
        butterworthFilter.setResonance(0.0f);
        butterworthFilter.setLFO(1.0f, 0.0f);
        butterworthFilter.process(buffer);

        // Blend destroy wet with dry
        if (mixVal < 0.999f)
        {
            for (int ch = 0; ch < numChannels; ++ch)
            {
                auto* wet = buffer.getWritePointer(ch);
                const auto* dry = dryBuffer.getReadPointer(ch);
                for (int i = 0; i < numSamples; ++i)
                    wet[i] = dry[i] * (1.0f - mixVal) + wet[i] * mixVal;
            }
        }
    }

    // 2. GRANULAR section
    if (granularOn)
    {
        granularEngine.setBasePitch(0.0f);
        granularEngine.setParameters(
            grainSizeVal, grainDensityVal, grainScatterVal,
            grainMixVal, stereoWidthVal);
        granularEngine.process(buffer);
    }

    // 3. MULTIPLY section
    if (multiplyOn)
    {
        const float panOuter = *apvts.getRawParameterValue("multiplyPanOuter");
        const float panInner = *apvts.getRawParameterValue("multiplyPanInner");
        chorusEngine.setMix(chorusMixVal);
        chorusEngine.setPans(panOuter, panInner);
        chorusEngine.process(buffer);
    }

    // Measure output levels
    outputLevelLeft.store(buffer.getMagnitude(0, 0, buffer.getNumSamples()));
    if (buffer.getNumChannels() > 1)
        outputLevelRight.store(buffer.getMagnitude(1, 0, buffer.getNumSamples()));
}

void StardustProcessor::setCurrentProgram(int index)
{
    if (index >= 0 && index < static_cast<int>(factoryPresets.size()))
    {
        currentPresetIndex = index;
        loadPreset(index);
    }
}

const juce::String StardustProcessor::getProgramName(int index)
{
    if (index >= 0 && index < static_cast<int>(factoryPresets.size()))
        return factoryPresets[static_cast<size_t>(index)].name;
    return {};
}

void StardustProcessor::loadPreset(int index)
{
    if (index < 0 || index >= static_cast<int>(factoryPresets.size()))
        return;

    const auto& preset = factoryPresets[static_cast<size_t>(index)];
    for (const auto& [paramId, value] : preset.values)
    {
        if (auto* param = apvts.getParameter(paramId))
            param->setValueNotifyingHost(param->convertTo0to1(value));
    }
    currentPresetIndex = index;
}

void StardustProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void StardustProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    if (xmlState != nullptr && xmlState->hasTagName(apvts.state.getType()))
        apvts.replaceState(juce::ValueTree::fromXml(*xmlState));
}

juce::AudioProcessorEditor* StardustProcessor::createEditor()
{
    return new StardustEditor(*this);
}

void StardustProcessor::initFactoryPresets()
{
    factoryPresets = {
        { "Default", {
            {"bitDepth", 16.0f}, {"sampleRate", 48000.0f},
            {"grainMix", 0.0f}, {"grainDensity", 1.0f}, {"grainSize", 30.0f},
            {"grainScatter", 0.0f}, {"grainTune", 0.0f}, {"stereoWidth", 0.0f},
            {"filterCutoff", 99.0f}, {"drive", 0.0f}, {"tone", 0.5f}, {"mix", 1.0f}, {"chorusMix", 0.0f},
            {"distortionEnabled", 1.0f}, {"destroyEnabled", 1.0f}, {"granularEnabled", 1.0f}, {"multiplyEnabled", 1.0f}
        }},
        { "Classic SP", {
            {"bitDepth", 12.0f}, {"sampleRate", 26040.0f},
            {"grainMix", 0.0f}, {"grainDensity", 4.0f}, {"grainSize", 30.0f},
            {"grainScatter", 0.2f}, {"grainTune", 0.0f}, {"stereoWidth", 0.0f},
            {"filterCutoff", 99.0f}, {"drive", 0.15f}, {"tone", 0.5f}, {"mix", 1.0f}, {"chorusMix", 0.0f},
            {"distortionEnabled", 1.0f}, {"destroyEnabled", 1.0f}, {"granularEnabled", 1.0f}, {"multiplyEnabled", 1.0f}
        }},
        { "Lo-Fi", {
            {"bitDepth", 12.0f}, {"sampleRate", 26040.0f},
            {"grainMix", 0.4f}, {"grainDensity", 4.0f}, {"grainSize", 40.0f},
            {"grainScatter", 0.2f}, {"grainTune", 0.0f}, {"stereoWidth", 0.2f},
            {"filterCutoff", 78.0f}, {"drive", 0.2f}, {"tone", 0.4f}, {"mix", 0.75f}, {"chorusMix", 0.0f},
            {"distortionEnabled", 1.0f}, {"destroyEnabled", 1.0f}, {"granularEnabled", 1.0f}, {"multiplyEnabled", 1.0f}
        }}
    };
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new StardustProcessor();
}
