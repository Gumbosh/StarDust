#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"
#include "gui/StarfieldBackground.h"

class StardustLookAndFeel : public juce::LookAndFeel_V4
{
public:
    StardustLookAndFeel();

    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPos, float rotaryStartAngle,
                          float rotaryEndAngle, juce::Slider& slider) override;

    void drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPos, float minSliderPos, float maxSliderPos,
                          juce::Slider::SliderStyle style, juce::Slider& slider) override;

    void drawToggleButton(juce::Graphics& g, juce::ToggleButton& button,
                          bool shouldDrawButtonAsHighlighted,
                          bool shouldDrawButtonAsDown) override;

    void drawComboBox(juce::Graphics& g, int width, int height, bool isButtonDown,
                      int buttonX, int buttonY, int buttonW, int buttonH,
                      juce::ComboBox& box) override;

    juce::Font getComboBoxFont(juce::ComboBox& box) override;

    // Monochrome galaxy palette
    static inline const juce::Colour kBg { 0xFF050505 };
    static inline const juce::Colour kFg { 0xFFE0E0E0 };
    static inline const juce::Colour kFgDim { 0xFF888888 };
    static inline const juce::Colour kFgGhost { 0xFF3A3A3A };
    static inline const juce::Colour kKnobBody { 0xFF0A0A0A };
    static inline const juce::Colour kKnobRing { 0xFF2A2A2A };
    static inline const juce::Colour kAccent { 0xFFFFFFFF };
    static inline const juce::Colour kInset { 0xFF060606 };
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
    float peakLevel = 0.0f;
    int peakHoldCounter = 0;
    static constexpr int kPeakHoldFrames = 45; // ~1.5s at 30Hz
};

class StardustEditor : public juce::AudioProcessorEditor
{
public:
    explicit StardustEditor(StardustProcessor& p);
    ~StardustEditor() override;

    void paint(juce::Graphics& g) override;
    void paintOverChildren(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& e) override;

private:
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    struct LabeledKnob
    {
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<SliderAttachment> attachment;
    };

    void setupKnob(LabeledKnob& knob, const juce::String& paramId, const juce::String& labelText);
    void layoutKnobInBounds(LabeledKnob& knob, juce::Rectangle<int> bounds);

    StardustProcessor& processorRef;
    StardustLookAndFeel lookAndFeel;

    StarfieldBackground starfield;
    juce::ComboBox presetSelector;

    LabeledKnob driveKnob, toneKnob;
    LabeledKnob bitsKnob, rateKnob, cutoffKnob, mixKnob;
    LabeledKnob grainMixKnob, grainDensityKnob, grainSizeKnob, grainScatterKnob, widthKnob;
    LabeledKnob chorusMixKnob;
    LabeledKnob panOuterKnob, panInnerKnob; // temp

    juce::Slider tuneFader;
    juce::Label tuneLabel, tuneValueLabel;
    std::unique_ptr<SliderAttachment> tuneAttachment;

    juce::ToggleButton distortionToggle, destroyToggle, granularToggle, multiplyToggle;
    std::unique_ptr<ButtonAttachment> distortionToggleAttach, destroyToggleAttach, granularToggleAttach, multiplyToggleAttach;

    LevelMeter inputMeterL, inputMeterR, outputMeterL, outputMeterR;

    juce::Rectangle<int> controlsBounds;
    juce::Rectangle<int> galaxyBounds;
    juce::Rectangle<int> screenBounds;
    juce::Rectangle<int> bottomBarBounds;
    juce::Image logoImage;
    float currentScale = 1.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StardustEditor)
};
