cmake_minimum_required(VERSION 3.22)
project(StarDust VERSION 1.0.0)

add_subdirectory(JUCE)

juce_add_plugin(StarDust
    COMPANY_NAME "StarDust"
    PLUGIN_MANUFACTURER_CODE Stdt
    PLUGIN_CODE Sdt1
    FORMATS VST3 Standalone
    PRODUCT_NAME "StarDust"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_AUTO_MANIFEST TRUE
    COPY_PLUGIN_AFTER_BUILD TRUE
)

target_sources(StarDust PRIVATE
    src/PluginProcessor.cpp
    src/PluginProcessor.h
    src/PluginEditor.cpp
    src/PluginEditor.h
    src/dsp/BitCrusher.cpp
    src/dsp/BitCrusher.h
    src/dsp/DetuneEngine.cpp
    src/dsp/DetuneEngine.h
    src/dsp/GranularEngine.cpp
    src/dsp/GranularEngine.h
    src/dsp/ButterworthFilter.cpp
    src/dsp/ButterworthFilter.h
    src/dsp/Saturation.cpp
    src/dsp/Saturation.h
    src/gui/StarfieldBackground.cpp
    src/gui/StarfieldBackground.h
)

target_compile_definitions(StarDust PUBLIC
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
)

target_compile_features(StarDust PUBLIC cxx_std_17)

target_link_libraries(StarDust
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)
