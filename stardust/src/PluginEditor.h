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

    void drawToggleButton(juce::Graphics& g, juce::ToggleButton& button,
                          bool shouldDrawButtonAsHighlighted,
                          bool shouldDrawButtonAsDown) override;

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

class StarDustEditor : public juce::AudioProcessorEditor
{
public:
    explicit StarDustEditor(StarDustProcessor& p);
    ~StarDustEditor() override;

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

    StarDustProcessor& processorRef;
    StarDustLookAndFeel lookAndFeel;

    StarfieldBackground starfield;
    juce::ComboBox presetSelector;

    LabeledKnob bitsKnob, rateKnob, cutoffKnob, driveKnob, mixKnob;
    LabeledKnob grainMixKnob, grainDensityKnob, grainSizeKnob, grainScatterKnob, widthKnob;
    LabeledKnob chorusMixKnob;

    juce::Slider tuneFader;
    juce::Label tuneLabel, tuneValueLabel;
    std::unique_ptr<SliderAttachment> tuneAttachment;

    juce::ToggleButton destroyToggle, granularToggle, multiplyToggle;
    std::unique_ptr<ButtonAttachment> destroyToggleAttach, granularToggleAttach, multiplyToggleAttach;

    LevelMeter inputMeterL, inputMeterR, outputMeterL, outputMeterR;

    juce::Rectangle<int> controlsBounds;
    juce::Rectangle<int> galaxyBounds;
    juce::Rectangle<int> screenBounds;
    juce::Rectangle<int> bottomBarBounds;
    juce::Image logoImage;
    float currentScale = 1.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StarDustEditor)
};
