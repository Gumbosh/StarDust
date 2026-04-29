#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "presets/FactoryPresets.h"
#include "dsp/FastMath.h"

namespace
{
struct CharacterFlavor
{
    float gritRate;
    float gritBits;
    float gritDriveDb;
    float gritJitter;
    float gritMix;
    float exciterDrive;
    float exciterToneHz;
    float exciterMix;
};

constexpr std::array<CharacterFlavor, 6> kCharacterFlavors {{
    // Dust
    { 26040.0f, 14.0f, 3.0f, 0.12f, 0.45f, 0.24f, 5200.0f, 0.12f },
    // Glass
    { 44100.0f, 18.0f, 1.0f, 0.02f, 0.16f, 0.28f, 9600.0f, 0.36f },
    // Rust
    { 16000.0f, 10.0f, 7.0f, 0.26f, 0.68f, 0.36f, 3600.0f, 0.14f },
    // Heat
    { 44100.0f, 16.0f, 8.0f, 0.04f, 0.52f, 0.34f, 6500.0f, 0.20f },
    // Broken
    { 7000.0f, 6.0f, 12.0f, 0.42f, 0.86f, 0.64f, 4300.0f, 0.30f },
    // Glow
    { 44100.0f, 18.0f, 2.0f, 0.02f, 0.22f, 0.30f, 8200.0f, 0.46f },
}};

float blendToward(float current, float target, float amount)
{
    return current + (target - current) * amount;
}
}

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

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID("characterMode", 1), "Flavor",
        juce::StringArray{"Dust", "Glass", "Rust", "Heat", "Broken", "Glow"}, 0));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("characterAmount", 1), "Character",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("destroyIn", 1), "Crush",
        juce::NormalisableRange<float>(0.0f, 24.0f, 0.1f), 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("destroyMix", 1), "Grit Mix",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 1.0f));
    {
        juce::NormalisableRange<float> rateRange(250.0f, 96000.0f, 1.0f);
        rateRange.setSkewForCentre(3000.0f); // 3kHz at knob centre, log feel
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("destroyFader", 1), "Rate", rateRange, 22082.0f));
    }
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("destroyBits", 1), "Bit",
        juce::NormalisableRange<float>(3.0f, 24.0f, 0.001f), 12.0f));  // 12-bit SP-950 authentic
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("destroyJitter", 1), "Grit",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.10f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("exciterDrive", 1), "Heat",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.35f));
    {
        juce::NormalisableRange<float> toneRange(800.0f, 16000.0f, 1.0f);
        toneRange.setSkewForCentre(6000.0f);
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("exciterTone", 1), "Freq", toneRange, 6000.0f));
    }
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("exciterMix", 1), "Air Mix",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.25f));

    return { params.begin(), params.end() };
}

void StardustProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;

    // CRUSH DSP modules run at native rate (Degrader-style: no oversampling)
    bitCrusher.prepare(sampleRate, samplesPerBlock);

    // Pre-allocate with generous headroom for dry/wet section blends.
    dryBuffer.setSize(2, samplesPerBlock * 8, false, true, true);
    characterAmountSmoothed.reset(sampleRate, 0.02);
    characterAmountSmoothed.setCurrentAndTargetValue(0.0f);

    // Prepare Harmonic Exciter
    exciter.prepare(sampleRate, samplesPerBlock);

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

    // Flush DSP state on preset change so buffered crush/exciter state cannot
    // bleed into the new preset.
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

        // Reset active DSP modules (zeroes buffers, resets positions)
        const int blk = buffer.getNumSamples();
        bitCrusher.prepare(currentSampleRate, blk);

        // Reset new exciter as well
        exciter.prepare(currentSampleRate, blk);

        return; // skip processing this block — next block starts clean
    }

    // Measure input levels
    inputLevelLeft.store(buffer.getMagnitude(0, 0, buffer.getNumSamples()));
    if (buffer.getNumChannels() > 1)
        inputLevelRight.store(buffer.getMagnitude(1, 0, buffer.getNumSamples()));

    const auto numSamples = buffer.getNumSamples();
    const auto numChannels = buffer.getNumChannels();

    // Read parameters
    const float destroyInVal    = *apvts.getRawParameterValue("destroyIn");
    const float destroyMixVal   = *apvts.getRawParameterValue("destroyMix");
    const float destroyFaderVal = *apvts.getRawParameterValue("destroyFader");
    const float destroyBitsVal  = *apvts.getRawParameterValue("destroyBits");
    const float destroyJitterVal = *apvts.getRawParameterValue("destroyJitter");
    const float exciterDriveVal = *apvts.getRawParameterValue("exciterDrive");
    const float exciterToneVal = *apvts.getRawParameterValue("exciterTone");
    const float exciterMixVal = *apvts.getRawParameterValue("exciterMix");
    const float characterAmountTarget = juce::jlimit(0.0f, 1.0f,
        apvts.getRawParameterValue("characterAmount")->load(std::memory_order_relaxed));
    characterAmountSmoothed.setTargetValue(characterAmountTarget);
    const float characterAmountVal = characterAmountSmoothed.getNextValue();
    if (numSamples > 1)
        characterAmountSmoothed.skip(numSamples - 1);
    const int characterModeVal = juce::jlimit(0, static_cast<int>(kCharacterFlavors.size()) - 1,
        juce::roundToInt(apvts.getRawParameterValue("characterMode")->load(std::memory_order_relaxed)));
    const auto& character = kCharacterFlavors[static_cast<size_t>(characterModeVal)];

    const float effectiveDestroyMix = blendToward(destroyMixVal,
                                                  character.gritMix,
                                                  characterAmountVal);
    const bool effectiveDestroyOn = effectiveDestroyMix > 0.001f;
    const float effectiveDestroyIn = blendToward(destroyInVal, character.gritDriveDb, characterAmountVal);
    const float effectiveDestroyFader = blendToward(destroyFaderVal, character.gritRate, characterAmountVal);
    const float effectiveDestroyBits = blendToward(destroyBitsVal, character.gritBits, characterAmountVal);
    const float effectiveDestroyJitter = blendToward(destroyJitterVal, character.gritJitter, characterAmountVal);

    const float effectiveExciterMix = blendToward(exciterMixVal,
                                                  character.exciterMix,
                                                  characterAmountVal);
    const float effectiveExciterDrive = blendToward(exciterDriveVal, character.exciterDrive, characterAmountVal);
    const float effectiveExciterTone = blendToward(exciterToneVal, character.exciterToneHz, characterAmountVal);

    // Skip dry/wet blending rather than allocating if a host exceeds the
    // preallocated dry buffer.
    const bool dryBufferOk = dryBuffer.getNumSamples() >= numSamples;

    // Fixed runtime chain order (independent of legacy chain slot params).
    static constexpr int kFixedFxChain[] = { 1, 2 };
    for (const int fx : kFixedFxChain)
    {
        switch (fx)
        {
            case 1: // CRUSH — Degrader-style: S&H → bit crush → saturation (no oversampling)
            {
                if (!effectiveDestroyOn) break;

                // Save dry copy for mix blending
                if (effectiveDestroyMix < 0.999f && dryBufferOk)
                    for (int ch = 0; ch < numChannels; ++ch)
                        dryBuffer.copyFrom(ch, 0, buffer, ch, 0, numSamples);

                // Guard against NaN/Inf from automation glitch
                const float safeFaderVal = (std::isfinite(effectiveDestroyFader)) ? effectiveDestroyFader : 26040.0f;
                const float effectiveRate = juce::jlimit(250.0f, 96000.0f, safeFaderVal);

                // 1. BitCrusher: S&H rate reduction → raw truncation (no dither)
                bitCrusher.setBitDepth(effectiveDestroyBits);
                bitCrusher.setSampleRate(effectiveRate);
                bitCrusher.setJitter(effectiveDestroyJitter);
                bitCrusher.process(buffer);

                // 2. Simple tanh saturation AFTER crush (Degrader-style soft clip)
                //    destroyIn (0-24 dB) drives into saturation.
                if (effectiveDestroyIn > 0.1f)
                {
                    const float driveGain = std::pow(10.0f, effectiveDestroyIn / 20.0f);
                    for (int ch = 0; ch < numChannels; ++ch)
                    {
                        auto* data = buffer.getWritePointer(ch);
                        for (int i = 0; i < numSamples; ++i)
                        {
                            const float x = data[i] * driveGain;
                            data[i] = FastMath::tanh(x);
                        }
                    }
                }

                // Dry/wet mix
                if (effectiveDestroyMix < 0.999f && dryBufferOk)
                {
                    const float dryGain = std::cos(effectiveDestroyMix * juce::MathConstants<float>::halfPi);
                    const float wetGain = std::sin(effectiveDestroyMix * juce::MathConstants<float>::halfPi);
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

            case 2: // EXCITER - focused high-band harmonics after GRIT
            {
                exciter.process(buffer, effectiveExciterDrive, effectiveExciterTone, effectiveExciterMix);
                break;
            }
            
            default: break; // 0 = empty slot
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
            preset.values = defaultPresetValues();
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


juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new StardustProcessor();
}
