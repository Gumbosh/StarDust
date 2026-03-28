#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"
#include "gui/StarfieldBackground.h"
#include "gui/PresetLibraryPanel.h"

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

    void drawButtonBackground(juce::Graphics& g, juce::Button& button, const juce::Colour& backgroundColour,
                              bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;
    void drawButtonText(juce::Graphics& g, juce::TextButton& button,
                        bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;
    void drawAlertBox(juce::Graphics& g, juce::AlertWindow& window,
                      const juce::Rectangle<int>& textArea, juce::TextLayout& layout) override;

    void drawComboBox(juce::Graphics& g, int width, int height, bool isButtonDown,
                      int buttonX, int buttonY, int buttonW, int buttonH,
                      juce::ComboBox& box) override;

    juce::Font getComboBoxFont(juce::ComboBox& box) override;
    void positionComboBoxText(juce::ComboBox& box, juce::Label& label) override;

    void drawBubble(juce::Graphics& g, juce::BubbleComponent& bubble,
                    const juce::Point<float>& tip, const juce::Rectangle<float>& body) override;
    juce::Font getSliderPopupFont(juce::Slider& slider) override;
    int getSliderPopupPlacement(juce::Slider& slider) override;

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

// LevelMeter removed — replaced by signal flow display + knobs

class StardustDialog : public juce::Component
{
public:
    StardustDialog(const juce::String& title, const juce::String& defaultText,
                   std::function<void(const juce::String&)> onConfirm);
    void paint(juce::Graphics& g) override;
    void resized() override;
    bool keyPressed(const juce::KeyPress& key) override;

    void setBankOptions(const std::vector<juce::String>& banks);
    void selectBank(int comboId);
    juce::String getSelectedBank() const;

    // Called with (presetName, bankName) when bank options are set, otherwise (presetName, "")
    std::function<void(const juce::String&, const juce::String&)> onConfirmWithBank;
    std::function<void()> onDismiss;

    juce::TextButton confirmBtn, cancelBtn;

private:
    void doConfirm();
    void updateSaveButton();

    juce::Label titleLabel;
    juce::TextEditor textInput;
    std::function<void(const juce::String&)> callback;
    std::unique_ptr<juce::ComboBox> bankCombo;
    std::unique_ptr<juce::TextEditor> newBankInput;
    std::vector<juce::String> bankOptions;
    juce::Rectangle<int> bankLabelArea;
};

class StardustEditor;

class SettingsPanel : public juce::Component
{
public:
    SettingsPanel(StardustEditor& editor);
    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    StardustEditor& editorRef;

    enum class Tab { General, About };
    Tab currentTab = Tab::General;

    juce::TextButton generalTabBtn, aboutTabBtn;
    juce::TextButton closeBtn;

    // General tab
    juce::ComboBox scaleCombo;
    juce::TextButton resetFactoryBtn;

    // About tab — drawn in paint()

    void selectTab(Tab tab);
};

class StardustEditor : public juce::AudioProcessorEditor,
                       private juce::Timer,
                       private juce::AudioProcessorValueTreeState::Listener
{
public:
    explicit StardustEditor(StardustProcessor& p);
    ~StardustEditor() override;

    void paint(juce::Graphics& g) override;
    void paintOverChildren(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseMove(const juce::MouseEvent& e) override;
    void timerCallback() override;
    void parameterChanged(const juce::String& parameterID, float newValue) override;

    void applyScale(float scale);
    void resetToFactory();
    void hideSettings();
    float getCurrentScale() const { return currentScale; }
    StardustProcessor& getProcessor() { return processorRef; }

private:
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using ComboBoxAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    struct LabeledKnob
    {
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<SliderAttachment> attachment;
    };

    void setupKnob(LabeledKnob& knob, const juce::String& paramId, const juce::String& labelText);
    void layoutKnobInBounds(LabeledKnob& knob, juce::Rectangle<int> bounds);
    void updateDoubleClickDefaults();
    std::map<juce::String, LabeledKnob*> paramToKnob;

    StardustProcessor& processorRef;
    StardustLookAndFeel lookAndFeel;

    StarfieldBackground starfield;
    juce::ComboBox presetSelector;
    juce::TextButton prevPresetBtn, nextPresetBtn, savePresetBtn, deletePresetBtn, scaleBtn;
    juce::TextButton libraryBtn, favoriteBtn;
    std::unique_ptr<PresetLibraryPanel> presetLibraryPanel;
    void refreshPresetList();
    void showPresetLibrary();
    void hidePresetLibrary();
    void showPresetDropdown();
    void updateFavoriteButton();

    LabeledKnob destroyInKnob, cutoffKnob, destroyOutKnob, destroyMixKnob, filterLfoKnob;
    juce::Slider destroyFader;
    std::unique_ptr<SliderAttachment> destroyFaderAttachment;
    LabeledKnob grainMixKnob, grainCloudKnob, grainDriftKnob, grainSpaceKnob, grainMorphKnob;
    LabeledKnob chorusMixKnob, chorusSpeedKnob;
    LabeledKnob panOuterKnob, panInnerKnob;
    LabeledKnob tapeWowKnob, tapeFlutterKnob, tapeHissKnob;

    LabeledKnob inputGainKnob, outputGainKnob, masterMixKnob;


    juce::ToggleButton destroyToggle, granularToggle, multiplyToggle, tapeToggle, grainFreezeToggle;
    juce::Label freezeLabel;
    std::unique_ptr<ButtonAttachment> destroyToggleAttach, granularToggleAttach, multiplyToggleAttach, tapeToggleAttach, grainFreezeAttach;

    // Meters removed — replaced by signal flow display + knobs

    juce::Rectangle<int> controlsBounds;
    juce::Rectangle<int> galaxyBounds;
    juce::Rectangle<int> screenBounds;
    juce::Rectangle<int> bottomBarBounds;
    juce::Image logoImage;
    juce::Image backgroundTexture;
    void generateBackgroundTexture();
    float currentScale = 1.0f;

    std::unique_ptr<StardustDialog> activeDialog;
    std::unique_ptr<SettingsPanel> settingsPanel;
    void showSettings();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StardustEditor)
};
