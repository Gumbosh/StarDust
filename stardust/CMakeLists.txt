cmake_minimum_required(VERSION 3.22)
project(Stardust VERSION 1.0.0)

add_subdirectory(JUCE)

juce_add_plugin(Stardust
    COMPANY_NAME "Stardust"
    PLUGIN_MANUFACTURER_CODE Stdt
    PLUGIN_CODE Sdt1
    FORMATS AU VST3 Standalone
    PRODUCT_NAME "Stardust"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_AUTO_MANIFEST TRUE
    ICON_BIG "${CMAKE_CURRENT_SOURCE_DIR}/assets/stardust-logo.png"
    ICON_SMALL "${CMAKE_CURRENT_SOURCE_DIR}/assets/stardust-logo.png"
    COPY_PLUGIN_AFTER_BUILD FALSE
)

target_sources(Stardust PRIVATE
    src/PluginProcessor.cpp
    src/PluginProcessor.h
    src/PluginEditor.cpp
    src/PluginEditor.h
    src/dsp/BitCrusher.cpp
    src/dsp/BitCrusher.h
    src/dsp/DestroyDrive.cpp
    src/dsp/DestroyDrive.h
    src/dsp/GranularEngine.cpp
    src/dsp/GranularEngine.h
    src/dsp/ButterworthFilter.cpp
    src/dsp/ButterworthFilter.h
    src/dsp/ChorusEngine.cpp
    src/dsp/ChorusEngine.h
    src/dsp/DattorroReverb.cpp
    src/dsp/DattorroReverb.h
    src/dsp/GrainEnvelopeLUT.h
    src/dsp/InputEnvelopeFollower.h
    src/dsp/TapeEngine.cpp
    src/dsp/TapeEngine.h
    src/dsp/Saturation.cpp
    src/dsp/Saturation.h
    src/gui/StarfieldBackground.cpp
    src/gui/StarfieldBackground.h
    src/gui/PresetLibraryPanel.cpp
    src/gui/PresetLibraryPanel.h
)

juce_add_binary_data(StardustAssets SOURCES
    assets/logo.png
    assets/stardust-logo.png
)

target_compile_definitions(Stardust PUBLIC
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    STARDUST_VERSION="${PROJECT_VERSION}"
)

target_compile_features(Stardust PUBLIC cxx_std_17)

target_link_libraries(Stardust
    PRIVATE
        StardustAssets
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)
