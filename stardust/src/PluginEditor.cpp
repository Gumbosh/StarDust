#include "PluginEditor.h"
#include "BinaryData.h"

// ============================================================================
// StarDustLookAndFeel — Orbital Galaxy Style
// ============================================================================

StarDustLookAndFeel::StarDustLookAndFeel()
{
    setColour(juce::Slider::rotarySliderFillColourId, kAccent);
    setColour(juce::Slider::thumbColourId, kAccent);
    setColour(juce::Label::textColourId, kFgDim);
    setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xFF0A0A0A));
    setColour(juce::ComboBox::textColourId, kFgDim);
    setColour(juce::ComboBox::outlineColourId, kFgGhost.withAlpha(0.35f));
    setColour(juce::ComboBox::arrowColourId, kFgGhost);
    setColour(juce::PopupMenu::backgroundColourId, juce::Colour(0xFF0E0E0E));
    setColour(juce::PopupMenu::textColourId, kFgDim);
    setColour(juce::PopupMenu::highlightedBackgroundColourId, kAccent.withAlpha(0.08f));
    setColour(juce::PopupMenu::highlightedTextColourId, kAccent);
}

void StarDustLookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y,
                                            int width, int height,
                                            float sliderPos,
                                            float rotaryStartAngle,
                                            float rotaryEndAngle,
                                            juce::Slider& /*slider*/)
{
    const float radius = static_cast<float>(juce::jmin(width, height)) * 0.36f;
    const float centreX = static_cast<float>(x) + static_cast<float>(width) * 0.5f;
    const float centreY = static_cast<float>(y) + static_cast<float>(height) * 0.5f;
    const float angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);

    // Background track arc (full range) — visible enough to show full range
    {
        juce::Path bgArc;
        bgArc.addCentredArc(centreX, centreY, radius, radius, 0.0f,
                             rotaryStartAngle, rotaryEndAngle, true);
        g.setColour(kFgGhost.withAlpha(0.35f));
        g.strokePath(bgArc, juce::PathStrokeType(2.5f, juce::PathStrokeType::curved,
                                                   juce::PathStrokeType::rounded));
    }

    // Value arc (from start to current)
    {
        juce::Path valueArc;
        valueArc.addCentredArc(centreX, centreY, radius, radius, 0.0f,
                                rotaryStartAngle, angle, true);
        g.setColour(kAccent.withAlpha(0.7f));
        g.strokePath(valueArc, juce::PathStrokeType(2.5f, juce::PathStrokeType::curved,
                                                     juce::PathStrokeType::rounded));
    }

    // Tick marks at start, center, end
    const float tickOuter = radius + 5.0f;
    const float tickInner = radius + 2.0f;
    g.setColour(kFgGhost.withAlpha(0.25f));
    for (int i = 0; i < 3; ++i)
    {
        const float tickAngle = rotaryStartAngle
            + static_cast<float>(i) * 0.5f * (rotaryEndAngle - rotaryStartAngle);
        g.drawLine(centreX + tickInner * std::sin(tickAngle),
                   centreY - tickInner * std::cos(tickAngle),
                   centreX + tickOuter * std::sin(tickAngle),
                   centreY - tickOuter * std::cos(tickAngle), 0.7f);
    }

    // Pointer line from center outward
    const float lineInner = radius * 0.25f;
    const float lineOuter = radius * 0.75f;
    juce::Path pointer;
    pointer.startNewSubPath(centreX + lineInner * std::sin(angle),
                             centreY - lineInner * std::cos(angle));
    pointer.lineTo(centreX + lineOuter * std::sin(angle),
                   centreY - lineOuter * std::cos(angle));
    g.setColour(kAccent);
    g.strokePath(pointer, juce::PathStrokeType(1.8f, juce::PathStrokeType::curved,
                                                juce::PathStrokeType::rounded));

    // Center dot
    g.setColour(kFgGhost.withAlpha(0.4f));
    g.fillEllipse(centreX - 2.0f, centreY - 2.0f, 4.0f, 4.0f);

    // Bright dot at arc tip
    const float dotX = centreX + radius * std::sin(angle);
    const float dotY = centreY - radius * std::cos(angle);
    g.setColour(kAccent.withAlpha(0.15f));
    g.fillEllipse(dotX - 5.0f, dotY - 5.0f, 10.0f, 10.0f);
    g.setColour(kAccent);
    g.fillEllipse(dotX - 2.5f, dotY - 2.5f, 5.0f, 5.0f);
}

void StarDustLookAndFeel::drawLinearSlider(juce::Graphics& g, int x, int y,
                                            int width, int height,
                                            float sliderPos,
                                            float /*minSliderPos*/,
                                            float /*maxSliderPos*/,
                                            juce::Slider::SliderStyle style,
                                            juce::Slider& /*slider*/)
{
    if (style != juce::Slider::LinearHorizontal)
        return;

    const float trackY = static_cast<float>(y) + static_cast<float>(height) * 0.5f;
    const float trackLeft = static_cast<float>(x) + 6.0f;
    const float trackRight = static_cast<float>(x + width) - 6.0f;
    const float trackWidth = trackRight - trackLeft;
    const float centre = trackLeft + trackWidth * 0.5f;

    // Background track
    g.setColour(kFgGhost.withAlpha(0.18f));
    g.fillRoundedRectangle(trackLeft, trackY - 2.0f, trackWidth, 4.0f, 2.0f);

    // Value fill from centre — bright arc style
    const float fillStart = std::min(sliderPos, centre);
    const float fillEnd = std::max(sliderPos, centre);
    g.setColour(kAccent.withAlpha(0.6f));
    g.fillRoundedRectangle(fillStart, trackY - 2.0f, fillEnd - fillStart, 4.0f, 2.0f);

    // Centre notch — taller, visible
    g.setColour(kFgDim.withAlpha(0.5f));
    g.fillRect(centre - 0.5f, trackY - 6.0f, 1.0f, 12.0f);

    // Tick marks at 25% and 75%
    g.setColour(kFgGhost.withAlpha(0.2f));
    for (float frac : { 0.25f, 0.75f })
    {
        const float tx = trackLeft + trackWidth * frac;
        g.fillRect(tx - 0.5f, trackY - 4.0f, 1.0f, 8.0f);
    }

    // Thumb — rounded rectangle with glow
    const float thumbW = 10.0f;
    const float thumbH = 16.0f;
    const float thumbX = sliderPos - thumbW * 0.5f;
    const float thumbY2 = trackY - thumbH * 0.5f;

    // Glow
    g.setColour(kAccent.withAlpha(0.08f));
    g.fillRoundedRectangle(thumbX - 3.0f, thumbY2 - 3.0f, thumbW + 6.0f, thumbH + 6.0f, 5.0f);

    // Body
    g.setColour(juce::Colour(0xFF151515));
    g.fillRoundedRectangle(thumbX, thumbY2, thumbW, thumbH, 3.0f);

    // Border
    g.setColour(kFgGhost.withAlpha(0.4f));
    g.drawRoundedRectangle(thumbX, thumbY2, thumbW, thumbH, 3.0f, 1.0f);

    // Center line on thumb
    g.setColour(kAccent.withAlpha(0.7f));
    g.fillRect(sliderPos - 0.5f, thumbY2 + 3.0f, 1.0f, thumbH - 6.0f);
}

void StarDustLookAndFeel::drawToggleButton(juce::Graphics& g, juce::ToggleButton& button,
                                            bool shouldDrawButtonAsHighlighted,
                                            bool /*shouldDrawButtonAsDown*/)
{
    auto bounds = button.getLocalBounds().toFloat().reduced(1.0f);
    const float h = bounds.getHeight();
    const float w = h * 1.6f; // pill aspect ratio
    const float pillX = bounds.getCentreX() - w * 0.5f;
    const float pillY = bounds.getCentreY() - h * 0.5f;
    const float radius = h * 0.5f;
    const float knobSize = h - 4.0f;
    const bool on = button.getToggleState();

    // Track background
    g.setColour(on ? kAccent.withAlpha(0.25f) : kFgGhost.withAlpha(0.2f));
    g.fillRoundedRectangle(pillX, pillY, w, h, radius);

    // Track border
    g.setColour(on ? kAccent.withAlpha(0.6f) : kFgGhost.withAlpha(0.4f));
    g.drawRoundedRectangle(pillX, pillY, w, h, radius, 1.0f);

    // Sliding knob
    const float knobX = on ? (pillX + w - knobSize - 2.0f) : (pillX + 2.0f);
    const float knobY = pillY + (h - knobSize) * 0.5f;

    if (on)
    {
        // Glow
        g.setColour(kAccent.withAlpha(0.15f));
        g.fillEllipse(knobX - 1.0f, knobY - 1.0f, knobSize + 2.0f, knobSize + 2.0f);
    }

    g.setColour(on ? kAccent : kFgGhost.withAlpha(0.6f));
    g.fillEllipse(knobX, knobY, knobSize, knobSize);

    // Hover highlight
    if (shouldDrawButtonAsHighlighted)
    {
        g.setColour(kAccent.withAlpha(0.05f));
        g.fillRoundedRectangle(pillX, pillY, w, h, radius);
    }
}

// ============================================================================
// LevelMeter
// ============================================================================

LevelMeter::LevelMeter(std::atomic<float>& levelSource) : level(levelSource)
{
    startTimerHz(30);
}

void LevelMeter::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    g.setColour(StarDustLookAndFeel::kInset);
    g.fillRoundedRectangle(bounds, 1.0f);

    const float meterHeight = bounds.getHeight() * displayLevel;
    auto meterArea = bounds;
    auto meterBounds = meterArea.removeFromBottom(meterHeight);

    if (displayLevel < 0.7f)
        g.setColour(StarDustLookAndFeel::kFgGhost.brighter(0.3f));
    else if (displayLevel < 0.9f)
        g.setColour(StarDustLookAndFeel::kFgDim);
    else
        g.setColour(StarDustLookAndFeel::kAccent);

    g.fillRoundedRectangle(meterBounds, 1.0f);

    // Peak hold indicator
    if (peakLevel > 0.01f)
    {
        const float peakY = bounds.getBottom() - bounds.getHeight() * peakLevel;
        g.setColour(StarDustLookAndFeel::kAccent.withAlpha(0.8f));
        g.fillRect(bounds.getX(), peakY, bounds.getWidth(), 1.0f);
    }
}

void LevelMeter::timerCallback()
{
    const float newLevel = juce::jlimit(0.0f, 1.0f, level.load());
    displayLevel = (newLevel > displayLevel) ? newLevel : displayLevel * 0.92f;

    // Peak hold
    if (newLevel > peakLevel)
    {
        peakLevel = newLevel;
        peakHoldCounter = kPeakHoldFrames;
    }
    else if (peakHoldCounter > 0)
    {
        --peakHoldCounter;
    }
    else
    {
        peakLevel *= 0.95f;
    }

    repaint();
}

// ============================================================================
// StarDustEditor
// ============================================================================

StarDustEditor::StarDustEditor(StarDustProcessor& p)
    : AudioProcessorEditor(&p),
      processorRef(p),
      starfield(p.apvts, p.outputLevelLeft, p.outputLevelRight),
      inputMeterL(p.inputLevelLeft),
      inputMeterR(p.inputLevelRight),
      outputMeterL(p.outputLevelLeft),
      outputMeterR(p.outputLevelRight)
{
    setLookAndFeel(&lookAndFeel);
    setSize(560, 560);

    addAndMakeVisible(starfield);
    starfield.setExcludeRect({});
    starfield.toBack();

    setupKnob(bitsKnob, "bitDepth", "BITS");
    setupKnob(rateKnob, "sampleRate", "RATE");
    setupKnob(cutoffKnob, "filterCutoff", "CUTOFF");
    setupKnob(driveKnob, "drive", "DRIVE");
    setupKnob(mixKnob, "mix", "DRY/WET");

    setupKnob(grainMixKnob, "grainMix", "GRAIN");
    setupKnob(grainDensityKnob, "grainDensity", "DENSITY");
    setupKnob(grainSizeKnob, "grainSize", "SIZE");
    setupKnob(grainScatterKnob, "grainScatter", "SCATTER");
    setupKnob(widthKnob, "stereoWidth", "WIDTH");

    setupKnob(chorusMixKnob, "chorusMix", "MULTIPLY");

    tuneFader.setSliderStyle(juce::Slider::LinearHorizontal);
    tuneFader.setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);
    tuneAttachment = std::make_unique<SliderAttachment>(processorRef.apvts, "grainTune", tuneFader);
    tuneFader.onValueChange = [this] {
        const float val = static_cast<float>(tuneFader.getValue());
        tuneValueLabel.setText(
            (val >= 0 ? "+" : "") + juce::String(val, 1) + " st",
            juce::dontSendNotification);
    };
    addAndMakeVisible(tuneFader);

    tuneLabel.setText("PITCH", juce::dontSendNotification);
    tuneLabel.setJustificationType(juce::Justification::centredLeft);
    tuneLabel.setFont(juce::FontOptions(11.0f).withStyle("Bold"));
    tuneLabel.setColour(juce::Label::textColourId, StarDustLookAndFeel::kFgGhost);
    addAndMakeVisible(tuneLabel);

    tuneValueLabel.setText("0.0 st", juce::dontSendNotification);
    tuneValueLabel.setJustificationType(juce::Justification::centredRight);
    tuneValueLabel.setFont(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(), 11.0f, juce::Font::plain));
    tuneValueLabel.setColour(juce::Label::textColourId, StarDustLookAndFeel::kFgDim);
    addAndMakeVisible(tuneValueLabel);

    int presetIdx = 1;
    for (const auto& preset : processorRef.getFactoryPresets())
        presetSelector.addItem(preset.name, presetIdx++);
    presetSelector.setSelectedId(processorRef.getCurrentProgram() + 1, juce::dontSendNotification);
    presetSelector.onChange = [this] {
        processorRef.loadPreset(presetSelector.getSelectedId() - 1);
    };
    addAndMakeVisible(presetSelector);

    // Section toggle buttons — minimal dot style
    auto setupToggle = [this](juce::ToggleButton& btn, std::unique_ptr<ButtonAttachment>& attach,
                              const juce::String& paramId) {
        btn.setButtonText("");
        attach = std::make_unique<ButtonAttachment>(processorRef.apvts, paramId, btn);
        addAndMakeVisible(btn);
    };
    setupToggle(destroyToggle, destroyToggleAttach, "destroyEnabled");
    setupToggle(granularToggle, granularToggleAttach, "granularEnabled");
    setupToggle(multiplyToggle, multiplyToggleAttach, "multiplyEnabled");

    addAndMakeVisible(inputMeterL);
    addAndMakeVisible(inputMeterR);
    addAndMakeVisible(outputMeterL);
    addAndMakeVisible(outputMeterR);

    logoImage = juce::ImageCache::getFromMemory(BinaryData::logo_png, BinaryData::logo_pngSize);
}

StarDustEditor::~StarDustEditor()
{
    setLookAndFeel(nullptr);
}

void StarDustEditor::setupKnob(LabeledKnob& knob, const juce::String& paramId,
                               const juce::String& labelText)
{
    knob.slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    knob.slider.setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);
    knob.attachment = std::make_unique<SliderAttachment>(processorRef.apvts, paramId, knob.slider);
    addAndMakeVisible(knob.slider);

    knob.label.setText(labelText, juce::dontSendNotification);
    knob.label.setJustificationType(juce::Justification::centred);
    knob.label.setFont(juce::FontOptions(9.0f).withStyle("Bold"));
    knob.label.setColour(juce::Label::textColourId, StarDustLookAndFeel::kFgDim);
    addAndMakeVisible(knob.label);
}

void StarDustEditor::layoutKnobInBounds(LabeledKnob& knob, juce::Rectangle<int> bounds)
{
    const auto labelHeight = 14;
    knob.slider.setBounds(bounds.removeFromTop(bounds.getHeight() - labelHeight));
    knob.label.setBounds(bounds);
}

void StarDustEditor::paint(juce::Graphics& g)
{
    // Static black background
    g.fillAll(juce::Colours::black);

    // ---- Controls panel — rounded border matching viewport style ----
    const auto cpf = controlsBounds.toFloat();

    g.setColour(juce::Colours::black);
    g.fillRoundedRectangle(cpf, 4.0f);
    g.setColour(StarDustLookAndFeel::kFgGhost.withAlpha(0.35f));
    g.drawRoundedRectangle(cpf, 4.0f, 2.0f);

    // Title: "S T A R D U S T" with letter spacing — top-left of panel
    g.setFont(juce::FontOptions(14.0f).withStyle("Bold"));
    g.setColour(StarDustLookAndFeel::kAccent.withAlpha(0.05f));
    g.drawText("S T A R D U S T", cpf.getX() + 13, cpf.getY() + 4, 200, 16, juce::Justification::centredLeft);
    g.setColour(StarDustLookAndFeel::kAccent.withAlpha(0.8f));
    g.drawText("S T A R D U S T", cpf.getX() + 12, cpf.getY() + 3, 200, 16, juce::Justification::centredLeft);

    // Section labels
    g.setFont(juce::FontOptions(8.0f).withStyle("Bold"));
    g.setColour(StarDustLookAndFeel::kFgDim.withAlpha(0.6f));

    const int innerX = controlsBounds.getX() + 14;
    const int labelOffset = 22; // space for toggle on the left
    g.drawText("DESTROY", innerX + labelOffset, controlsBounds.getY() + 50, 55, 10, juce::Justification::centredLeft);

    // Divider between sections
    const float divY = cpf.getY() + 166.0f;
    g.setColour(StarDustLookAndFeel::kFgGhost.withAlpha(0.2f));
    g.drawHorizontalLine(static_cast<int>(divY), cpf.getX() + 12.0f, cpf.getRight() - 12.0f);

    g.setFont(juce::FontOptions(8.0f).withStyle("Bold"));
    g.setColour(StarDustLookAndFeel::kFgDim.withAlpha(0.6f));
    g.drawText("GRANULAR", innerX + labelOffset, controlsBounds.getY() + 169, 65, 10, juce::Justification::centredLeft);

    // Divider before MULTIPLY
    const float div2Y = cpf.getY() + 250.0f;
    g.setColour(StarDustLookAndFeel::kFgGhost.withAlpha(0.2f));
    g.drawHorizontalLine(static_cast<int>(div2Y), cpf.getX() + 12.0f, cpf.getRight() - 12.0f);

    g.setFont(juce::FontOptions(8.0f).withStyle("Bold"));
    g.setColour(StarDustLookAndFeel::kFgDim.withAlpha(0.6f));
    g.drawText("MULTIPLY", innerX + labelOffset, controlsBounds.getY() + 253, 65, 10, juce::Justification::centredLeft);

    // ---- Galaxy viewport: VHS display with padded visuals + depth lines ----
    const auto gvf = galaxyBounds.toFloat();
    const auto screenf = screenBounds.toFloat();

    // 1. Fill black background
    g.setColour(juce::Colours::black);
    g.fillRoundedRectangle(gvf, 4.0f);

    // 2. Fill padding trapezoids (black, before any borders)
    {
        g.setColour(juce::Colours::black);

        juce::Path topTrap;
        topTrap.startNewSubPath(gvf.getX(), gvf.getY());
        topTrap.lineTo(gvf.getRight(), gvf.getY());
        topTrap.lineTo(screenf.getRight(), screenf.getY());
        topTrap.lineTo(screenf.getX(), screenf.getY());
        topTrap.closeSubPath();
        g.fillPath(topTrap);

        juce::Path botTrap;
        botTrap.startNewSubPath(screenf.getX(), screenf.getBottom());
        botTrap.lineTo(screenf.getRight(), screenf.getBottom());
        botTrap.lineTo(gvf.getRight(), gvf.getBottom());
        botTrap.lineTo(gvf.getX(), gvf.getBottom());
        botTrap.closeSubPath();
        g.fillPath(botTrap);

        juce::Path leftTrap;
        leftTrap.startNewSubPath(gvf.getX(), gvf.getY());
        leftTrap.lineTo(screenf.getX(), screenf.getY());
        leftTrap.lineTo(screenf.getX(), screenf.getBottom());
        leftTrap.lineTo(gvf.getX(), gvf.getBottom());
        leftTrap.closeSubPath();
        g.fillPath(leftTrap);

        juce::Path rightTrap;
        rightTrap.startNewSubPath(screenf.getRight(), screenf.getY());
        rightTrap.lineTo(gvf.getRight(), gvf.getY());
        rightTrap.lineTo(gvf.getRight(), gvf.getBottom());
        rightTrap.lineTo(screenf.getRight(), screenf.getBottom());
        rightTrap.closeSubPath();
        g.fillPath(rightTrap);
    }

    // 3. Draw ALL borders and lines ON TOP of fills
    const auto borderCol = StarDustLookAndFeel::kFgGhost.withAlpha(0.35f);

    // Outer border
    g.setColour(borderCol);
    g.drawRoundedRectangle(gvf, 4.0f, 2.0f);

    // Depth lines
    g.drawLine(gvf.getX(), gvf.getY(), screenf.getX(), screenf.getY(), 2.0f);
    g.drawLine(gvf.getRight(), gvf.getY(), screenf.getRight(), screenf.getY(), 2.0f);
    g.drawLine(gvf.getX(), gvf.getBottom(), screenf.getX(), screenf.getBottom(), 2.0f);
    g.drawLine(gvf.getRight(), gvf.getBottom(), screenf.getRight(), screenf.getBottom(), 2.0f);

    // ---- Bottom bar — white strip with logo+name left, version right ----
    const auto bbf = bottomBarBounds.toFloat();

    g.setColour(StarDustLookAndFeel::kAccent);
    g.fillRoundedRectangle(bbf, 3.0f);

    // Logo + name on left
    g.setColour(juce::Colours::black);
    const float logoH = bbf.getHeight() - 4.0f;
    const float logoW = logoImage.isValid()
        ? logoH * (static_cast<float>(logoImage.getWidth()) / static_cast<float>(logoImage.getHeight()))
        : 0.0f;

    if (logoImage.isValid())
    {
        g.drawImage(logoImage, bbf.getX() + 8, bbf.getY() + 2, static_cast<int>(logoW), static_cast<int>(logoH),
                    0, 0, logoImage.getWidth(), logoImage.getHeight());
    }

    g.setFont(juce::FontOptions(12.0f).withStyle("Bold"));
    g.drawText("LoudDealers", bbf.getX() + 12 + logoW, bbf.getY(), bbf.getWidth() * 0.5f, bbf.getHeight(),
               juce::Justification::centredLeft);

    // Version on right
    g.setFont(juce::FontOptions(10.0f).withStyle("Bold"));
    g.drawText("v" STARDUST_VERSION, bbf.getRight() - 60, bbf.getY(), 50, bbf.getHeight(),
               juce::Justification::centredRight);
}

void StarDustEditor::paintOverChildren(juce::Graphics& g)
{
    // Draw inner screen border ON TOP of the starfield child component
    const auto screenf = screenBounds.toFloat();
    const auto gvf = galaxyBounds.toFloat();

    g.setColour(StarDustLookAndFeel::kFgGhost.withAlpha(0.35f));
    g.drawRoundedRectangle(screenf, 3.0f, 2.0f);

    // IN/OUT labels in the padding gaps
    g.setFont(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(), 7.0f, juce::Font::bold));
    g.setColour(StarDustLookAndFeel::kAccent);

    // "IN" label above left meters
    g.drawText("IN", static_cast<int>(gvf.getX()) + 4, screenBounds.getY() + 4, 20, 10, juce::Justification::centred);

    // "OUT" label above right meters
    g.drawText("OUT", galaxyBounds.getRight() - 24, screenBounds.getY() + 4, 20, 10, juce::Justification::centred);

}

void StarDustEditor::mouseDown(const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu())
    {
        juce::PopupMenu menu;
        menu.addSectionHeader("UI Scale");
        menu.addItem(1, "100%", true, currentScale < 1.1f);
        menu.addItem(2, "125%", true, currentScale >= 1.1f && currentScale < 1.4f);
        menu.addItem(3, "150%", true, currentScale >= 1.4f);

        menu.showMenuAsync(juce::PopupMenu::Options(), [this](int result) {
            if (result == 0) return;
            const float scale = (result == 1) ? 1.0f : (result == 2) ? 1.25f : 1.5f;
            currentScale = scale;
            setScaleFactor(scale);
        });
        return;
    }

    AudioProcessorEditor::mouseDown(e);
}

void StarDustEditor::resized()
{
    const int margin = 8;
    const int gap = 6;

    // Controls panel
    const int panelW = getWidth() - margin * 2;
    const int panelH = 330;
    controlsBounds = { margin, margin, panelW, panelH };

    // Bottom bar
    const int barH = 22;
    bottomBarBounds = { margin, getHeight() - margin - barH, panelW, barH };

    // Galaxy viewport: between controls and bottom bar
    const int galaxyY = controlsBounds.getBottom() + gap;
    const int galaxyH = bottomBarBounds.getY() - gap - galaxyY;
    galaxyBounds = { margin, galaxyY, panelW, galaxyH };

    // Screen bounds: padded area inside viewport where visuals render
    const int pad = 28;
    screenBounds = galaxyBounds.reduced(pad);

    // Starfield renders only in the screen area
    starfield.setBounds(screenBounds);
    starfield.setExcludeRect({});

    // Preset selector top-right of panel
    presetSelector.setBounds(controlsBounds.getRight() - 150, controlsBounds.getY() + 3, 140, 16);

    // Shared grid: 5 columns, equal spacing, both rows use same X positions
    const int knobW = 87;
    const int knobH = 68;
    const int numCols = 5;
    const int totalW = knobW * numCols;
    const int gridX = controlsBounds.getX() + (controlsBounds.getWidth() - totalW) / 2;

    // DESTROY section — row 1 knobs
    const int row1Y = controlsBounds.getY() + 62;

    layoutKnobInBounds(bitsKnob,   { gridX,              row1Y, knobW, knobH });
    layoutKnobInBounds(rateKnob,   { gridX + knobW,      row1Y, knobW, knobH });
    layoutKnobInBounds(cutoffKnob, { gridX + knobW * 2,  row1Y, knobW, knobH });
    layoutKnobInBounds(driveKnob,  { gridX + knobW * 3,  row1Y, knobW, knobH });
    layoutKnobInBounds(mixKnob,    { gridX + knobW * 4,  row1Y, knobW, knobH });

    // PITCH slider — in DESTROY section, below knobs
    const int tunePad = 16;
    const int pitchY = controlsBounds.getY() + 132;
    tuneLabel.setBounds(gridX + tunePad, pitchY, 42, 14);
    tuneValueLabel.setBounds(gridX + totalW - tunePad - 60, pitchY, 60, 14);
    tuneFader.setBounds(gridX + tunePad, pitchY + 13, totalW - tunePad * 2, 18);

    // GRANULAR section — grain knobs
    const int grainRowY = controlsBounds.getY() + 182;

    layoutKnobInBounds(grainMixKnob,     { gridX,              grainRowY, knobW, knobH });
    layoutKnobInBounds(grainDensityKnob, { gridX + knobW,      grainRowY, knobW, knobH });
    layoutKnobInBounds(grainSizeKnob,    { gridX + knobW * 2,  grainRowY, knobW, knobH });
    layoutKnobInBounds(grainScatterKnob, { gridX + knobW * 3,  grainRowY, knobW, knobH });
    layoutKnobInBounds(widthKnob,        { gridX + knobW * 4,  grainRowY, knobW, knobH });

    // MULTIPLY section — single knob, centered
    const int multiplyY = controlsBounds.getY() + 264;
    layoutKnobInBounds(chorusMixKnob, { gridX + knobW * 2, multiplyY, knobW, knobH });

    // Section toggle buttons (left of section labels)
    const int toggleH = 12;
    const int toggleW = 20;
    const int tInnerX = controlsBounds.getX() + 12;
    destroyToggle.setBounds(tInnerX, controlsBounds.getY() + 49, toggleW, toggleH);
    granularToggle.setBounds(tInnerX, controlsBounds.getY() + 168, toggleW, toggleH);
    multiplyToggle.setBounds(tInnerX, controlsBounds.getY() + 252, toggleW, toggleH);

    // Meters in the left/right padding gaps
    const int meterPadX = 8;
    const int meterPadY = 18;
    const int meterH = screenBounds.getHeight() - meterPadY * 2;
    const int meterW = 4;

    // Left gap: IN meters
    inputMeterL.setBounds(galaxyBounds.getX() + meterPadX, screenBounds.getY() + meterPadY, meterW, meterH);
    inputMeterR.setBounds(galaxyBounds.getX() + meterPadX + meterW + 3, screenBounds.getY() + meterPadY, meterW, meterH);

    // Right gap: OUT meters
    outputMeterL.setBounds(galaxyBounds.getRight() - meterPadX - meterW * 2 - 3, screenBounds.getY() + meterPadY, meterW, meterH);
    outputMeterR.setBounds(galaxyBounds.getRight() - meterPadX - meterW, screenBounds.getY() + meterPadY, meterW, meterH);
}
