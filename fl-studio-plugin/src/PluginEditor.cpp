#include "PluginEditor.h"

// ============================================================================
// StarDustLookAndFeel — Vintage Hardware Style
// ============================================================================

StarDustLookAndFeel::StarDustLookAndFeel()
{
    setColour(juce::Slider::rotarySliderFillColourId, kAccent);
    setColour(juce::Slider::thumbColourId, kAccent);
    setColour(juce::Label::textColourId, kLabel);
    setColour(juce::ComboBox::backgroundColourId, kPanelLight);
    setColour(juce::ComboBox::textColourId, kTextBright);
    setColour(juce::ComboBox::outlineColourId, kKnobRing);
    setColour(juce::PopupMenu::backgroundColourId, kPanelLight);
    setColour(juce::PopupMenu::textColourId, kTextBright);
    setColour(juce::PopupMenu::highlightedBackgroundColourId, kAccent.withAlpha(0.25f));
}

void StarDustLookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y,
                                            int width, int height,
                                            float sliderPos,
                                            float rotaryStartAngle,
                                            float rotaryEndAngle,
                                            juce::Slider& /*slider*/)
{
    const float radius = static_cast<float>(juce::jmin(width, height)) * 0.38f;
    const float centreX = static_cast<float>(x) + static_cast<float>(width) * 0.5f;
    const float centreY = static_cast<float>(y) + static_cast<float>(height) * 0.5f;
    const float angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);

    // Shadow
    g.setColour(juce::Colour(0x30000000));
    g.fillEllipse(centreX - radius + 1, centreY - radius + 2, radius * 2.0f, radius * 2.0f);

    // Outer ring (knurled edge feel)
    g.setColour(kKnobRing);
    g.fillEllipse(centreX - radius - 2, centreY - radius - 2,
                   (radius + 2) * 2.0f, (radius + 2) * 2.0f);

    // Knob body with subtle gradient
    {
        juce::ColourGradient grad(kKnobHighlight, centreX, centreY - radius,
                                   kKnobBody, centreX, centreY + radius, false);
        g.setGradientFill(grad);
        g.fillEllipse(centreX - radius, centreY - radius, radius * 2.0f, radius * 2.0f);
    }

    // Tick marks around the knob
    const int numTicks = 11;
    const float tickRadius = radius + 7.0f;
    g.setColour(kLabel.withAlpha(0.3f));
    for (int i = 0; i < numTicks; ++i)
    {
        const float tickAngle = rotaryStartAngle
            + static_cast<float>(i) / static_cast<float>(numTicks - 1)
            * (rotaryEndAngle - rotaryStartAngle);
        const float tx1 = centreX + (tickRadius - 3.0f) * std::sin(tickAngle);
        const float ty1 = centreY - (tickRadius - 3.0f) * std::cos(tickAngle);
        const float tx2 = centreX + tickRadius * std::sin(tickAngle);
        const float ty2 = centreY - tickRadius * std::cos(tickAngle);
        g.drawLine(tx1, ty1, tx2, ty2, 1.0f);
    }

    // Pointer line (classic vintage indicator)
    const float lineStart = radius * 0.15f;
    const float lineEnd = radius * 0.75f;
    juce::Path pointer;
    pointer.startNewSubPath(centreX, centreY - lineStart);
    pointer.lineTo(centreX, centreY - lineEnd);
    pointer.applyTransform(juce::AffineTransform::rotation(angle, centreX, centreY));
    g.setColour(kAccent);
    g.strokePath(pointer, juce::PathStrokeType(2.5f, juce::PathStrokeType::curved,
                                                juce::PathStrokeType::rounded));

    // Centre cap
    const float capRadius = radius * 0.18f;
    g.setColour(kKnobRing);
    g.fillEllipse(centreX - capRadius, centreY - capRadius, capRadius * 2.0f, capRadius * 2.0f);
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
    const float trackLeft = static_cast<float>(x) + 4.0f;
    const float trackRight = static_cast<float>(x + width) - 4.0f;
    const float trackWidth = trackRight - trackLeft;
    const float centre = trackLeft + trackWidth * 0.5f;

    // Inset track groove
    g.setColour(kInset);
    g.fillRoundedRectangle(trackLeft - 1, trackY - 4.0f, trackWidth + 2, 8.0f, 4.0f);

    // Track surface
    g.setColour(kKnobBody.withAlpha(0.6f));
    g.fillRoundedRectangle(trackLeft, trackY - 3.0f, trackWidth, 6.0f, 3.0f);

    // Centre notch
    g.setColour(kAccent.withAlpha(0.4f));
    g.fillRect(centre - 0.5f, trackY - 5.0f, 1.0f, 10.0f);

    // Fill from centre to thumb
    const float fillStart = std::min(sliderPos, centre);
    const float fillEnd = std::max(sliderPos, centre);
    g.setColour(kAccent.withAlpha(0.6f));
    g.fillRoundedRectangle(fillStart, trackY - 2.0f, fillEnd - fillStart, 4.0f, 2.0f);

    // Thumb — chunky vintage fader cap
    const float thumbW = 18.0f;
    const float thumbH = 24.0f;
    const float thumbX = sliderPos - thumbW * 0.5f;
    const float thumbY = trackY - thumbH * 0.5f;

    // Shadow
    g.setColour(juce::Colour(0x30000000));
    g.fillRoundedRectangle(thumbX + 1, thumbY + 2, thumbW, thumbH, 3.0f);

    // Thumb body gradient
    {
        juce::ColourGradient grad(kKnobHighlight, thumbX, thumbY,
                                   kKnobBody, thumbX, thumbY + thumbH, false);
        g.setGradientFill(grad);
        g.fillRoundedRectangle(thumbX, thumbY, thumbW, thumbH, 3.0f);
    }

    // Thumb grip lines
    g.setColour(kKnobRing);
    for (float ly = thumbY + 7.0f; ly < thumbY + thumbH - 6.0f; ly += 3.0f)
        g.drawHorizontalLine(static_cast<int>(ly), thumbX + 4.0f, thumbX + thumbW - 4.0f);

    // Centre indicator on thumb
    g.setColour(kAccent);
    g.fillRect(sliderPos - 0.5f, thumbY + 3.0f, 1.0f, thumbH - 6.0f);
}

// ============================================================================
// LevelMeter — Vintage amber VU style
// ============================================================================

LevelMeter::LevelMeter(std::atomic<float>& levelSource) : level(levelSource)
{
    startTimerHz(30);
}

void LevelMeter::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    g.setColour(StarDustLookAndFeel::kInset);
    g.fillRoundedRectangle(bounds, 2.0f);

    const float meterHeight = bounds.getHeight() * displayLevel;
    auto meterBounds = bounds.removeFromBottom(meterHeight);

    // Amber VU colors
    if (displayLevel < 0.7f)
        g.setColour(StarDustLookAndFeel::kAccent);
    else if (displayLevel < 0.9f)
        g.setColour(juce::Colour(0xFFCC8800));
    else
        g.setColour(juce::Colour(0xFFCC4422));

    g.fillRoundedRectangle(meterBounds, 2.0f);
}

void LevelMeter::timerCallback()
{
    const float newLevel = juce::jlimit(0.0f, 1.0f, level.load());
    displayLevel = (newLevel > displayLevel) ? newLevel : displayLevel * 0.92f;
    repaint();
}

// ============================================================================
// StarDustEditor
// ============================================================================

StarDustEditor::StarDustEditor(StarDustProcessor& p)
    : AudioProcessorEditor(&p),
      processorRef(p),
      starfield(p.apvts),
      inputMeterL(p.inputLevelLeft),
      inputMeterR(p.inputLevelRight),
      outputMeterL(p.outputLevelLeft),
      outputMeterR(p.outputLevelRight)
{
    setLookAndFeel(&lookAndFeel);
    setSize(540, 440);

    addAndMakeVisible(starfield);
    starfield.toBack();

    setupKnob(bitsKnob, "bitDepth", "BITS");
    setupKnob(rateKnob, "sampleRate", "RATE");
    setupKnob(cutoffKnob, "filterCutoff", "CUTOFF");
    setupKnob(driveKnob, "drive", "DRIVE");
    setupKnob(mixKnob, "mix", "MIX");

    setupKnob(grainMixKnob, "grainMix", "MIX");
    setupKnob(grainDensityKnob, "grainDensity", "DENSITY");
    setupKnob(grainSizeKnob, "grainSize", "SIZE");
    setupKnob(grainScatterKnob, "grainScatter", "SCATTER");
    setupKnob(widthKnob, "stereoWidth", "WIDTH");

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

    tuneLabel.setText("TUNE", juce::dontSendNotification);
    tuneLabel.setJustificationType(juce::Justification::centredLeft);
    tuneLabel.setFont(juce::Font(10.0f, juce::Font::bold));
    tuneLabel.setColour(juce::Label::textColourId, StarDustLookAndFeel::kLabel);
    addAndMakeVisible(tuneLabel);

    tuneValueLabel.setText("0.0 st", juce::dontSendNotification);
    tuneValueLabel.setJustificationType(juce::Justification::centredRight);
    tuneValueLabel.setFont(juce::Font(juce::Font::getDefaultMonospacedFontName(), 10.0f, juce::Font::plain));
    tuneValueLabel.setColour(juce::Label::textColourId, StarDustLookAndFeel::kAccent);
    addAndMakeVisible(tuneValueLabel);

    int presetIdx = 1;
    for (const auto& preset : processorRef.getFactoryPresets())
        presetSelector.addItem(preset.name, presetIdx++);
    presetSelector.setSelectedId(processorRef.getCurrentProgram() + 1, juce::dontSendNotification);
    presetSelector.onChange = [this] {
        processorRef.loadPreset(presetSelector.getSelectedId() - 1);
    };
    addAndMakeVisible(presetSelector);

    addAndMakeVisible(inputMeterL);
    addAndMakeVisible(inputMeterR);
    addAndMakeVisible(outputMeterL);
    addAndMakeVisible(outputMeterR);
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
    knob.label.setFont(juce::Font(10.0f, juce::Font::bold));
    knob.label.setColour(juce::Label::textColourId, StarDustLookAndFeel::kLabel);
    addAndMakeVisible(knob.label);
}

void StarDustEditor::layoutKnobInBounds(LabeledKnob& knob, juce::Rectangle<int> bounds)
{
    const auto labelHeight = 16;
    knob.slider.setBounds(bounds.removeFromTop(bounds.getHeight() - labelHeight));
    knob.label.setBounds(bounds);
}

void StarDustEditor::drawScrew(juce::Graphics& g, float cx, float cy)
{
    const float r = 4.0f;
    g.setColour(StarDustLookAndFeel::kScrewHead);
    g.fillEllipse(cx - r, cy - r, r * 2.0f, r * 2.0f);
    g.setColour(juce::Colour(0x20000000));
    g.drawEllipse(cx - r, cy - r, r * 2.0f, r * 2.0f, 0.5f);
    // Cross slot
    g.setColour(StarDustLookAndFeel::kInset);
    g.drawLine(cx - 2.5f, cy, cx + 2.5f, cy, 1.0f);
    g.drawLine(cx, cy - 2.5f, cx, cy + 2.5f, 1.0f);
}

void StarDustEditor::paint(juce::Graphics& g)
{
    const auto w = static_cast<float>(getWidth());

    // ---- Main panel background (brushed metal feel) ----
    {
        juce::ColourGradient panelGrad(
            StarDustLookAndFeel::kPanel.brighter(0.08f), 0.0f, 0.0f,
            StarDustLookAndFeel::kPanel, 0.0f, static_cast<float>(getHeight()), false);
        g.setGradientFill(panelGrad);
        g.fillAll();
    }

    // Subtle horizontal brushed lines
    g.setColour(juce::Colour(0x06FFFFFF));
    for (int yy = 0; yy < getHeight(); yy += 2)
        g.drawHorizontalLine(yy, 0.0f, w);

    // ---- Header strip ----
    const int headerH = 36;
    g.setColour(StarDustLookAndFeel::kPanelLight);
    g.fillRect(0, 0, getWidth(), headerH);
    // Bottom edge bevel
    g.setColour(juce::Colour(0x18FFFFFF));
    g.drawHorizontalLine(0, 0.0f, w);
    g.setColour(juce::Colour(0x20000000));
    g.drawHorizontalLine(headerH - 1, 0.0f, w);

    // Title — embossed style
    g.setFont(juce::Font(19.0f, juce::Font::bold));
    g.setColour(juce::Colour(0x20000000));
    g.drawText("StarDust", 15, 2, 140, headerH, juce::Justification::centredLeft);
    g.setColour(StarDustLookAndFeel::kAccent);
    g.drawText("StarDust", 14, 1, 140, headerH, juce::Justification::centredLeft);

    // ---- Screws in corners ----
    drawScrew(g, 12.0f, 12.0f);
    drawScrew(g, w - 12.0f, 12.0f);
    drawScrew(g, 12.0f, static_cast<float>(getHeight()) - 12.0f);
    drawScrew(g, w - 12.0f, static_cast<float>(getHeight()) - 12.0f);

    // ---- Granular section inset panel ----
    const int panelY = 134;
    const int panelH = 148;
    const int panelMargin = 16;
    auto grainPanel = juce::Rectangle<int>(panelMargin, panelY, getWidth() - panelMargin * 2, panelH);

    // Inset shadow
    g.setColour(juce::Colour(0x30000000));
    g.fillRoundedRectangle(grainPanel.toFloat().translated(1.0f, 1.0f), 4.0f);
    // Inset background
    g.setColour(StarDustLookAndFeel::kInset.withAlpha(0.7f));
    g.fillRoundedRectangle(grainPanel.toFloat(), 4.0f);
    // Inner highlight edge
    g.setColour(juce::Colour(0x0AFFFFFF));
    g.drawRoundedRectangle(grainPanel.toFloat().reduced(1.0f), 3.0f, 0.5f);

    // Section label — silk-screened style
    g.setColour(StarDustLookAndFeel::kLabel.withAlpha(0.5f));
    g.setFont(juce::Font(9.0f, juce::Font::bold));
    g.drawText("GRANULAR", panelMargin + 10, panelY + 3, 80, 12, juce::Justification::centredLeft);

    // ---- Bottom strip ----
    const int bottomY = getHeight() - 38;
    g.setColour(StarDustLookAndFeel::kPanelLight);
    g.fillRect(0, bottomY, getWidth(), 38);
    g.setColour(juce::Colour(0x18FFFFFF));
    g.drawHorizontalLine(bottomY, 0.0f, w);

    // VU labels
    g.setColour(StarDustLookAndFeel::kLabel.withAlpha(0.5f));
    g.setFont(juce::Font(8.0f, juce::Font::bold));
    g.drawText("IN", 10, bottomY + 3, 20, 10, juce::Justification::centred);
    g.drawText("OUT", getWidth() - 30, bottomY + 3, 24, 10, juce::Justification::centred);
}

void StarDustEditor::resized()
{
    starfield.setBounds(getLocalBounds());

    presetSelector.setBounds(getWidth() - 175, 7, 165, 22);

    // Top row: BITS, RATE, CUTOFF, DRIVE, MIX
    const int knobW = 80;
    const int knobH = 78;
    const int row1Y = 46;
    const int row1X = (getWidth() - knobW * 5) / 2;

    layoutKnobInBounds(bitsKnob,   { row1X,              row1Y, knobW, knobH });
    layoutKnobInBounds(rateKnob,   { row1X + knobW,      row1Y, knobW, knobH });
    layoutKnobInBounds(cutoffKnob, { row1X + knobW * 2,  row1Y, knobW, knobH });
    layoutKnobInBounds(driveKnob,  { row1X + knobW * 3,  row1Y, knobW, knobH });
    layoutKnobInBounds(mixKnob,    { row1X + knobW * 4,  row1Y, knobW, knobH });

    // Grain tune slider
    const int panelMargin = 16;
    const int panelInner = panelMargin + 10;
    const int tuneY = 148;
    tuneLabel.setBounds(panelInner + 24, tuneY, 50, 14);
    tuneValueLabel.setBounds(getWidth() - panelInner - 84, tuneY, 60, 14);
    tuneFader.setBounds(panelInner + 24, tuneY + 14, getWidth() - (panelInner + 24) * 2 + 24, 26);

    // Grain knobs under slider
    const int grainKnobW = 80;
    const int grainKnobH = 68;
    const int grainRowY = tuneY + 44;
    const int grainRowX = (getWidth() - grainKnobW * 5) / 2;

    layoutKnobInBounds(grainMixKnob,     { grainRowX,                  grainRowY, grainKnobW, grainKnobH });
    layoutKnobInBounds(grainDensityKnob, { grainRowX + grainKnobW,     grainRowY, grainKnobW, grainKnobH });
    layoutKnobInBounds(grainSizeKnob,    { grainRowX + grainKnobW * 2, grainRowY, grainKnobW, grainKnobH });
    layoutKnobInBounds(grainScatterKnob, { grainRowX + grainKnobW * 3, grainRowY, grainKnobW, grainKnobH });
    layoutKnobInBounds(widthKnob,        { grainRowX + grainKnobW * 4, grainRowY, grainKnobW, grainKnobH });

    // Level meters
    const int meterY = getHeight() - 32;
    const int meterH = 24;
    inputMeterL.setBounds(12, meterY, 5, meterH);
    inputMeterR.setBounds(19, meterY, 5, meterH);
    outputMeterL.setBounds(getWidth() - 24, meterY, 5, meterH);
    outputMeterR.setBounds(getWidth() - 17, meterY, 5, meterH);
}
