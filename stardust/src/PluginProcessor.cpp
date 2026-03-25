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

    // Pre-allocate with extra headroom to avoid audio-thread allocation
    dryBuffer.setSize(2, samplesPerBlock * 2, false, true, true);
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

    // Ensure dryBuffer is large enough (host may exceed prepareToPlay block size)
    if (dryBuffer.getNumSamples() < numSamples)
        dryBuffer.setSize(2, numSamples, false, false, true);

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
        // Save pre-destroy copy for blending (pre-allocated in prepareToPlay)
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
            preset.name = xml->getStringAttribute("name", file.getFileNameWithoutExtension());
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
    factoryPresets = {
        { "Default", {
            {"bitDepth", 16.0f}, {"sampleRate", 48000.0f},
            {"grainMix", 0.0f}, {"grainDensity", 1.0f}, {"grainSize", 30.0f},
            {"grainScatter", 0.0f}, {"grainTune", 0.0f}, {"stereoWidth", 0.0f},
            {"filterCutoff", 99.0f}, {"drive", 0.0f}, {"tone", 0.5f}, {"mix", 1.0f}, {"chorusMix", 0.0f},
            {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.8f},
            {"distortionEnabled", 1.0f}, {"destroyEnabled", 1.0f}, {"granularEnabled", 1.0f}, {"multiplyEnabled", 1.0f}
        }},
        { "Classic SP", {
            {"bitDepth", 12.0f}, {"sampleRate", 26040.0f},
            {"grainMix", 0.0f}, {"grainDensity", 4.0f}, {"grainSize", 30.0f},
            {"grainScatter", 0.2f}, {"grainTune", 0.0f}, {"stereoWidth", 0.0f},
            {"filterCutoff", 99.0f}, {"drive", 0.15f}, {"tone", 0.5f}, {"mix", 1.0f}, {"chorusMix", 0.0f},
            {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.8f},
            {"distortionEnabled", 1.0f}, {"destroyEnabled", 1.0f}, {"granularEnabled", 1.0f}, {"multiplyEnabled", 1.0f}
        }},
        { "Lo-Fi", {
            {"bitDepth", 12.0f}, {"sampleRate", 26040.0f},
            {"grainMix", 0.4f}, {"grainDensity", 4.0f}, {"grainSize", 40.0f},
            {"grainScatter", 0.2f}, {"grainTune", 0.0f}, {"stereoWidth", 0.2f},
            {"filterCutoff", 78.0f}, {"drive", 0.2f}, {"tone", 0.4f}, {"mix", 0.75f}, {"chorusMix", 0.0f},
            {"multiplyPanOuter", 1.0f}, {"multiplyPanInner", 0.8f},
            {"distortionEnabled", 1.0f}, {"destroyEnabled", 1.0f}, {"granularEnabled", 1.0f}, {"multiplyEnabled", 1.0f}
        }}
    };
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new StardustProcessor();
}
