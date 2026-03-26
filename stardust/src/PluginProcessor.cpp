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
        juce::NormalisableRange<float>(0.0f, 3.0f, 0.01f), 2.0f));

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
        juce::ParameterID("stereoWidth", 1), "Stereo Width",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("filterCutoff", 1), "Filter Cutoff",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 1.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("drive", 1), "Drive",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("tone", 1), "Tone",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.5f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("chorusMix", 1), "Chorus Mix",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("multiplyPanOuter", 1), "Multiply Pan Outer",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 1.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("multiplyPanInner", 1), "Multiply Pan Inner",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.8f));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID("grainShape", 1), "Grain Shape",
        juce::StringArray{ "Hanning", "Gaussian", "Triangle", "Trapezoid" }, 0));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID("distortionEnabled", 1), "Distortion Enabled", true));
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID("destroyEnabled", 1), "Destroy Enabled", true));
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID("granularEnabled", 1), "Granular Enabled", true));
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID("multiplyEnabled", 1), "Multiply Enabled", true));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("inputGain", 1), "Input Gain",
        juce::NormalisableRange<float>(-24.0f, 12.0f, 0.1f, 1.71f), 0.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("outputGain", 1), "Output Gain",
        juce::NormalisableRange<float>(-24.0f, 12.0f, 0.1f, 1.71f), 0.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("masterMix", 1), "Master Mix",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 1.0f));

    return { params.begin(), params.end() };
}

void StardustProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;

    // 2x oversampling (order=1 means 2^1 = 2x) for distortion + destroy
    oversampling = std::make_unique<juce::dsp::Oversampling<float>>(
        2, 1, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true);
    oversampling->initProcessing(static_cast<size_t>(samplesPerBlock));

    // Distortion + destroy DSP modules run at oversampled rate
    const double oversampledRate = sampleRate * 2.0;
    saturation.prepare(oversampledRate, samplesPerBlock * 2);
    bitCrusher.prepare(oversampledRate, samplesPerBlock * 2);
    destroyDrive.prepare(oversampledRate, samplesPerBlock * 2);
    butterworthFilter.prepare(oversampledRate, samplesPerBlock * 2);

    // Granular + chorus run at original rate
    granularEngine.prepare(sampleRate, samplesPerBlock);
    chorusEngine.prepare(sampleRate, samplesPerBlock);

    // Pre-allocate with generous headroom for oversampled data
    // Use 8x to handle hosts that exceed the declared block size
    dryBuffer.setSize(2, samplesPerBlock * 8, false, true, true);
    masterDryBuffer.setSize(2, samplesPerBlock * 2, false, true, true);
    setLatencySamples(static_cast<int>(oversampling->getLatencyInSamples()));

    // Reset tone filter state to avoid click on first block
    toneStateL = 0.0f;
    toneStateR = 0.0f;
}

void StardustProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                     juce::MidiBuffer& /*midiMessages*/)
{
    juce::ScopedNoDenormals noDenormals;

    const auto totalNumInputChannels = getTotalNumInputChannels();
    const auto totalNumOutputChannels = getTotalNumOutputChannels();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear(i, 0, buffer.getNumSamples());

    // Input gain (applied before everything)
    const float inputGainDb = *apvts.getRawParameterValue("inputGain");
    if (std::abs(inputGainDb) > 0.05f)
    {
        const float inputGainLinear = std::pow(10.0f, inputGainDb / 20.0f);
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            buffer.applyGain(ch, 0, buffer.getNumSamples(), inputGainLinear);
    }

    // Measure input levels
    inputLevelLeft.store(buffer.getMagnitude(0, 0, buffer.getNumSamples()));
    if (buffer.getNumChannels() > 1)
        inputLevelRight.store(buffer.getMagnitude(1, 0, buffer.getNumSamples()));

    const auto numSamples = buffer.getNumSamples();
    const auto numChannels = buffer.getNumChannels();

    // Save dry copy for master dry/wet blend
    const float masterMixVal = *apvts.getRawParameterValue("masterMix");
    if (masterMixVal < 0.999f)
    {
        for (int ch = 0; ch < numChannels; ++ch)
            masterDryBuffer.copyFrom(ch, 0, buffer, ch, 0, numSamples);
    }

    // Read parameters
    const float destroyInVal   = *apvts.getRawParameterValue("destroyIn");
    const float destroyOutVal  = *apvts.getRawParameterValue("destroyOut");
    const float destroyMixVal  = *apvts.getRawParameterValue("destroyMix");
    const float destroyFaderVal = *apvts.getRawParameterValue("destroyFader");
    const float grainMixVal    = *apvts.getRawParameterValue("grainMix");
    const float grainDensityVal = *apvts.getRawParameterValue("grainDensity");
    const float grainSizeVal   = *apvts.getRawParameterValue("grainSize");
    const float grainScatterVal = *apvts.getRawParameterValue("grainScatter");
    const float stereoWidthVal = *apvts.getRawParameterValue("stereoWidth");
    const float cutoffVal      = *apvts.getRawParameterValue("filterCutoff");
    const float driveVal       = *apvts.getRawParameterValue("drive");
    const float chorusMixVal   = *apvts.getRawParameterValue("chorusMix");
    const float toneVal        = *apvts.getRawParameterValue("tone");
    const bool distortionOn = *apvts.getRawParameterValue("distortionEnabled") >= 0.5f;
    const bool destroyOn  = *apvts.getRawParameterValue("destroyEnabled") >= 0.5f;
    const bool granularOn = *apvts.getRawParameterValue("granularEnabled") >= 0.5f;
    const bool multiplyOn = *apvts.getRawParameterValue("multiplyEnabled") >= 0.5f;

    // If host exceeds our generous headroom, skip oversampled processing
    // rather than allocating on the audio thread
    const bool dryBufferOk = dryBuffer.getNumSamples() >= numSamples * 4;

    // Oversampled processing for DISTORTION + DESTROY sections
    const bool needsDistortion = distortionOn && driveVal > 0.001f;
    const bool needsDestroy = destroyOn;

    if ((needsDistortion || needsDestroy) && dryBufferOk)
    {
        juce::dsp::AudioBlock<float> block(buffer);
        auto oversampledBlock = oversampling->processSamplesUp(block);

        // Create non-owning AudioBuffer from oversampled block
        const auto osNumSamples = static_cast<int>(oversampledBlock.getNumSamples());
        const auto osNumChannels = static_cast<int>(oversampledBlock.getNumChannels());
        float* channelPtrs[2] = {
            oversampledBlock.getChannelPointer(0),
            osNumChannels > 1 ? oversampledBlock.getChannelPointer(1) : nullptr
        };
        juce::AudioBuffer<float> osBuffer(channelPtrs, osNumChannels, osNumSamples);

        // 1. DISTORTION section (DRIVE + TONE) at oversampled rate
        if (needsDistortion)
        {
            saturation.setInputGain(driveVal * 6.0f);
            saturation.setDrive(driveVal);
            saturation.processInput(osBuffer);

            // TONE: 1-pole low-pass (0=1kHz dark, 0.5=10kHz neutral, 1=20kHz bright)
            const float toneFreq = 1000.0f * std::pow(20.0f, toneVal);
            const float toneAlpha = 1.0f - std::exp(-2.0f * juce::MathConstants<float>::pi * toneFreq
                                                      / static_cast<float>(currentSampleRate * 2.0));
            for (int i = 0; i < osNumSamples; ++i)
            {
                auto* dataL = osBuffer.getWritePointer(0);
                toneStateL += toneAlpha * (dataL[i] - toneStateL);
                dataL[i] = toneStateL;

                if (osNumChannels > 1)
                {
                    auto* dataR = osBuffer.getWritePointer(1);
                    toneStateR += toneAlpha * (dataR[i] - toneStateR);
                    dataR[i] = toneStateR;
                }
            }

            saturation.setOutputGain(-driveVal * 6.0f);
            saturation.processOutput(osBuffer);
        }

        // 2. DESTROY section (IN, SPEED/TUNE, FILTER, OUT, MIX) at oversampled rate
        if (needsDestroy)
        {
            // Save pre-destroy dry copy for wet/dry blend
            for (int ch = 0; ch < osNumChannels; ++ch)
                dryBuffer.copyFrom(ch, 0, osBuffer, ch, 0, osNumSamples);

            // Single fader → sample rate (piecewise interpolation between RPM positions)
            // Reversed: 0=78RPM(bottom), 1=x2, 2=45RPM, 3=33RPM(top)
            static constexpr float kRpmRates[4] = { 45000.0f, 52080.0f, 26040.0f, 19000.0f };
            const float clampedFader = juce::jlimit(0.0f, 3.0f, destroyFaderVal);
            const int seg = juce::jlimit(0, 2, static_cast<int>(clampedFader));
            const float frac = clampedFader - static_cast<float>(seg);
            const float effectiveRate = kRpmRates[seg] + frac * (kRpmRates[seg + 1] - kRpmRates[seg]);

            // Input gain (dB) — drives signal into 12-bit converter
            destroyDrive.setInputDrive(destroyInVal);
            destroyDrive.processInput(osBuffer);

            // 12-bit reduction + sample-and-hold decimation
            bitCrusher.setBitDepth(12.0f);
            bitCrusher.setSampleRate(effectiveRate);
            bitCrusher.process(osBuffer);

            // Low-pass filter (6th-order Butterworth, SP-950 style)
            butterworthFilter.setCutoff(cutoffVal);
            butterworthFilter.setResonance(0.15f);
            butterworthFilter.setLFO(1.0f, 0.0f);
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
        }

        oversampling->processSamplesDown(block);
    }

    // 2. GRANULAR section
    if (granularOn)
    {
        granularEngine.setBasePitch(0.0f);
        granularEngine.setGrainShape(static_cast<GranularEngine::GrainShape>(
            static_cast<int>(*apvts.getRawParameterValue("grainShape"))));
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

    // Output safety limiter (always-on soft clip)
    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto* data = buffer.getWritePointer(ch);
        for (int i = 0; i < numSamples; ++i)
            data[i] = std::tanh(data[i]);
    }

    // Output gain (applied after limiter)
    const float outputGainDb = *apvts.getRawParameterValue("outputGain");
    if (std::abs(outputGainDb) > 0.05f)
    {
        const float outputGainLinear = std::pow(10.0f, outputGainDb / 20.0f);
        for (int ch = 0; ch < numChannels; ++ch)
            buffer.applyGain(ch, 0, numSamples, outputGainLinear);
    }

    // Master dry/wet blend (constant-power crossfade)
    if (masterMixVal < 0.999f)
    {
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

    // Measure output levels
    outputLevelLeft.store(buffer.getMagnitude(0, 0, buffer.getNumSamples()));
    if (buffer.getNumChannels() > 1)
        outputLevelRight.store(buffer.getMagnitude(1, 0, buffer.getNumSamples()));
}

void StardustProcessor::setCurrentProgram(int index)
{
    SpinLockGuard g(presetLock);
    if (index >= 0 && index < static_cast<int>(allPresets.size()))
    {
        currentPresetIndex = index;
        loadPreset(index);
    }
}

const juce::String StardustProcessor::getProgramName(int index)
{
    SpinLockGuard g(presetLock);
    if (index >= 0 && index < static_cast<int>(allPresets.size()))
        return allPresets[static_cast<size_t>(index)].name;
    return {};
}

void StardustProcessor::loadPreset(int index)
{
    // Caller must already hold presetLock, or call externally with lock
    if (index < 0 || index >= static_cast<int>(allPresets.size()))
        return;

    loadingPreset.store(true);
    ignoreParamChanges.store(static_cast<int>(allPresets[static_cast<size_t>(index)].values.size()) * 2 + 10);

    // Copy preset values under lock, then release before notifying host
    const auto values = allPresets[static_cast<size_t>(index)].values;

    for (const auto& [paramId, value] : values)
    {
        if (auto* param = apvts.getParameter(paramId))
            param->setValueNotifyingHost(param->convertTo0to1(value));
    }
    currentPresetIndex = index;
    presetDirty.store(false);
    loadingPreset.store(false);
}

bool StardustProcessor::isFactoryPreset(int index) const
{
    SpinLockGuard g(presetLock);
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
        SpinLockGuard g(presetLock);
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
        SpinLockGuard g(presetLock);
        if (currentPresetIndex >= static_cast<int>(allPresets.size()))
            currentPresetIndex = 0;
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
    SpinLockGuard g(presetLock);
    rebuildAllPresets();
    if (currentPresetIndex >= static_cast<int>(allPresets.size()))
        currentPresetIndex = 0;
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
        SpinLockGuard g(presetLock);
        state.setProperty("presetIndex", currentPresetIndex, nullptr);
        if (currentPresetIndex >= 0 && currentPresetIndex < static_cast<int>(allPresets.size()))
            state.setProperty("presetName", allPresets[static_cast<size_t>(currentPresetIndex)].name, nullptr);
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
            SpinLockGuard g(presetLock);
            bool found = false;
            if (savedName.isNotEmpty())
            {
                for (int i = 0; i < static_cast<int>(allPresets.size()); ++i)
                {
                    if (allPresets[static_cast<size_t>(i)].name == savedName)
                    {
                        currentPresetIndex = i;
                        found = true;
                        break;
                    }
                }
            }
            if (!found)
            {
                currentPresetIndex = juce::jlimit(0, juce::jmax(0, static_cast<int>(allPresets.size()) - 1), savedIndex);
            }
        }
    }
}

juce::AudioProcessorEditor* StardustProcessor::createEditor()
{
    return new StardustEditor(*this);
}

void StardustProcessor::initFactoryPresets()
{
    // Helper: common params that most presets share as defaults
    // New params: grainShape (0=Hanning,1=Gaussian,2=Triangle,3=Trapezoid), inputGain, outputGain, masterMix

    // Macro for common preset fields — keeps presets readable
    // Params: destroyFader, destroyIn, destroyOut, destroyMix, filterCutoff,
    //         grainMix, grainDensity, grainSize, grainScatter, stereoWidth, grainShape,
    //         drive, tone, chorusMix, multiplyPanOuter, multiplyPanInner,
    //         distortionEnabled, destroyEnabled, granularEnabled, multiplyEnabled,
    //         inputGain, outputGain, masterMix

    factoryPresets = {
        // ======== INIT & ESSENTIALS ========
        { "Init", {
            {"destroyFader", 2.0f}, {"destroyIn", 0.0f}, {"destroyOut", 0.0f}, {"destroyMix", 1.0f},
            {"filterCutoff", 1.0f}, {"grainMix", 0.0f}, {"grainDensity", 4.0f}, {"grainSize", 30.0f},
            {"grainScatter", 0.0f}, {"stereoWidth", 0.0f}, {"grainShape", 0.0f},
            {"drive", 0.0f}, {"tone", 0.5f}, {"chorusMix", 0.0f},
            {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.8f},
            {"distortionEnabled", 0.0f}, {"destroyEnabled", 0.0f}, {"granularEnabled", 0.0f}, {"multiplyEnabled", 0.0f},
            {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}
        }},
        { "SP-950 Classic", {
            {"destroyFader", 2.0f}, {"destroyIn", 0.0f}, {"destroyOut", 0.0f}, {"destroyMix", 1.0f},
            {"filterCutoff", 0.85f}, {"grainMix", 0.0f}, {"grainDensity", 4.0f}, {"grainSize", 30.0f},
            {"grainScatter", 0.0f}, {"stereoWidth", 0.0f}, {"grainShape", 0.0f},
            {"drive", 0.0f}, {"tone", 0.5f}, {"chorusMix", 0.0f},
            {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.8f},
            {"distortionEnabled", 0.0f}, {"destroyEnabled", 1.0f}, {"granularEnabled", 0.0f}, {"multiplyEnabled", 0.0f},
            {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}
        }},
        // ======== DESTROY — SP-950 CHARACTER ========
        { "SP-950 Hot", {
            {"destroyFader", 2.0f}, {"destroyIn", 6.0f}, {"destroyOut", -3.0f}, {"destroyMix", 1.0f},
            {"filterCutoff", 0.8f}, {"grainMix", 0.0f}, {"grainDensity", 4.0f}, {"grainSize", 30.0f},
            {"grainScatter", 0.0f}, {"stereoWidth", 0.0f}, {"grainShape", 0.0f},
            {"drive", 0.0f}, {"tone", 0.5f}, {"chorusMix", 0.0f},
            {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.8f},
            {"distortionEnabled", 0.0f}, {"destroyEnabled", 1.0f}, {"granularEnabled", 0.0f}, {"multiplyEnabled", 0.0f},
            {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}
        }},

        // ======== DESTROY FOCUSED ========
        { "33 RPM Slow", {
            {"destroyFader", 3.0f}, {"destroyIn", 0.0f}, {"destroyOut", 0.0f}, {"destroyMix", 1.0f},
            {"filterCutoff", 0.4f}, {"grainMix", 0.0f}, {"grainDensity", 4.0f}, {"grainSize", 30.0f},
            {"grainScatter", 0.0f}, {"stereoWidth", 0.0f}, {"grainShape", 0.0f},
            {"drive", 0.0f}, {"tone", 0.3f}, {"chorusMix", 0.0f},
            {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.8f},
            {"distortionEnabled", 0.0f}, {"destroyEnabled", 1.0f}, {"granularEnabled", 0.0f}, {"multiplyEnabled", 0.0f},
            {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}
        }},
        { "45 RPM Clean", {
            {"destroyFader", 2.0f}, {"destroyIn", 0.0f}, {"destroyOut", 0.0f}, {"destroyMix", 1.0f},
            {"filterCutoff", 1.0f}, {"grainMix", 0.0f}, {"grainDensity", 4.0f}, {"grainSize", 30.0f},
            {"grainScatter", 0.0f}, {"stereoWidth", 0.0f}, {"grainShape", 0.0f},
            {"drive", 0.0f}, {"tone", 0.5f}, {"chorusMix", 0.0f},
            {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.8f},
            {"distortionEnabled", 0.0f}, {"destroyEnabled", 1.0f}, {"granularEnabled", 0.0f}, {"multiplyEnabled", 0.0f},
            {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}
        }},
        { "78 RPM Bright", {
            {"destroyFader", 0.0f}, {"destroyIn", 0.0f}, {"destroyOut", 0.0f}, {"destroyMix", 1.0f},
            {"filterCutoff", 1.0f}, {"grainMix", 0.0f}, {"grainDensity", 4.0f}, {"grainSize", 30.0f},
            {"grainScatter", 0.0f}, {"stereoWidth", 0.0f}, {"grainShape", 0.0f},
            {"drive", 0.0f}, {"tone", 0.75f}, {"chorusMix", 0.0f},
            {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.8f},
            {"distortionEnabled", 0.0f}, {"destroyEnabled", 1.0f}, {"granularEnabled", 0.0f}, {"multiplyEnabled", 0.0f},
            {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}
        }},
        { "Crushed Tape", {
            {"destroyFader", 2.4f}, {"destroyIn", 8.0f}, {"destroyOut", -2.0f}, {"destroyMix", 1.0f},
            {"filterCutoff", 0.5f}, {"grainMix", 0.0f}, {"grainDensity", 4.0f}, {"grainSize", 30.0f},
            {"grainScatter", 0.0f}, {"stereoWidth", 0.0f}, {"grainShape", 0.0f},
            {"drive", 0.0f}, {"tone", 0.35f}, {"chorusMix", 0.0f},
            {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.8f},
            {"distortionEnabled", 0.0f}, {"destroyEnabled", 1.0f}, {"granularEnabled", 0.0f}, {"multiplyEnabled", 0.0f},
            {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}
        }},
        { "ADC Overdrive", {
            {"destroyFader", 2.0f}, {"destroyIn", 12.0f}, {"destroyOut", -6.0f}, {"destroyMix", 1.0f},
            {"filterCutoff", 0.8f}, {"grainMix", 0.0f}, {"grainDensity", 4.0f}, {"grainSize", 30.0f},
            {"grainScatter", 0.0f}, {"stereoWidth", 0.0f}, {"grainShape", 0.0f},
            {"drive", 0.0f}, {"tone", 0.5f}, {"chorusMix", 0.0f},
            {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.8f},
            {"distortionEnabled", 0.0f}, {"destroyEnabled", 1.0f}, {"granularEnabled", 0.0f}, {"multiplyEnabled", 0.0f},
            {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}
        }},
        { "Muffled Sampler", {
            {"destroyFader", 2.8f}, {"destroyIn", 0.0f}, {"destroyOut", 0.0f}, {"destroyMix", 1.0f},
            {"filterCutoff", 0.3f}, {"grainMix", 0.0f}, {"grainDensity", 4.0f}, {"grainSize", 30.0f},
            {"grainScatter", 0.0f}, {"stereoWidth", 0.0f}, {"grainShape", 0.0f},
            {"drive", 0.0f}, {"tone", 0.25f}, {"chorusMix", 0.0f},
            {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.8f},
            {"distortionEnabled", 0.0f}, {"destroyEnabled", 1.0f}, {"granularEnabled", 0.0f}, {"multiplyEnabled", 0.0f},
            {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}
        }},
        { "Vinyl Crackle", {
            {"destroyFader", 3.0f}, {"destroyIn", 4.0f}, {"destroyOut", -1.0f}, {"destroyMix", 1.0f},
            {"filterCutoff", 0.7f}, {"grainMix", 0.0f}, {"grainDensity", 4.0f}, {"grainSize", 30.0f},
            {"grainScatter", 0.0f}, {"stereoWidth", 0.0f}, {"grainShape", 0.0f},
            {"drive", 0.0f}, {"tone", 0.4f}, {"chorusMix", 0.0f},
            {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.8f},
            {"distortionEnabled", 0.0f}, {"destroyEnabled", 1.0f}, {"granularEnabled", 0.0f}, {"multiplyEnabled", 0.0f},
            {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}
        }},
        { "Lo-Fi Radio", {
            {"destroyFader", 2.6f}, {"destroyIn", 2.0f}, {"destroyOut", 0.0f}, {"destroyMix", 1.0f},
            {"filterCutoff", 0.45f}, {"grainMix", 0.0f}, {"grainDensity", 4.0f}, {"grainSize", 30.0f},
            {"grainScatter", 0.0f}, {"stereoWidth", 0.0f}, {"grainShape", 0.0f},
            {"drive", 0.0f}, {"tone", 0.4f}, {"chorusMix", 0.0f},
            {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.8f},
            {"distortionEnabled", 0.0f}, {"destroyEnabled", 1.0f}, {"granularEnabled", 0.0f}, {"multiplyEnabled", 0.0f},
            {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}
        }},
        { "Digital Grit", {
            {"destroyFader", 1.5f}, {"destroyIn", 6.0f}, {"destroyOut", -2.0f}, {"destroyMix", 1.0f},
            {"filterCutoff", 0.7f}, {"grainMix", 0.0f}, {"grainDensity", 4.0f}, {"grainSize", 30.0f},
            {"grainScatter", 0.0f}, {"stereoWidth", 0.0f}, {"grainShape", 0.0f},
            {"drive", 0.0f}, {"tone", 0.55f}, {"chorusMix", 0.0f},
            {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.8f},
            {"distortionEnabled", 0.0f}, {"destroyEnabled", 1.0f}, {"granularEnabled", 0.0f}, {"multiplyEnabled", 0.0f},
            {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}
        }},
        { "SP Warmth", {
            {"destroyFader", 2.0f}, {"destroyIn", 2.0f}, {"destroyOut", 0.0f}, {"destroyMix", 0.7f},
            {"filterCutoff", 0.75f}, {"grainMix", 0.0f}, {"grainDensity", 4.0f}, {"grainSize", 30.0f},
            {"grainScatter", 0.0f}, {"stereoWidth", 0.0f}, {"grainShape", 0.0f},
            {"drive", 0.0f}, {"tone", 0.45f}, {"chorusMix", 0.0f},
            {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.8f},
            {"distortionEnabled", 0.0f}, {"destroyEnabled", 1.0f}, {"granularEnabled", 0.0f}, {"multiplyEnabled", 0.0f},
            {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}
        }},

        // ======== GRANULAR FOCUSED ========
        { "Cloud Pad", {
            {"destroyFader", 0.0f}, {"destroyIn", 0.0f}, {"destroyOut", 0.0f}, {"destroyMix", 1.0f},
            {"filterCutoff", 1.0f}, {"grainMix", 0.7f}, {"grainDensity", 12.0f}, {"grainSize", 80.0f},
            {"grainScatter", 0.3f}, {"stereoWidth", 0.6f}, {"grainShape", 1.0f},
            {"drive", 0.0f}, {"tone", 0.5f}, {"chorusMix", 0.0f},
            {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.8f},
            {"distortionEnabled", 0.0f}, {"destroyEnabled", 0.0f}, {"granularEnabled", 1.0f}, {"multiplyEnabled", 0.0f},
            {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}
        }},
        { "Glitch Rain", {
            {"destroyFader", 0.0f}, {"destroyIn", 0.0f}, {"destroyOut", 0.0f}, {"destroyMix", 1.0f},
            {"filterCutoff", 1.0f}, {"grainMix", 0.8f}, {"grainDensity", 15.0f}, {"grainSize", 10.0f},
            {"grainScatter", 0.9f}, {"stereoWidth", 1.0f}, {"grainShape", 2.0f},
            {"drive", 0.0f}, {"tone", 0.5f}, {"chorusMix", 0.0f},
            {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.8f},
            {"distortionEnabled", 0.0f}, {"destroyEnabled", 0.0f}, {"granularEnabled", 1.0f}, {"multiplyEnabled", 0.0f},
            {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}
        }},
        { "Frozen Moment", {
            {"destroyFader", 0.0f}, {"destroyIn", 0.0f}, {"destroyOut", 0.0f}, {"destroyMix", 1.0f},
            {"filterCutoff", 1.0f}, {"grainMix", 0.75f}, {"grainDensity", 8.0f}, {"grainSize", 100.0f},
            {"grainScatter", 0.1f}, {"stereoWidth", 0.3f}, {"grainShape", 0.0f},
            {"drive", 0.0f}, {"tone", 0.5f}, {"chorusMix", 0.0f},
            {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.8f},
            {"distortionEnabled", 0.0f}, {"destroyEnabled", 0.0f}, {"granularEnabled", 1.0f}, {"multiplyEnabled", 0.0f},
            {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}
        }},
        { "Dust Particles", {
            {"destroyFader", 0.0f}, {"destroyIn", 0.0f}, {"destroyOut", 0.0f}, {"destroyMix", 1.0f},
            {"filterCutoff", 1.0f}, {"grainMix", 0.9f}, {"grainDensity", 20.0f}, {"grainSize", 5.0f},
            {"grainScatter", 1.0f}, {"stereoWidth", 0.8f}, {"grainShape", 3.0f},
            {"drive", 0.0f}, {"tone", 0.5f}, {"chorusMix", 0.0f},
            {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.8f},
            {"distortionEnabled", 0.0f}, {"destroyEnabled", 0.0f}, {"granularEnabled", 1.0f}, {"multiplyEnabled", 0.0f},
            {"inputGain", 0.0f}, {"outputGain", -2.0f}, {"masterMix", 1.0f}
        }},
        { "Soft Shimmer", {
            {"destroyFader", 0.0f}, {"destroyIn", 0.0f}, {"destroyOut", 0.0f}, {"destroyMix", 1.0f},
            {"filterCutoff", 1.0f}, {"grainMix", 0.6f}, {"grainDensity", 6.0f}, {"grainSize", 60.0f},
            {"grainScatter", 0.2f}, {"stereoWidth", 0.5f}, {"grainShape", 1.0f},
            {"drive", 0.0f}, {"tone", 0.5f}, {"chorusMix", 0.0f},
            {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.8f},
            {"distortionEnabled", 0.0f}, {"destroyEnabled", 0.0f}, {"granularEnabled", 1.0f}, {"multiplyEnabled", 0.0f},
            {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}
        }},
        { "Broken Glass", {
            {"destroyFader", 0.0f}, {"destroyIn", 0.0f}, {"destroyOut", 0.0f}, {"destroyMix", 1.0f},
            {"filterCutoff", 1.0f}, {"grainMix", 0.85f}, {"grainDensity", 10.0f}, {"grainSize", 8.0f},
            {"grainScatter", 0.8f}, {"stereoWidth", 0.7f}, {"grainShape", 2.0f},
            {"drive", 0.0f}, {"tone", 0.5f}, {"chorusMix", 0.0f},
            {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.8f},
            {"distortionEnabled", 0.0f}, {"destroyEnabled", 0.0f}, {"granularEnabled", 1.0f}, {"multiplyEnabled", 0.0f},
            {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}
        }},
        { "Time Stretch", {
            {"destroyFader", 0.0f}, {"destroyIn", 0.0f}, {"destroyOut", 0.0f}, {"destroyMix", 1.0f},
            {"filterCutoff", 1.0f}, {"grainMix", 0.8f}, {"grainDensity", 3.0f}, {"grainSize", 100.0f},
            {"grainScatter", 0.0f}, {"stereoWidth", 0.0f}, {"grainShape", 0.0f},
            {"drive", 0.0f}, {"tone", 0.5f}, {"chorusMix", 0.0f},
            {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.8f},
            {"distortionEnabled", 0.0f}, {"destroyEnabled", 0.0f}, {"granularEnabled", 1.0f}, {"multiplyEnabled", 0.0f},
            {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}
        }},
        { "Scatter Field", {
            {"destroyFader", 0.0f}, {"destroyIn", 0.0f}, {"destroyOut", 0.0f}, {"destroyMix", 1.0f},
            {"filterCutoff", 1.0f}, {"grainMix", 0.7f}, {"grainDensity", 8.0f}, {"grainSize", 40.0f},
            {"grainScatter", 0.7f}, {"stereoWidth", 0.9f}, {"grainShape", 3.0f},
            {"drive", 0.0f}, {"tone", 0.5f}, {"chorusMix", 0.0f},
            {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.8f},
            {"distortionEnabled", 0.0f}, {"destroyEnabled", 0.0f}, {"granularEnabled", 1.0f}, {"multiplyEnabled", 0.0f},
            {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}
        }},
        { "Grain Storm", {
            {"destroyFader", 0.0f}, {"destroyIn", 0.0f}, {"destroyOut", 0.0f}, {"destroyMix", 1.0f},
            {"filterCutoff", 1.0f}, {"grainMix", 0.9f}, {"grainDensity", 18.0f}, {"grainSize", 15.0f},
            {"grainScatter", 0.6f}, {"stereoWidth", 1.0f}, {"grainShape", 2.0f},
            {"drive", 0.0f}, {"tone", 0.5f}, {"chorusMix", 0.0f},
            {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.8f},
            {"distortionEnabled", 0.0f}, {"destroyEnabled", 0.0f}, {"granularEnabled", 1.0f}, {"multiplyEnabled", 0.0f},
            {"inputGain", 0.0f}, {"outputGain", -2.0f}, {"masterMix", 1.0f}
        }},
        { "Ambient Wash", {
            {"destroyFader", 0.0f}, {"destroyIn", 0.0f}, {"destroyOut", 0.0f}, {"destroyMix", 1.0f},
            {"filterCutoff", 1.0f}, {"grainMix", 0.6f}, {"grainDensity", 5.0f}, {"grainSize", 85.0f},
            {"grainScatter", 0.15f}, {"stereoWidth", 0.4f}, {"grainShape", 1.0f},
            {"drive", 0.0f}, {"tone", 0.5f}, {"chorusMix", 0.0f},
            {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.8f},
            {"distortionEnabled", 0.0f}, {"destroyEnabled", 0.0f}, {"granularEnabled", 1.0f}, {"multiplyEnabled", 0.0f},
            {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}
        }},

        // ======== SATURATION FOCUSED ========
        { "Warm Saturation", {
            {"destroyFader", 0.0f}, {"destroyIn", 0.0f}, {"destroyOut", 0.0f}, {"destroyMix", 1.0f},
            {"filterCutoff", 1.0f}, {"grainMix", 0.0f}, {"grainDensity", 4.0f}, {"grainSize", 30.0f},
            {"grainScatter", 0.0f}, {"stereoWidth", 0.0f}, {"grainShape", 0.0f},
            {"drive", 0.3f}, {"tone", 0.45f}, {"chorusMix", 0.0f},
            {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.8f},
            {"distortionEnabled", 1.0f}, {"destroyEnabled", 0.0f}, {"granularEnabled", 0.0f}, {"multiplyEnabled", 0.0f},
            {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}
        }},
        { "Hot Tape", {
            {"destroyFader", 0.0f}, {"destroyIn", 0.0f}, {"destroyOut", 0.0f}, {"destroyMix", 1.0f},
            {"filterCutoff", 1.0f}, {"grainMix", 0.0f}, {"grainDensity", 4.0f}, {"grainSize", 30.0f},
            {"grainScatter", 0.0f}, {"stereoWidth", 0.0f}, {"grainShape", 0.0f},
            {"drive", 0.6f}, {"tone", 0.35f}, {"chorusMix", 0.0f},
            {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.8f},
            {"distortionEnabled", 1.0f}, {"destroyEnabled", 0.0f}, {"granularEnabled", 0.0f}, {"multiplyEnabled", 0.0f},
            {"inputGain", 0.0f}, {"outputGain", -2.0f}, {"masterMix", 1.0f}
        }},
        { "Bright Edge", {
            {"destroyFader", 0.0f}, {"destroyIn", 0.0f}, {"destroyOut", 0.0f}, {"destroyMix", 1.0f},
            {"filterCutoff", 1.0f}, {"grainMix", 0.0f}, {"grainDensity", 4.0f}, {"grainSize", 30.0f},
            {"grainScatter", 0.0f}, {"stereoWidth", 0.0f}, {"grainShape", 0.0f},
            {"drive", 0.4f}, {"tone", 0.8f}, {"chorusMix", 0.0f},
            {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.8f},
            {"distortionEnabled", 1.0f}, {"destroyEnabled", 0.0f}, {"granularEnabled", 0.0f}, {"multiplyEnabled", 0.0f},
            {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}
        }},
        { "Dark Crunch", {
            {"destroyFader", 0.0f}, {"destroyIn", 0.0f}, {"destroyOut", 0.0f}, {"destroyMix", 1.0f},
            {"filterCutoff", 1.0f}, {"grainMix", 0.0f}, {"grainDensity", 4.0f}, {"grainSize", 30.0f},
            {"grainScatter", 0.0f}, {"stereoWidth", 0.0f}, {"grainShape", 0.0f},
            {"drive", 0.7f}, {"tone", 0.2f}, {"chorusMix", 0.0f},
            {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.8f},
            {"distortionEnabled", 1.0f}, {"destroyEnabled", 0.0f}, {"granularEnabled", 0.0f}, {"multiplyEnabled", 0.0f},
            {"inputGain", 0.0f}, {"outputGain", -2.0f}, {"masterMix", 1.0f}
        }},
        { "Subtle Warmth", {
            {"destroyFader", 0.0f}, {"destroyIn", 0.0f}, {"destroyOut", 0.0f}, {"destroyMix", 1.0f},
            {"filterCutoff", 1.0f}, {"grainMix", 0.0f}, {"grainDensity", 4.0f}, {"grainSize", 30.0f},
            {"grainScatter", 0.0f}, {"stereoWidth", 0.0f}, {"grainShape", 0.0f},
            {"drive", 0.15f}, {"tone", 0.5f}, {"chorusMix", 0.0f},
            {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.8f},
            {"distortionEnabled", 1.0f}, {"destroyEnabled", 0.0f}, {"granularEnabled", 0.0f}, {"multiplyEnabled", 0.0f},
            {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 0.8f}
        }},
        { "Full Overdrive", {
            {"destroyFader", 0.0f}, {"destroyIn", 0.0f}, {"destroyOut", 0.0f}, {"destroyMix", 1.0f},
            {"filterCutoff", 1.0f}, {"grainMix", 0.0f}, {"grainDensity", 4.0f}, {"grainSize", 30.0f},
            {"grainScatter", 0.0f}, {"stereoWidth", 0.0f}, {"grainShape", 0.0f},
            {"drive", 1.0f}, {"tone", 0.4f}, {"chorusMix", 0.0f},
            {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.8f},
            {"distortionEnabled", 1.0f}, {"destroyEnabled", 0.0f}, {"granularEnabled", 0.0f}, {"multiplyEnabled", 0.0f},
            {"inputGain", 0.0f}, {"outputGain", -4.0f}, {"masterMix", 1.0f}
        }},

        // ======== MULTIPLY FOCUSED ========
        { "Stereo Widen", {
            {"destroyFader", 0.0f}, {"destroyIn", 0.0f}, {"destroyOut", 0.0f}, {"destroyMix", 1.0f},
            {"filterCutoff", 1.0f}, {"grainMix", 0.0f}, {"grainDensity", 4.0f}, {"grainSize", 30.0f},
            {"grainScatter", 0.0f}, {"stereoWidth", 0.0f}, {"grainShape", 0.0f},
            {"drive", 0.0f}, {"tone", 0.5f}, {"chorusMix", 0.4f},
            {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.6f},
            {"distortionEnabled", 0.0f}, {"destroyEnabled", 0.0f}, {"granularEnabled", 0.0f}, {"multiplyEnabled", 1.0f},
            {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}
        }},
        { "Thick Chorus", {
            {"destroyFader", 0.0f}, {"destroyIn", 0.0f}, {"destroyOut", 0.0f}, {"destroyMix", 1.0f},
            {"filterCutoff", 1.0f}, {"grainMix", 0.0f}, {"grainDensity", 4.0f}, {"grainSize", 30.0f},
            {"grainScatter", 0.0f}, {"stereoWidth", 0.0f}, {"grainShape", 0.0f},
            {"drive", 0.0f}, {"tone", 0.5f}, {"chorusMix", 0.7f},
            {"multiplyPanOuter", 0.8f}, {"multiplyPanInner", 0.5f},
            {"distortionEnabled", 0.0f}, {"destroyEnabled", 0.0f}, {"granularEnabled", 0.0f}, {"multiplyEnabled", 1.0f},
            {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}
        }},
        { "Subtle Double", {
            {"destroyFader", 0.0f}, {"destroyIn", 0.0f}, {"destroyOut", 0.0f}, {"destroyMix", 1.0f},
            {"filterCutoff", 1.0f}, {"grainMix", 0.0f}, {"grainDensity", 4.0f}, {"grainSize", 30.0f},
            {"grainScatter", 0.0f}, {"stereoWidth", 0.0f}, {"grainShape", 0.0f},
            {"drive", 0.0f}, {"tone", 0.5f}, {"chorusMix", 0.2f},
            {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.9f},
            {"distortionEnabled", 0.0f}, {"destroyEnabled", 0.0f}, {"granularEnabled", 0.0f}, {"multiplyEnabled", 1.0f},
            {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}
        }},
        { "Wide Spread", {
            {"destroyFader", 0.0f}, {"destroyIn", 0.0f}, {"destroyOut", 0.0f}, {"destroyMix", 1.0f},
            {"filterCutoff", 1.0f}, {"grainMix", 0.0f}, {"grainDensity", 4.0f}, {"grainSize", 30.0f},
            {"grainScatter", 0.0f}, {"stereoWidth", 0.0f}, {"grainShape", 0.0f},
            {"drive", 0.0f}, {"tone", 0.5f}, {"chorusMix", 0.5f},
            {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.3f},
            {"distortionEnabled", 0.0f}, {"destroyEnabled", 0.0f}, {"granularEnabled", 0.0f}, {"multiplyEnabled", 1.0f},
            {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}
        }},
        { "Mono Thicken", {
            {"destroyFader", 0.0f}, {"destroyIn", 0.0f}, {"destroyOut", 0.0f}, {"destroyMix", 1.0f},
            {"filterCutoff", 1.0f}, {"grainMix", 0.0f}, {"grainDensity", 4.0f}, {"grainSize", 30.0f},
            {"grainScatter", 0.0f}, {"stereoWidth", 0.0f}, {"grainShape", 0.0f},
            {"drive", 0.0f}, {"tone", 0.5f}, {"chorusMix", 0.6f},
            {"multiplyPanOuter", 0.5f}, {"multiplyPanInner", 0.5f},
            {"distortionEnabled", 0.0f}, {"destroyEnabled", 0.0f}, {"granularEnabled", 0.0f}, {"multiplyEnabled", 1.0f},
            {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}
        }},
        { "Detuned Haze", {
            {"destroyFader", 0.0f}, {"destroyIn", 0.0f}, {"destroyOut", 0.0f}, {"destroyMix", 1.0f},
            {"filterCutoff", 1.0f}, {"grainMix", 0.0f}, {"grainDensity", 4.0f}, {"grainSize", 30.0f},
            {"grainScatter", 0.0f}, {"stereoWidth", 0.0f}, {"grainShape", 0.0f},
            {"drive", 0.0f}, {"tone", 0.5f}, {"chorusMix", 0.8f},
            {"multiplyPanOuter", 0.9f}, {"multiplyPanInner", 0.4f},
            {"distortionEnabled", 0.0f}, {"destroyEnabled", 0.0f}, {"granularEnabled", 0.0f}, {"multiplyEnabled", 1.0f},
            {"inputGain", 0.0f}, {"outputGain", -2.0f}, {"masterMix", 1.0f}
        }},

        // ======== COMBINATION PRESETS ========
        { "Lo-Fi Chill", {
            {"destroyFader", 2.0f}, {"destroyIn", 0.0f}, {"destroyOut", 0.0f}, {"destroyMix", 0.8f},
            {"filterCutoff", 0.7f}, {"grainMix", 0.3f}, {"grainDensity", 5.0f}, {"grainSize", 50.0f},
            {"grainScatter", 0.2f}, {"stereoWidth", 0.3f}, {"grainShape", 1.0f},
            {"drive", 0.2f}, {"tone", 0.4f}, {"chorusMix", 0.0f},
            {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.8f},
            {"distortionEnabled", 1.0f}, {"destroyEnabled", 1.0f}, {"granularEnabled", 1.0f}, {"multiplyEnabled", 0.0f},
            {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}
        }},
        { "Tape Machine", {
            {"destroyFader", 2.2f}, {"destroyIn", 2.0f}, {"destroyOut", 0.0f}, {"destroyMix", 1.0f},
            {"filterCutoff", 0.65f}, {"grainMix", 0.0f}, {"grainDensity", 4.0f}, {"grainSize", 30.0f},
            {"grainScatter", 0.0f}, {"stereoWidth", 0.0f}, {"grainShape", 0.0f},
            {"drive", 0.4f}, {"tone", 0.3f}, {"chorusMix", 0.0f},
            {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.8f},
            {"distortionEnabled", 1.0f}, {"destroyEnabled", 1.0f}, {"granularEnabled", 0.0f}, {"multiplyEnabled", 0.0f},
            {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}
        }},
        { "Cosmic Dust", {
            {"destroyFader", 2.0f}, {"destroyIn", 0.0f}, {"destroyOut", 0.0f}, {"destroyMix", 0.7f},
            {"filterCutoff", 0.8f}, {"grainMix", 0.5f}, {"grainDensity", 10.0f}, {"grainSize", 40.0f},
            {"grainScatter", 0.5f}, {"stereoWidth", 0.6f}, {"grainShape", 1.0f},
            {"drive", 0.0f}, {"tone", 0.5f}, {"chorusMix", 0.3f},
            {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.6f},
            {"distortionEnabled", 0.0f}, {"destroyEnabled", 1.0f}, {"granularEnabled", 1.0f}, {"multiplyEnabled", 1.0f},
            {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}
        }},
        { "VHS Memory", {
            {"destroyFader", 2.4f}, {"destroyIn", 2.0f}, {"destroyOut", 0.0f}, {"destroyMix", 0.9f},
            {"filterCutoff", 0.6f}, {"grainMix", 0.2f}, {"grainDensity", 4.0f}, {"grainSize", 45.0f},
            {"grainScatter", 0.3f}, {"stereoWidth", 0.2f}, {"grainShape", 1.0f},
            {"drive", 0.2f}, {"tone", 0.35f}, {"chorusMix", 0.15f},
            {"multiplyPanOuter", 0.9f}, {"multiplyPanInner", 0.7f},
            {"distortionEnabled", 1.0f}, {"destroyEnabled", 1.0f}, {"granularEnabled", 1.0f}, {"multiplyEnabled", 1.0f},
            {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}
        }},
        { "Dream Sequence", {
            {"destroyFader", 0.0f}, {"destroyIn", 0.0f}, {"destroyOut", 0.0f}, {"destroyMix", 1.0f},
            {"filterCutoff", 1.0f}, {"grainMix", 0.5f}, {"grainDensity", 7.0f}, {"grainSize", 70.0f},
            {"grainScatter", 0.3f}, {"stereoWidth", 0.6f}, {"grainShape", 1.0f},
            {"drive", 0.0f}, {"tone", 0.5f}, {"chorusMix", 0.6f},
            {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.5f},
            {"distortionEnabled", 0.0f}, {"destroyEnabled", 0.0f}, {"granularEnabled", 1.0f}, {"multiplyEnabled", 1.0f},
            {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}
        }},
        { "Broken Transmission", {
            {"destroyFader", 3.0f}, {"destroyIn", 8.0f}, {"destroyOut", -4.0f}, {"destroyMix", 1.0f},
            {"filterCutoff", 0.5f}, {"grainMix", 0.4f}, {"grainDensity", 8.0f}, {"grainSize", 15.0f},
            {"grainScatter", 0.8f}, {"stereoWidth", 0.5f}, {"grainShape", 2.0f},
            {"drive", 0.0f}, {"tone", 0.4f}, {"chorusMix", 0.0f},
            {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.8f},
            {"distortionEnabled", 0.0f}, {"destroyEnabled", 1.0f}, {"granularEnabled", 1.0f}, {"multiplyEnabled", 0.0f},
            {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}
        }},
        { "Underwater", {
            {"destroyFader", 2.0f}, {"destroyIn", 0.0f}, {"destroyOut", 0.0f}, {"destroyMix", 0.8f},
            {"filterCutoff", 0.25f}, {"grainMix", 0.4f}, {"grainDensity", 6.0f}, {"grainSize", 90.0f},
            {"grainScatter", 0.15f}, {"stereoWidth", 0.4f}, {"grainShape", 1.0f},
            {"drive", 0.0f}, {"tone", 0.3f}, {"chorusMix", 0.3f},
            {"multiplyPanOuter", 0.8f}, {"multiplyPanInner", 0.6f},
            {"distortionEnabled", 0.0f}, {"destroyEnabled", 1.0f}, {"granularEnabled", 1.0f}, {"multiplyEnabled", 1.0f},
            {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}
        }},
        { "Neon Nights", {
            {"destroyFader", 1.5f}, {"destroyIn", 2.0f}, {"destroyOut", 0.0f}, {"destroyMix", 0.7f},
            {"filterCutoff", 0.8f}, {"grainMix", 0.35f}, {"grainDensity", 8.0f}, {"grainSize", 35.0f},
            {"grainScatter", 0.4f}, {"stereoWidth", 0.5f}, {"grainShape", 1.0f},
            {"drive", 0.3f}, {"tone", 0.55f}, {"chorusMix", 0.4f},
            {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.5f},
            {"distortionEnabled", 1.0f}, {"destroyEnabled", 1.0f}, {"granularEnabled", 1.0f}, {"multiplyEnabled", 1.0f},
            {"inputGain", 0.0f}, {"outputGain", -2.0f}, {"masterMix", 1.0f}
        }},
        { "Industrial", {
            {"destroyFader", 2.0f}, {"destroyIn", 10.0f}, {"destroyOut", -4.0f}, {"destroyMix", 1.0f},
            {"filterCutoff", 0.6f}, {"grainMix", 0.0f}, {"grainDensity", 4.0f}, {"grainSize", 30.0f},
            {"grainScatter", 0.0f}, {"stereoWidth", 0.0f}, {"grainShape", 0.0f},
            {"drive", 0.8f}, {"tone", 0.25f}, {"chorusMix", 0.0f},
            {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.8f},
            {"distortionEnabled", 1.0f}, {"destroyEnabled", 1.0f}, {"granularEnabled", 0.0f}, {"multiplyEnabled", 0.0f},
            {"inputGain", 0.0f}, {"outputGain", -3.0f}, {"masterMix", 1.0f}
        }},
        { "Ambient Space", {
            {"destroyFader", 1.0f}, {"destroyIn", 0.0f}, {"destroyOut", 0.0f}, {"destroyMix", 0.5f},
            {"filterCutoff", 0.85f}, {"grainMix", 0.6f}, {"grainDensity", 7.0f}, {"grainSize", 75.0f},
            {"grainScatter", 0.3f}, {"stereoWidth", 0.7f}, {"grainShape", 1.0f},
            {"drive", 0.0f}, {"tone", 0.5f}, {"chorusMix", 0.5f},
            {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.5f},
            {"distortionEnabled", 0.0f}, {"destroyEnabled", 1.0f}, {"granularEnabled", 1.0f}, {"multiplyEnabled", 1.0f},
            {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}
        }},
        { "Bit Garden", {
            {"destroyFader", 3.0f}, {"destroyIn", 0.0f}, {"destroyOut", 0.0f}, {"destroyMix", 1.0f},
            {"filterCutoff", 0.7f}, {"grainMix", 0.45f}, {"grainDensity", 6.0f}, {"grainSize", 35.0f},
            {"grainScatter", 0.5f}, {"stereoWidth", 0.4f}, {"grainShape", 0.0f},
            {"drive", 0.0f}, {"tone", 0.45f}, {"chorusMix", 0.0f},
            {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.8f},
            {"distortionEnabled", 0.0f}, {"destroyEnabled", 1.0f}, {"granularEnabled", 1.0f}, {"multiplyEnabled", 0.0f},
            {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}
        }},
        { "Warm Blanket", {
            {"destroyFader", 0.0f}, {"destroyIn", 0.0f}, {"destroyOut", 0.0f}, {"destroyMix", 1.0f},
            {"filterCutoff", 0.7f}, {"grainMix", 0.0f}, {"grainDensity", 4.0f}, {"grainSize", 30.0f},
            {"grainScatter", 0.0f}, {"stereoWidth", 0.0f}, {"grainShape", 0.0f},
            {"drive", 0.3f}, {"tone", 0.4f}, {"chorusMix", 0.4f},
            {"multiplyPanOuter", 0.9f}, {"multiplyPanInner", 0.6f},
            {"distortionEnabled", 1.0f}, {"destroyEnabled", 0.0f}, {"granularEnabled", 0.0f}, {"multiplyEnabled", 1.0f},
            {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}
        }},
        { "Glitch Hop", {
            {"destroyFader", 2.0f}, {"destroyIn", 6.0f}, {"destroyOut", -2.0f}, {"destroyMix", 1.0f},
            {"filterCutoff", 0.65f}, {"grainMix", 0.4f}, {"grainDensity", 10.0f}, {"grainSize", 12.0f},
            {"grainScatter", 0.7f}, {"stereoWidth", 0.5f}, {"grainShape", 2.0f},
            {"drive", 0.15f}, {"tone", 0.45f}, {"chorusMix", 0.0f},
            {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.8f},
            {"distortionEnabled", 1.0f}, {"destroyEnabled", 1.0f}, {"granularEnabled", 1.0f}, {"multiplyEnabled", 0.0f},
            {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}
        }},
        { "Nebula", {
            {"destroyFader", 1.5f}, {"destroyIn", 0.0f}, {"destroyOut", 0.0f}, {"destroyMix", 0.6f},
            {"filterCutoff", 0.9f}, {"grainMix", 0.7f}, {"grainDensity", 10.0f}, {"grainSize", 70.0f},
            {"grainScatter", 0.5f}, {"stereoWidth", 1.0f}, {"grainShape", 1.0f},
            {"drive", 0.1f}, {"tone", 0.5f}, {"chorusMix", 0.5f},
            {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.4f},
            {"distortionEnabled", 1.0f}, {"destroyEnabled", 1.0f}, {"granularEnabled", 1.0f}, {"multiplyEnabled", 1.0f},
            {"inputGain", 0.0f}, {"outputGain", -3.0f}, {"masterMix", 1.0f}
        }},
        { "Old Radio", {
            {"destroyFader", 2.8f}, {"destroyIn", 2.0f}, {"destroyOut", 0.0f}, {"destroyMix", 1.0f},
            {"filterCutoff", 0.35f}, {"grainMix", 0.0f}, {"grainDensity", 4.0f}, {"grainSize", 30.0f},
            {"grainScatter", 0.0f}, {"stereoWidth", 0.0f}, {"grainShape", 0.0f},
            {"drive", 0.5f}, {"tone", 0.3f}, {"chorusMix", 0.0f},
            {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.8f},
            {"distortionEnabled", 1.0f}, {"destroyEnabled", 1.0f}, {"granularEnabled", 0.0f}, {"multiplyEnabled", 0.0f},
            {"inputGain", 0.0f}, {"outputGain", 0.0f}, {"masterMix", 1.0f}
        }},
        { "Everything On", {
            {"destroyFader", 2.0f}, {"destroyIn", 2.0f}, {"destroyOut", 0.0f}, {"destroyMix", 0.8f},
            {"filterCutoff", 0.75f}, {"grainMix", 0.4f}, {"grainDensity", 8.0f}, {"grainSize", 45.0f},
            {"grainScatter", 0.4f}, {"stereoWidth", 0.5f}, {"grainShape", 1.0f},
            {"drive", 0.35f}, {"tone", 0.45f}, {"chorusMix", 0.4f},
            {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.6f},
            {"distortionEnabled", 1.0f}, {"destroyEnabled", 1.0f}, {"granularEnabled", 1.0f}, {"multiplyEnabled", 1.0f},
            {"inputGain", 0.0f}, {"outputGain", -2.0f}, {"masterMix", 1.0f}
        }}
    };
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new StardustProcessor();
}
