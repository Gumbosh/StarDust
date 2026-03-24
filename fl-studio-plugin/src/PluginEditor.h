#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"
#include "gui/StarfieldBackground.h"

class StarDustLookAndFeel : public juce::LookAndFeel_V4
{
public:
    StarDustLookAndFeel();

    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPos, float rotaryStartAngle,
                          float rotaryEndAngle, juce::Slider& slider) override;

    void drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPos, float minSliderPos, float maxSliderPos,
                          juce::Slider::SliderStyle style, juce::Slider& slider) override;

    // Vintage warm palette
    static inline const juce::Colour kPanel { 0xFF1C1A18 };        // warm dark charcoal
    static inline const juce::Colour kPanelLight { 0xFF2A2724 };   // lighter panel
    static inline const juce::Colour kAccent { 0xFFD4A24C };       // warm amber/gold
    static inline const juce::Colour kAccentDim { 0xFF8B6A30 };    // dim amber
    static inline const juce::Colour kKnobBody { 0xFF3D3935 };     // warm dark gray
    static inline const juce::Colour kKnobRing { 0xFF504A44 };     // knob outer ring
    static inline const juce::Colour kKnobHighlight { 0xFF5A5248 };// knob highlight
    static inline const juce::Colour kLabel { 0xFFB8A88A };        // cream/tan label
    static inline const juce::Colour kTextBright { 0xFFE8D8C0 };   // bright cream
    static inline const juce::Colour kScrewHead { 0xFF444038 };    // screw accent
    static inline const juce::Colour kInset { 0xFF141210 };        // inset/recessed
};

class LevelMeter : public juce::Component, public juce::Timer
{
public:
    explicit LevelMeter(std::atomic<float>& levelSource);
    void paint(juce::Graphics& g) override;
    void timerCallback() override;

private:
    std::atomic<float>& level;
    float displayLevel = 0.0f;
};

class StarDustEditor : public juce::AudioProcessorEditor
{
public:
    explicit StarDustEditor(StarDustProcessor& p);
    ~StarDustEditor() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;

    struct LabeledKnob
    {
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<SliderAttachment> attachment;
    };

    void setupKnob(LabeledKnob& knob, const juce::String& paramId, const juce::String& labelText);
    void layoutKnobInBounds(LabeledKnob& knob, juce::Rectangle<int> bounds);
    void drawScrew(juce::Graphics& g, float x, float y);

    StarDustProcessor& processorRef;
    StarDustLookAndFeel lookAndFeel;

    StarfieldBackground starfield;
    juce::ComboBox presetSelector;

    LabeledKnob bitsKnob;
    LabeledKnob rateKnob;
    LabeledKnob cutoffKnob;
    LabeledKnob driveKnob;
    LabeledKnob mixKnob;

    juce::Slider tuneFader;
    juce::Label tuneLabel;
    juce::Label tuneValueLabel;
    std::unique_ptr<SliderAttachment> tuneAttachment;

    LabeledKnob grainMixKnob;
    LabeledKnob grainDensityKnob;
    LabeledKnob grainSizeKnob;
    LabeledKnob grainScatterKnob;
    LabeledKnob widthKnob;

    LevelMeter inputMeterL, inputMeterR;
    LevelMeter outputMeterL, outputMeterR;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StarDustEditor)
};
