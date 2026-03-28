#include "PluginEditor.h"
#include "BinaryData.h"

// ============================================================================
// StardustLookAndFeel — Orbital Galaxy Style
// ============================================================================

StardustLookAndFeel::StardustLookAndFeel()
{
    setColour(juce::Slider::rotarySliderFillColourId, kAccent);
    setColour(juce::Slider::thumbColourId, kAccent);
    setColour(juce::Label::textColourId, kFg);
    setColour(juce::ComboBox::backgroundColourId, juce::Colours::transparentBlack);
    setColour(juce::ComboBox::textColourId, kAccent);
    setColour(juce::ComboBox::outlineColourId, juce::Colours::transparentBlack);
    setColour(juce::ComboBox::arrowColourId, kFg);
    setColour(juce::PopupMenu::backgroundColourId, juce::Colour(0xFF0E0E0E));
    setColour(juce::PopupMenu::textColourId, kFg);
    setColour(juce::PopupMenu::highlightedBackgroundColourId, kAccent.withAlpha(0.08f));
    setColour(juce::PopupMenu::highlightedTextColourId, kAccent);

    // AlertWindow (Save/Rename dialogs)
    setColour(juce::AlertWindow::backgroundColourId, juce::Colour(0xFF0A0A0A));
    setColour(juce::AlertWindow::textColourId, kFg);
    setColour(juce::AlertWindow::outlineColourId, kFgGhost.withAlpha(0.65f));

    // TextEditor inside AlertWindow
    setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xFF151515));
    setColour(juce::TextEditor::textColourId, kAccent);
    setColour(juce::TextEditor::outlineColourId, kFgGhost.withAlpha(0.4f));
    setColour(juce::TextEditor::focusedOutlineColourId, kAccent.withAlpha(0.5f));
    setColour(juce::CaretComponent::caretColourId, kAccent);

    // Slider popup tooltip
    setColour(juce::BubbleComponent::backgroundColourId, juce::Colour(0xFF0A0A0A));
    setColour(juce::BubbleComponent::outlineColourId, kFgGhost.withAlpha(0.4f));
    setColour(juce::TooltipWindow::textColourId, kAccent);

    // Buttons inside AlertWindow
    setColour(juce::TextButton::buttonColourId, juce::Colour(0xFF1A1A1A));
    setColour(juce::TextButton::buttonOnColourId, kAccent.withAlpha(0.15f));
    setColour(juce::TextButton::textColourOffId, kFg);
    setColour(juce::TextButton::textColourOnId, kAccent);
}

void StardustLookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y,
                                            int width, int height,
                                            float sliderPos,
                                            float rotaryStartAngle,
                                            float rotaryEndAngle,
                                            juce::Slider& slider)
{
    const float radius = static_cast<float>(juce::jmin(width, height)) * 0.36f;
    const float centreX = static_cast<float>(x) + static_cast<float>(width) * 0.5f;
    const float centreY = static_cast<float>(y) + static_cast<float>(height) * 0.5f;
    const float angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);

    // Detect bipolar knob (min < 0 and max > 0, or grainMorph which is 0-1 centered at 0.5)
    const bool isBipolar = (slider.getMinimum() < 0.0 && slider.getMaximum() > 0.0)
                           || slider.getName() == "grainMorph";
    const bool isHovered = slider.isEnabled() && (slider.isMouseOver(true) || slider.isMouseButtonDown());

    // Background track arc — brightens on hover
    {
        juce::Path bgArc;
        bgArc.addCentredArc(centreX, centreY, radius, radius, 0.0f,
                             rotaryStartAngle, rotaryEndAngle, true);
        g.setColour(isHovered ? kFgGhost.withAlpha(0.7f) : kFgGhost.withAlpha(0.45f));
        g.strokePath(bgArc, juce::PathStrokeType(2.5f, juce::PathStrokeType::curved,
                                                   juce::PathStrokeType::rounded));
    }

    // Value arc — pure white
    {
        const float midAngle = (rotaryStartAngle + rotaryEndAngle) * 0.5f;
        const float arcStart = isBipolar ? midAngle : rotaryStartAngle;
        const float arcEnd = angle;

        if (std::abs(arcEnd - arcStart) > 0.01f)
        {
            juce::Path valueArc;
            valueArc.addCentredArc(centreX, centreY, radius, radius, 0.0f,
                                    std::min(arcStart, arcEnd),
                                    std::max(arcStart, arcEnd), true);
            g.setColour(kAccent);
            g.strokePath(valueArc, juce::PathStrokeType(2.5f, juce::PathStrokeType::curved,
                                                         juce::PathStrokeType::rounded));
        }
    }

    // Tick marks at start, center, end
    const float tickOuter = radius + 5.0f;
    const float tickInner = radius + 2.0f;
    g.setColour(kFgGhost.withAlpha(isHovered ? 0.4f : 0.25f));
    for (int i = 0; i < 3; ++i)
    {
        const float tickAngle = rotaryStartAngle
            + static_cast<float>(i) * 0.5f * (rotaryEndAngle - rotaryStartAngle);
        g.drawLine(centreX + tickInner * std::sin(tickAngle),
                   centreY - tickInner * std::cos(tickAngle),
                   centreX + tickOuter * std::sin(tickAngle),
                   centreY - tickOuter * std::cos(tickAngle), 0.7f);
    }
}

void StardustLookAndFeel::drawLinearSlider(juce::Graphics& g, int x, int y,
                                            int width, int height,
                                            float sliderPos,
                                            float /*minSliderPos*/,
                                            float /*maxSliderPos*/,
                                            juce::Slider::SliderStyle style,
                                            juce::Slider& slider)
{
    if (style == juce::Slider::LinearHorizontal && slider.getName() == "destroyFader")
    {
        // SP-950 horizontal fader — RPM labels below, semitone ticks, Tune readout
        const float fx = static_cast<float>(x);
        const float fy = static_cast<float>(y);
        const float fw = static_cast<float>(width);
        const float fh = static_cast<float>(height);
        const float val = static_cast<float>(slider.getValue());

        const float trackPad = 12.0f;
        const float trackLeft = fx + trackPad;
        const float trackRight = fx + fw - trackPad;
        const float trackLen = trackRight - trackLeft;
        const float trackY = fy + fh * 0.35f; // track in upper portion, labels below

        // Track — recessed groove (horizontal)
        g.setColour(kAccent.withAlpha(0.08f));
        g.fillRoundedRectangle(trackLeft, trackY - 1.5f, trackLen, 3.0f, 3.0f);
        g.setColour(juce::Colour(0xFF000000).withAlpha(0.25f));
        g.fillRect(trackLeft + 3.0f, trackY - 0.5f, trackLen - 6.0f, 1.0f);

        // Semitone ticks — vertical lines crossing the track
        // Horizontal: val 0=left (78RPM), val 3=right (33RPM)
        static constexpr int kTicksPerSeg[3] = { 2, 12, 5 };
        const float tickHalfH = 6.0f;
        for (int seg = 0; seg < 3; ++seg)
        {
            const float segLeftX = trackLeft + (static_cast<float>(seg) / 3.0f) * trackLen;
            const float segRightX = trackLeft + (static_cast<float>(seg + 1) / 3.0f) * trackLen;
            const float segSpan = segRightX - segLeftX;
            const int nTicks = kTicksPerSeg[seg];
            for (int t = 1; t < nTicks; ++t)
            {
                const float tx = segLeftX + (static_cast<float>(t) / static_cast<float>(nTicks)) * segSpan;
                g.setColour(kAccent.withAlpha(0.15f));
                g.fillRect(tx - 0.5f, trackY - tickHalfH, 1.0f, tickHalfH * 2.0f);
            }
        }

        // RPM labels + major ticks (left→right: 78, x2, 45, 33)
        static const juce::StringArray rpmLabels { "78 RPM", "x2", "45 RPM", "33 RPM" };
        auto labelFont = juce::Font(juce::FontOptions(
            juce::Font::getDefaultMonospacedFontName(), 10.0f, juce::Font::plain));
        g.setFont(labelFont);
        for (int i = 0; i < 4; ++i)
        {
            const float posX = trackLeft + (static_cast<float>(i) / 3.0f) * trackLen;
            const float dist = std::abs(val - static_cast<float>(i));
            const bool nearest = (dist < 0.5f);

            // Major tick — taller, full white when active
            g.setColour(nearest ? kAccent : kAccent.withAlpha(0.20f));
            g.fillRect(posX - 0.5f, trackY - tickHalfH - 2.0f, 1.0f, (tickHalfH + 2.0f) * 2.0f);

            // Label below track
            g.setColour(nearest ? kAccent : kAccent.withAlpha(0.35f));
            g.drawText(rpmLabels[i], static_cast<int>(posX - 28.0f),
                       static_cast<int>(trackY + tickHalfH + 4.0f), 56, 14,
                       juce::Justification::centred);
        }

        // Thumb — horizontal slider handle
        const float thumbX = trackLeft + (val / 3.0f) * trackLen;
        const float thumbW = 12.0f;
        const float thumbH = 22.0f;
        const float thumbLeft2 = thumbX - thumbW * 0.5f;
        const float thumbTopY = trackY - thumbH * 0.5f;

        juce::ColourGradient grad(juce::Colour(0xFF999999), thumbLeft2, thumbTopY,
                                  juce::Colour(0xFF555555), thumbLeft2, thumbTopY + thumbH, false);
        g.setGradientFill(grad);
        g.fillRoundedRectangle(thumbLeft2, thumbTopY, thumbW, thumbH, 3.0f);

        // Center grip line (vertical on horizontal thumb)
        g.setColour(kAccent.withAlpha(0.7f));
        g.fillRect(thumbX - 0.5f, thumbTopY + 4.0f, 1.0f, thumbH - 8.0f);

        g.setColour(kAccent.withAlpha(0.15f));
        g.drawRoundedRectangle(thumbLeft2, thumbTopY, thumbW, thumbH, 3.0f, 1.0f);

        // Tune readout centered below labels
        const float faderPos = val;
        int semitones = 0;
        if (faderPos <= 1.0f)
            semitones = static_cast<int>(std::round(9.5f + (12.0f - 9.5f) * faderPos));
        else if (faderPos <= 2.0f)
            semitones = static_cast<int>(std::round(12.0f * (2.0f - faderPos)));
        else
            semitones = static_cast<int>(std::round(-5.5f * (faderPos - 2.0f)));

        auto valFont = juce::Font(juce::FontOptions(
            juce::Font::getDefaultMonospacedFontName(), 13.0f, juce::Font::bold));
        g.setFont(valFont);
        g.setColour(kFg);
        juce::String tuneText = "Tune: ";
        if (semitones > 0) tuneText += "+" + juce::String(semitones) + " st";
        else if (semitones < 0) tuneText += juce::String(semitones) + " st";
        else tuneText += "0 st";
        g.drawText(tuneText, static_cast<int>(fx), static_cast<int>(trackY + tickHalfH + 20.0f),
                   static_cast<int>(fw), 18, juce::Justification::centred);

        return;
    }

    if (style == juce::Slider::LinearVertical)
    {
        // Vertical gain fader — rectangle thumb with dB value, hover highlight below
        const float fx = static_cast<float>(x);
        const float fy = static_cast<float>(y);
        const float fw = static_cast<float>(width);
        const float fh = static_cast<float>(height);

        // Use full component bounds for highlight (not just slider travel range)
        const auto fullBounds = slider.getLocalBounds().toFloat();

        // Hover: highlight meter area below thumb (narrower than full slider)
        if (slider.isMouseOver(true) || slider.isMouseButtonDown())
        {
            const float meterHighlightW = 16.0f; // approximate meter group width
            const float hlX = fx + (fw - meterHighlightW) * 0.5f;
            g.setColour(kAccent.withAlpha(0.06f));
            g.fillRect(hlX, sliderPos, meterHighlightW, fullBounds.getBottom() - sliderPos);
        }

        // Thumb — dynamic width rectangle with dB value text
        const float thumbH = 20.0f;
        const float thumbY = sliderPos - thumbH * 0.5f;

        const auto valueText = slider.getTextFromValue(slider.getValue());
        auto font = juce::Font(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(), 9.0f, juce::Font::bold));
        g.setFont(font);
        const float textW = font.getStringWidthFloat(valueText);
        const float padX = 6.0f;
        const float thumbW = std::max(fw, textW + padX * 2.0f);
        const float thumbX = fx + (fw - thumbW) * 0.5f; // center on slider

        // Body
        g.setColour(juce::Colour(0xFF111111));
        g.fillRoundedRectangle(thumbX, thumbY, thumbW, thumbH, 2.0f);

        // Border
        const float borderAlpha = (slider.isMouseOver(true) || slider.isMouseButtonDown()) ? 0.6f : 0.3f;
        g.setColour(kAccent.withAlpha(borderAlpha));
        g.drawRoundedRectangle(thumbX, thumbY, thumbW, thumbH, 2.0f, 1.0f);

        // dB value text
        g.setColour(kAccent.withAlpha(0.9f));
        g.drawText(valueText, static_cast<int>(thumbX), static_cast<int>(thumbY),
                   static_cast<int>(thumbW), static_cast<int>(thumbH), juce::Justification::centred);

        return;
    }

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

void StardustLookAndFeel::drawToggleButton(juce::Graphics& g, juce::ToggleButton& button,
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
    g.setColour(on ? kAccent.withAlpha(0.15f) : kFgGhost.withAlpha(0.25f));
    g.fillRoundedRectangle(pillX, pillY, w, h, radius);

    // Track border
    g.setColour(on ? kAccent.withAlpha(0.6f) : kFgGhost.withAlpha(0.8f));
    g.drawRoundedRectangle(pillX, pillY, w, h, radius, 1.0f);

    // Sliding knob
    const float knobX = on ? (pillX + w - knobSize - 2.0f) : (pillX + 2.0f);
    const float knobY = pillY + (h - knobSize) * 0.5f;

    if (on)
    {
        // Subtle white glow behind filled knob
        g.setColour(kAccent.withAlpha(0.2f));
        g.fillEllipse(knobX - 2.0f, knobY - 2.0f, knobSize + 4.0f, knobSize + 4.0f);
        // Filled white circle
        g.setColour(kAccent);
        g.fillEllipse(knobX, knobY, knobSize, knobSize);
    }
    else
    {
        // Empty outline circle
        g.setColour(kFgDim);
        g.drawEllipse(knobX + 0.5f, knobY + 0.5f, knobSize - 1.0f, knobSize - 1.0f, 1.2f);
    }

    // Hover highlight
    if (shouldDrawButtonAsHighlighted)
    {
        g.setColour(kAccent.withAlpha(0.06f));
        g.fillRoundedRectangle(pillX, pillY, w, h, radius);
    }
}

void StardustLookAndFeel::drawButtonBackground(juce::Graphics& g, juce::Button& button,
                                                const juce::Colour& /*backgroundColour*/,
                                                bool shouldDrawButtonAsHighlighted,
                                                bool /*shouldDrawButtonAsDown*/)
{
    auto bounds = button.getLocalBounds().toFloat();

    bool isLibraryBtn = button.getComponentID() == "libraryBtn";
    bool isFavoriteBtn = button.getComponentID() == "favoriteBtn";
    bool isScaleBtn = button.getComponentID() == "scaleBtn";
    bool isPresetBarBtn = button.getButtonText() == "..."
                          || button.getButtonText() == "<"
                          || button.getButtonText() == ">"
                          || isLibraryBtn
                          || isFavoriteBtn
                          || isScaleBtn;
    bool isAlertBtn = dynamic_cast<juce::AlertWindow*>(button.getParentComponent()) != nullptr;

    if (isAlertBtn)
    {
        // AlertWindow buttons — rounded dark buttons with border
        g.setColour(shouldDrawButtonAsHighlighted ? kAccent.withAlpha(0.12f) : juce::Colour(0xFF151515));
        g.fillRoundedRectangle(bounds.reduced(1.0f), 4.0f);
        g.setColour(shouldDrawButtonAsHighlighted ? kAccent.withAlpha(0.5f) : kFgGhost.withAlpha(0.4f));
        g.drawRoundedRectangle(bounds.reduced(1.0f), 4.0f, 1.0f);
        return;
    }

    // Shared hover effect for all preset bar buttons
    if (shouldDrawButtonAsHighlighted && (isPresetBarBtn))
    {
        g.setColour(kAccent.withAlpha(0.08f));
        g.fillRoundedRectangle(bounds.reduced(1.0f), 3.0f);
    }

    if (button.getButtonText() == "...")
    {
        // Draw circle border
        const float size = juce::jmin(bounds.getWidth(), bounds.getHeight()) - 8.0f;
        const float cx = bounds.getCentreX();
        const float cy = bounds.getCentreY();
        g.setColour(shouldDrawButtonAsHighlighted ? kAccent : kFgDim);
        g.drawEllipse(cx - size * 0.5f, cy - size * 0.5f, size, size, 1.2f);
    }

    if (isLibraryBtn)
    {
        // Draw three horizontal lines (list icon)
        const float cx = bounds.getCentreX();
        const float cy = bounds.getCentreY();
        const float lineW = 13.0f;
        const float gap = 4.0f;
        g.setColour(shouldDrawButtonAsHighlighted ? kAccent : kFg);
        for (int i = -1; i <= 1; ++i)
        {
            const float ly = cy + static_cast<float>(i) * gap;
            g.drawLine(cx - lineW * 0.5f, ly, cx + lineW * 0.5f, ly, 2.0f);
        }
    }

    if (isScaleBtn)
    {
        // Draw gear icon — circle with 6 teeth
        const float cx = bounds.getCentreX();
        const float cy = bounds.getCentreY();
        const float outerR = 7.0f;
        const float innerR = 4.5f;
        const float toothW = 2.5f;
        g.setColour(shouldDrawButtonAsHighlighted ? kAccent : kFg);

        // Inner circle
        g.drawEllipse(cx - innerR, cy - innerR, innerR * 2.0f, innerR * 2.0f, 1.5f);

        // 6 teeth around the circle
        for (int i = 0; i < 6; ++i)
        {
            const float angle = static_cast<float>(i) * juce::MathConstants<float>::twoPi / 6.0f;
            const float cosA = std::cos(angle);
            const float sinA = std::sin(angle);
            const float x1 = cx + cosA * (innerR + 0.5f);
            const float y1 = cy + sinA * (innerR + 0.5f);
            const float x2 = cx + cosA * outerR;
            const float y2 = cy + sinA * outerR;
            g.drawLine(x1, y1, x2, y2, toothW);
        }
    }
}

void StardustLookAndFeel::drawButtonText(juce::Graphics& g, juce::TextButton& button,
                                          bool shouldDrawButtonAsHighlighted,
                                          bool /*shouldDrawButtonAsDown*/)
{
    auto bounds = button.getLocalBounds().toFloat();

    bool isAlertBtn = dynamic_cast<juce::AlertWindow*>(button.getParentComponent()) != nullptr;

    if (isAlertBtn)
    {
        g.setColour(shouldDrawButtonAsHighlighted ? kAccent : kFg);
        g.setFont(juce::FontOptions(12.0f).withStyle("Bold"));
        g.drawText(button.getButtonText(), bounds, juce::Justification::centred);
    }
    else if (button.getButtonText() == "...")
    {
        // Draw 3 horizontal dots inside the circle
        const float cx = bounds.getCentreX();
        const float cy = bounds.getCentreY();
        const float dotR = 1.2f;
        const float spacing = 3.0f;
        g.setColour(shouldDrawButtonAsHighlighted ? kAccent : kFg);
        g.fillEllipse(cx - spacing - dotR, cy - dotR, dotR * 2, dotR * 2);
        g.fillEllipse(cx - dotR, cy - dotR, dotR * 2, dotR * 2);
        g.fillEllipse(cx + spacing - dotR, cy - dotR, dotR * 2, dotR * 2);
    }
    else if (button.getButtonText() == "<" || button.getButtonText() == ">")
    {
        // Draw arrow chevrons as paths for pixel-perfect vertical centering
        auto baseCol = button.findColour(juce::TextButton::textColourOffId);
        g.setColour(shouldDrawButtonAsHighlighted ? kAccent : baseCol);
        const float cx = bounds.getCentreX();
        const float cy = bounds.getCentreY();
        const float arrowH = 5.0f;
        const float arrowW = 3.5f;
        const bool isLeft = button.getButtonText() == "<";
        juce::Path arrow;
        if (isLeft)
        {
            arrow.startNewSubPath(cx + arrowW, cy - arrowH);
            arrow.lineTo(cx - arrowW, cy);
            arrow.lineTo(cx + arrowW, cy + arrowH);
        }
        else
        {
            arrow.startNewSubPath(cx - arrowW, cy - arrowH);
            arrow.lineTo(cx + arrowW, cy);
            arrow.lineTo(cx - arrowW, cy + arrowH);
        }
        g.strokePath(arrow, juce::PathStrokeType(1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
    else
    {
        auto baseCol = button.findColour(juce::TextButton::textColourOffId);
        bool isDialogBtn = dynamic_cast<StardustDialog*>(button.getParentComponent()) != nullptr;

        if (isDialogBtn)
        {
            g.setColour(shouldDrawButtonAsHighlighted ? baseCol.withAlpha(0.65f) : baseCol);
            g.setFont(juce::FontOptions(13.0f).withStyle("Bold"));
        }
        else
        {
            g.setColour(shouldDrawButtonAsHighlighted ? kAccent : baseCol);
            g.setFont(juce::FontOptions(16.0f).withStyle("Bold"));
        }
        g.drawText(button.getButtonText(), bounds, juce::Justification::centred);
    }
}

void StardustLookAndFeel::drawAlertBox(juce::Graphics& g, juce::AlertWindow& window,
                                        const juce::Rectangle<int>& textArea, juce::TextLayout& layout)
{
    auto bounds = window.getLocalBounds().toFloat();

    // Background
    g.setColour(juce::Colour(0xFF0A0A0A));
    g.fillRoundedRectangle(bounds, 6.0f);

    // Border — same style as plugin sections
    g.setColour(kFgGhost.withAlpha(0.4f));
    g.drawRoundedRectangle(bounds.reduced(0.5f), 6.0f, 1.5f);

    // Inner subtle line below title
    const float titleBottom = static_cast<float>(textArea.getY());
    g.setColour(kFgGhost.withAlpha(0.2f));
    g.drawHorizontalLine(static_cast<int>(titleBottom), bounds.getX() + 12.0f, bounds.getRight() - 12.0f);

    // Title text
    g.setColour(kAccent);
    g.setFont(juce::FontOptions(14.0f).withStyle("Bold"));
    g.drawText(window.getName(), bounds.getX() + 14, bounds.getY() + 8, bounds.getWidth() - 28, 20,
               juce::Justification::centredLeft);

    // Message layout
    g.setColour(kFg);
    layout.draw(g, textArea.toFloat());
}

void StardustLookAndFeel::drawComboBox(juce::Graphics& g, int width, int height,
                                        bool /*isButtonDown*/,
                                        int /*buttonX*/, int /*buttonY*/,
                                        int /*buttonW*/, int /*buttonH*/,
                                        juce::ComboBox& box)
{
    auto bounds = juce::Rectangle<float>(0, 0, static_cast<float>(width), static_cast<float>(height));
    const auto outlineCol = box.findColour(juce::ComboBox::outlineColourId);
    const bool hasOutline = outlineCol.getAlpha() > 0;

    if (hasOutline)
    {
        // Input-field styled combo (dialogs, settings) — background + border + left-aligned
        g.setColour(box.findColour(juce::ComboBox::backgroundColourId));
        g.fillRoundedRectangle(bounds, 4.0f);

        g.setColour(box.isMouseOver(true)
            ? kAccent.withAlpha(0.35f)
            : outlineCol);
        g.drawRoundedRectangle(bounds, 4.0f, 1.0f);

        // Left-aligned text with padding — smaller font for compact combos
        const float fontSize = (height < 22) ? 11.0f : 13.0f;
        auto font = juce::Font(juce::FontOptions(juce::Font::getDefaultSansSerifFontName(), fontSize, juce::Font::bold));
        g.setFont(font);
        g.setColour(box.findColour(juce::ComboBox::textColourId));
        g.drawText(box.getText(), 8, 0, width - 24, height, juce::Justification::centredLeft);

        // Chevron pinned to right
        const float arrowSize = 6.0f;
        const float arrowX = static_cast<float>(width) - 14.0f;
        const float arrowY = static_cast<float>(height) * 0.5f;
        juce::Path arrow;
        arrow.startNewSubPath(arrowX - arrowSize * 0.5f, arrowY - arrowSize * 0.25f);
        arrow.lineTo(arrowX, arrowY + arrowSize * 0.25f);
        arrow.lineTo(arrowX + arrowSize * 0.5f, arrowY - arrowSize * 0.25f);
        g.setColour(kFgDim);
        g.strokePath(arrow, juce::PathStrokeType(1.5f, juce::PathStrokeType::curved,
                                                  juce::PathStrokeType::rounded));
    }
    else
    {
        // Transparent combo (preset selector, etc.) — centered text + chevron after text
        if (box.isMouseOver(true))
        {
            g.setColour(kAccent.withAlpha(0.08f));
            g.fillRoundedRectangle(bounds.reduced(1.0f), 3.0f);
        }

        auto font = getComboBoxFont(box);
        g.setFont(font);
        const bool isDirty = box.getName() == "dirty";
        auto displayText = box.getText() + (isDirty ? " *" : "");
        const float textW = font.getStringWidthFloat(displayText);
        const float centerX = static_cast<float>(width) * 0.5f;

        g.setColour(box.findColour(juce::ComboBox::textColourId));
        g.drawText(displayText, 0, 0, width, height, juce::Justification::centred);

        const float arrowSize = 6.0f;
        const float arrowGap = 6.0f;
        const float arrowXIdeal = centerX + textW * 0.5f + arrowGap + arrowSize * 0.5f;
        const float arrowX = std::min(arrowXIdeal, static_cast<float>(width) - arrowSize * 0.5f - 2.0f);
        const float arrowY = static_cast<float>(height) * 0.5f;

        juce::Path arrow;
        arrow.startNewSubPath(arrowX - arrowSize * 0.5f, arrowY - arrowSize * 0.25f);
        arrow.lineTo(arrowX, arrowY + arrowSize * 0.25f);
        arrow.lineTo(arrowX + arrowSize * 0.5f, arrowY - arrowSize * 0.25f);

        g.setColour(kAccent.withAlpha(0.6f));
        g.strokePath(arrow, juce::PathStrokeType(1.5f, juce::PathStrokeType::curved,
                                                  juce::PathStrokeType::rounded));
    }
}

juce::Font StardustLookAndFeel::getComboBoxFont(juce::ComboBox& /*box*/)
{
    return juce::Font(juce::FontOptions(juce::Font::getDefaultSansSerifFontName(), 15.0f, juce::Font::bold));
}

void StardustLookAndFeel::positionComboBoxText(juce::ComboBox& box, juce::Label& label)
{
    // Hide the default label — we draw text ourselves in drawComboBox
    label.setBounds(0, 0, 0, 0);
    label.setVisible(false);
}

void StardustLookAndFeel::drawBubble(juce::Graphics& g, juce::BubbleComponent& /*bubble*/,
                                      const juce::Point<float>& /*tip*/,
                                      const juce::Rectangle<float>& body)
{
    // Dark rounded tooltip matching plugin style
    g.setColour(juce::Colour(0xFF0A0A0A));
    g.fillRoundedRectangle(body, 4.0f);
    g.setColour(kFgGhost.withAlpha(0.4f));
    g.drawRoundedRectangle(body, 4.0f, 1.0f);
}

juce::Font StardustLookAndFeel::getSliderPopupFont(juce::Slider& /*slider*/)
{
    return juce::Font(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(), 11.0f, juce::Font::bold));
}

int StardustLookAndFeel::getSliderPopupPlacement(juce::Slider& /*slider*/)
{
    // Always show tooltip above the knob, centered
    return juce::BubbleComponent::above;
}

// ============================================================================
// StardustDialog
// ============================================================================

StardustDialog::StardustDialog(const juce::String& title, const juce::String& defaultText,
                               std::function<void(const juce::String&)> onConfirm)
    : callback(std::move(onConfirm))
{
    setSize(320, 155);
    setOpaque(true);
    setWantsKeyboardFocus(true);

    titleLabel.setText(title, juce::dontSendNotification);
    titleLabel.setFont(juce::FontOptions(14.0f).withStyle("Bold"));
    titleLabel.setColour(juce::Label::textColourId, StardustLookAndFeel::kAccent);
    titleLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(titleLabel);

    // Primary input — preset name
    textInput.setText(defaultText);
    textInput.setFont(juce::FontOptions(13.0f));
    textInput.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xFF141414));
    textInput.setColour(juce::TextEditor::textColourId, StardustLookAndFeel::kAccent);
    textInput.setColour(juce::TextEditor::outlineColourId, StardustLookAndFeel::kFgGhost.withAlpha(0.35f));
    textInput.setColour(juce::TextEditor::focusedOutlineColourId, StardustLookAndFeel::kAccent.withAlpha(0.4f));
    textInput.setColour(juce::CaretComponent::caretColourId, StardustLookAndFeel::kAccent);
    textInput.setJustification(juce::Justification::centredLeft);
    textInput.setTextToShowWhenEmpty("Enter preset name...", StardustLookAndFeel::kFgGhost);
    textInput.setInputRestrictions(32);
    textInput.setIndents(8, 0);
    textInput.onTextChange = [this] { updateSaveButton(); };
    addAndMakeVisible(textInput);

    // Save — plain white text button
    confirmBtn.setButtonText("Save");
    confirmBtn.setColour(juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    confirmBtn.setColour(juce::TextButton::textColourOffId, StardustLookAndFeel::kAccent);
    confirmBtn.onClick = [this] { doConfirm(); };
    addAndMakeVisible(confirmBtn);

    // Cancel — text-only ghost button (no border, no background)
    cancelBtn.setButtonText("Cancel");
    cancelBtn.setColour(juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    cancelBtn.setColour(juce::TextButton::textColourOffId, StardustLookAndFeel::kFgDim);
    cancelBtn.onClick = [this] {
        if (onDismiss) onDismiss();
    };
    addAndMakeVisible(cancelBtn);

    updateSaveButton();
}

void StardustDialog::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    const float m = 16.0f;

    // Solid opaque background
    g.fillAll(juce::Colour(0xFF0C0C0C));

    // Outer border
    g.setColour(StardustLookAndFeel::kFgGhost.withAlpha(0.4f));
    g.drawRect(bounds, 1.5f);

    // Title divider
    const float titleH = 36.0f;
    g.setColour(StardustLookAndFeel::kFgGhost.withAlpha(0.15f));
    g.drawHorizontalLine(static_cast<int>(titleH), m, bounds.getRight() - m);

    // Section labels — derive positions from component bounds
    g.setFont(juce::FontOptions(9.0f).withStyle("Bold"));
    g.setColour(StardustLookAndFeel::kFgDim);
    g.drawText("NAME", static_cast<int>(m), textInput.getY() - 15,
               80, 11, juce::Justification::centredLeft);

    if (bankCombo != nullptr)
    {
        g.drawText("BANK", static_cast<int>(m), bankCombo->getY() - 15,
                   80, 11, juce::Justification::centredLeft);

        // Accent left bar for new bank input
        if (newBankInput != nullptr && newBankInput->isVisible())
        {
            auto nbBounds = newBankInput->getBounds();
            g.setColour(StardustLookAndFeel::kAccent.withAlpha(0.3f));
            g.fillRect(static_cast<float>(nbBounds.getX() - 4), static_cast<float>(nbBounds.getY()),
                       2.0f, static_cast<float>(nbBounds.getHeight()));
        }
    }

    // Button divider — above the buttons
    const int btnDivY = confirmBtn.getY() - 12;
    g.setColour(StardustLookAndFeel::kFgGhost.withAlpha(0.15f));
    g.drawHorizontalLine(btnDivY, m, bounds.getRight() - m);

    // Both buttons are flat text — no borders drawn
}

void StardustDialog::resized()
{
    const int m = 16;
    const int titleH = 36;
    const int labelH = 11;
    const int labelToInput = 4;
    const int inputH = 32;
    const int groupGap = 14;
    const int btnH = 32;
    const int btnAreaH = 16 + btnH + 16; // divider pad + buttons + bottom pad

    titleLabel.setBounds(0, 0, getWidth(), titleH);

    const int cx = m;
    const int cw = getWidth() - m * 2;
    int y = titleH + 10;

    // NAME label (drawn in paint)
    y += labelH + labelToInput;
    textInput.setBounds(cx, y, cw, inputH);
    y += inputH;

    if (bankCombo != nullptr)
    {
        y += groupGap;
        // BANK label (drawn in paint)
        y += labelH + labelToInput;
        bankCombo->setBounds(cx, y, cw, inputH);
        y += inputH;

        if (newBankInput != nullptr && newBankInput->isVisible())
        {
            y += 8;
            newBankInput->setBounds(cx + 8, y, cw - 8, inputH);
            y += inputH;
        }
    }

    // Compute needed height and resize if needed
    const int neededH = y + btnAreaH;
    if (getHeight() != neededH)
    {
        setSize(getWidth(), neededH);
        if (auto* parent = getParentComponent())
            setCentrePosition(parent->getWidth() / 2, parent->getHeight() / 2);
    }

    // Buttons — right-aligned, Save is primary
    const int saveBtnW = 80;
    const int cancelBtnW = 65;
    const int btnGap = 8;
    const int btnY = getHeight() - m - btnH;
    const int btnRight = getWidth() - m;
    confirmBtn.setBounds(btnRight - saveBtnW, btnY, saveBtnW, btnH);
    cancelBtn.setBounds(btnRight - saveBtnW - btnGap - cancelBtnW, btnY, cancelBtnW, btnH);
}

bool StardustDialog::keyPressed(const juce::KeyPress& key)
{
    if (key == juce::KeyPress::returnKey)
    {
        confirmBtn.triggerClick();
        return true;
    }
    if (key == juce::KeyPress::escapeKey)
    {
        cancelBtn.triggerClick();
        return true;
    }
    return false;
}

void StardustDialog::updateSaveButton()
{
    const bool nameOk = textInput.getText().trim().isNotEmpty();
    bool bankOk = true;
    if (newBankInput != nullptr && newBankInput->isVisible())
        bankOk = newBankInput->getText().trim().isNotEmpty();

    const bool enabled = nameOk && bankOk;
    confirmBtn.setEnabled(enabled);
    confirmBtn.setAlpha(enabled ? 1.0f : 0.35f);
}

void StardustDialog::doConfirm()
{
    auto text = textInput.getText().trim();
    text = text.removeCharacters("/\\:*?\"<>|");
    text = text.substring(0, 32);
    if (text.isEmpty()) return;

    if (onConfirmWithBank)
        onConfirmWithBank(text, getSelectedBank());
    else if (callback)
        callback(text);

    if (onDismiss) onDismiss();
}

void StardustDialog::setBankOptions(const std::vector<juce::String>& banks)
{
    bankOptions = banks;
    bankCombo = std::make_unique<juce::ComboBox>();
    bankCombo->addItem("User", 1);
    for (int i = 0; i < static_cast<int>(banks.size()); ++i)
        bankCombo->addItem(banks[static_cast<size_t>(i)], i + 2);
    bankCombo->addItem("+ New Bank...", static_cast<int>(banks.size()) + 2);
    bankCombo->setSelectedId(1, juce::dontSendNotification);
    bankCombo->setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xFF141414));
    bankCombo->setColour(juce::ComboBox::textColourId, StardustLookAndFeel::kFg);
    bankCombo->setColour(juce::ComboBox::outlineColourId, StardustLookAndFeel::kFgGhost.withAlpha(0.35f));
    bankCombo->setColour(juce::ComboBox::focusedOutlineColourId, StardustLookAndFeel::kAccent.withAlpha(0.4f));
    bankCombo->setColour(juce::ComboBox::arrowColourId, StardustLookAndFeel::kFgDim);
    bankCombo->setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(bankCombo.get());

    newBankInput = std::make_unique<juce::TextEditor>();
    newBankInput->setFont(juce::FontOptions(12.0f));
    newBankInput->setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xFF141414));
    newBankInput->setColour(juce::TextEditor::textColourId, StardustLookAndFeel::kAccent);
    newBankInput->setColour(juce::TextEditor::outlineColourId, StardustLookAndFeel::kFgGhost.withAlpha(0.35f));
    newBankInput->setColour(juce::TextEditor::focusedOutlineColourId, StardustLookAndFeel::kAccent.withAlpha(0.4f));
    newBankInput->setColour(juce::CaretComponent::caretColourId, StardustLookAndFeel::kAccent);
    newBankInput->setTextToShowWhenEmpty("New bank name...", StardustLookAndFeel::kFgGhost);
    newBankInput->setIndents(8, 8);
    newBankInput->onTextChange = [this] { updateSaveButton(); };
    addAndMakeVisible(newBankInput.get());
    newBankInput->setVisible(false);

    bankCombo->onChange = [this] {
        const bool isNewBank = (bankCombo->getSelectedId() == static_cast<int>(bankOptions.size()) + 2);
        newBankInput->setVisible(isNewBank);
        updateSaveButton();
        resized();
    };

    resized();
}

void StardustDialog::selectBank(int comboId)
{
    if (bankCombo != nullptr)
        bankCombo->setSelectedId(comboId, juce::dontSendNotification);
}

juce::String StardustDialog::getSelectedBank() const
{
    if (bankCombo == nullptr)
        return {};

    const int id = bankCombo->getSelectedId();
    if (id == 1)
        return {}; // User (root)

    const int newBankId = static_cast<int>(bankOptions.size()) + 2;
    if (id == newBankId && newBankInput != nullptr)
    {
        auto name = newBankInput->getText().trim();
        return name.removeCharacters("/\\:*?\"<>|").substring(0, 64);
    }

    // Existing bank
    const int bankIdx = id - 2;
    if (bankIdx >= 0 && bankIdx < static_cast<int>(bankOptions.size()))
        return bankOptions[static_cast<size_t>(bankIdx)];

    return {};
}

// ============================================================================
// SettingsPanel
// ============================================================================

SettingsPanel::SettingsPanel(StardustEditor& editor) : editorRef(editor)
{
    setOpaque(true);

    // Tab buttons
    generalTabBtn.setButtonText("GENERAL");
    generalTabBtn.setColour(juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    generalTabBtn.setColour(juce::TextButton::textColourOffId, StardustLookAndFeel::kFg);
    generalTabBtn.onClick = [this] { selectTab(Tab::General); };
    addAndMakeVisible(generalTabBtn);

    aboutTabBtn.setButtonText("ABOUT");
    aboutTabBtn.setColour(juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    aboutTabBtn.setColour(juce::TextButton::textColourOffId, StardustLookAndFeel::kFg);
    aboutTabBtn.onClick = [this] { selectTab(Tab::About); };
    addAndMakeVisible(aboutTabBtn);

    // Close button
    closeBtn.setButtonText("X");
    closeBtn.setColour(juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    closeBtn.setColour(juce::TextButton::textColourOffId, StardustLookAndFeel::kFg);
    closeBtn.onClick = [this] { editorRef.hideSettings(); };
    addAndMakeVisible(closeBtn);

    // Scale dropdown
    scaleCombo.addItemList({ "75%", "100%", "125%", "150%", "175%", "200%" }, 1);
    scaleCombo.setJustificationType(juce::Justification::centred);
    scaleCombo.setColour(juce::ComboBox::outlineColourId, StardustLookAndFeel::kFgGhost.withAlpha(0.4f));
    scaleCombo.setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xFF151515));
    // Select current scale
    const float scales[] = { 0.75f, 1.0f, 1.25f, 1.5f, 1.75f, 2.0f };
    for (int i = 0; i < 6; ++i)
        if (std::abs(editorRef.getCurrentScale() - scales[i]) < 0.05f)
            scaleCombo.setSelectedId(i + 1, juce::dontSendNotification);
    scaleCombo.onChange = [this, scales]() {
        const int idx = scaleCombo.getSelectedId() - 1;
        if (idx >= 0 && idx < 6)
            editorRef.applyScale(scales[idx]);
    };
    addAndMakeVisible(scaleCombo);

    // Reset to factory
    resetFactoryBtn.setButtonText("Reset to Factory");
    resetFactoryBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xFF8B2020));
    resetFactoryBtn.setColour(juce::TextButton::textColourOffId, StardustLookAndFeel::kFg);
    resetFactoryBtn.onClick = [this] { editorRef.resetToFactory(); };
    addAndMakeVisible(resetFactoryBtn);

    selectTab(Tab::General);
}

void SettingsPanel::selectTab(Tab tab)
{
    currentTab = tab;
    const bool isGeneral = (tab == Tab::General);

    generalTabBtn.setColour(juce::TextButton::textColourOffId,
                            isGeneral ? StardustLookAndFeel::kAccent : StardustLookAndFeel::kFgDim);
    aboutTabBtn.setColour(juce::TextButton::textColourOffId,
                          isGeneral ? StardustLookAndFeel::kFgDim : StardustLookAndFeel::kAccent);

    scaleCombo.setVisible(isGeneral);
    resetFactoryBtn.setVisible(isGeneral);

    repaint();
}

void SettingsPanel::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    // Fully opaque solid background — fill entire rect, no rounded corners on fill
    g.fillAll(juce::Colour(0xFF0C0C0C));
    // Border
    g.setColour(StardustLookAndFeel::kFgGhost.withAlpha(0.4f));
    g.drawRect(bounds, 1.5f);

    // Title
    g.setFont(juce::FontOptions(14.0f).withStyle("Bold"));
    g.setColour(StardustLookAndFeel::kAccent);
    g.drawText("SETTINGS", bounds.getX(), bounds.getY() + 10, bounds.getWidth(), 20,
               juce::Justification::centred);

    // Sidebar divider
    const float sidebarW = 100.0f;
    const float headerH = 40.0f;
    g.setColour(StardustLookAndFeel::kFgGhost.withAlpha(0.2f));
    g.drawLine(bounds.getX() + sidebarW, bounds.getY() + headerH,
               bounds.getX() + sidebarW, bounds.getBottom() - 8, 1.0f);
    // Header divider
    g.drawLine(bounds.getX() + 8, bounds.getY() + headerH,
               bounds.getRight() - 8, bounds.getY() + headerH, 1.0f);

    // Content area
    const float contentX = bounds.getX() + sidebarW + 16;
    const float contentY = bounds.getY() + headerH + 16;

    if (currentTab == Tab::General)
    {
        // "Display" section header
        g.setFont(juce::FontOptions(12.0f).withStyle("Bold"));
        g.setColour(StardustLookAndFeel::kFg);
        g.drawText("Display", static_cast<int>(contentX), static_cast<int>(contentY),
                   200, 16, juce::Justification::centredLeft);

        g.setFont(juce::FontOptions(11.0f));
        g.setColour(StardustLookAndFeel::kFgDim);
        g.drawText("Display Zoom", static_cast<int>(contentX), static_cast<int>(contentY + 24),
                   100, 20, juce::Justification::centredLeft);

        // Divider before Reset
        const float resetDivY = contentY + 60;
        g.setColour(StardustLookAndFeel::kFgGhost.withAlpha(0.15f));
        g.drawLine(contentX, resetDivY, bounds.getRight() - 16, resetDivY, 1.0f);

        // Reset to Factory border (drawn manually)
        auto resetBounds = resetFactoryBtn.getBounds().toFloat();
        g.setColour(juce::Colour(0xFFAA3030));
        g.drawRoundedRectangle(resetBounds, 4.0f, 1.5f);
    }
    else if (currentTab == Tab::About)
    {
        // Center content vertically in the content area
        const float contentH = bounds.getBottom() - headerH;
        const float blockH = 22 + 16 + 16 + 12; // title + version + author + gaps
        const float cy = headerH + (contentH - blockH) / 2.0f;

        g.setFont(juce::FontOptions(16.0f).withStyle("Bold"));
        g.setColour(StardustLookAndFeel::kAccent);
        g.drawText("Stardust", static_cast<int>(contentX), static_cast<int>(cy),
                   300, 22, juce::Justification::centredLeft);

        g.setFont(juce::FontOptions(12.0f));
        g.setColour(StardustLookAndFeel::kFgDim);
        g.drawText("Version " STARDUST_VERSION, static_cast<int>(contentX), static_cast<int>(cy + 28),
                   300, 16, juce::Justification::centredLeft);

        g.drawText("by LoudDealers", static_cast<int>(contentX), static_cast<int>(cy + 50),
                   300, 16, juce::Justification::centredLeft);
    }
}

void SettingsPanel::resized()
{
    auto bounds = getLocalBounds();
    const int sidebarW = 100;
    const int headerH = 40;
    const int tabH = 28;
    const int tabPad = 8;

    // Close button
    closeBtn.setBounds(bounds.getRight() - 32, bounds.getY() + 8, 24, 24);

    // Tab buttons in sidebar
    const int tabX = bounds.getX() + 4;
    const int tabY = bounds.getY() + headerH + tabPad;
    generalTabBtn.setBounds(tabX, tabY, sidebarW - 8, tabH);
    aboutTabBtn.setBounds(tabX, tabY + tabH + 4, sidebarW - 8, tabH);

    // Content area
    const int contentX = bounds.getX() + sidebarW + 16;
    const int contentY = bounds.getY() + headerH + 16;

    // Scale dropdown — next to "Display Zoom" label
    scaleCombo.setBounds(contentX + 110, contentY + 22, 100, 22);

    // Reset button — below divider
    resetFactoryBtn.setBounds(contentX, contentY + 70, 160, 28);
}

// ============================================================================
// StardustEditor
// ============================================================================

StardustEditor::StardustEditor(StardustProcessor& p)
    : AudioProcessorEditor(&p),
      processorRef(p),
      starfield(p.apvts, p.outputLevelLeft, p.outputLevelRight)
{
    setLookAndFeel(&lookAndFeel);
    setSize(860, 780);

    addAndMakeVisible(starfield);
    starfield.setExcludeRect({});
    starfield.toBack();



    setupKnob(destroyInKnob, "destroyIn", "In");
    setupKnob(cutoffKnob, "filterCutoff", "Filter");
    setupKnob(destroyOutKnob, "destroyOut", "Out");
    setupKnob(destroyMixKnob, "destroyMix", "Mix");
    setupKnob(filterLfoKnob, "filterLfo", "LFO");

    // SP-950 style vertical fader with RPM snap points
    destroyFader.setSliderStyle(juce::Slider::LinearHorizontal);
    destroyFader.setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);
    destroyFader.setName("destroyFader");
    destroyFaderAttachment = std::make_unique<SliderAttachment>(processorRef.apvts, "destroyFader", destroyFader);
    // Snap to nearest semitone tick (reversed: 0=78RPM, 1=x2, 2=45RPM, 3=33RPM)
    // 78→x2: 2 ticks, x2→45: 12 ticks, 45→33: 5 ticks
    destroyFader.onValueChange = [this]() {
        const double raw = destroyFader.getValue();
        const int seg = juce::jlimit(0, 2, static_cast<int>(raw));
        const double segFrac = raw - static_cast<double>(seg);
        static constexpr int kTicks[3] = { 2, 12, 5 };
        const double step = 1.0 / static_cast<double>(kTicks[seg]);
        const double snappedFrac = std::round(segFrac / step) * step;
        const double snapped = juce::jlimit(0.0, 3.0, static_cast<double>(seg) + snappedFrac);
        if (std::abs(raw - snapped) > 0.001)
            destroyFader.setValue(snapped, juce::sendNotificationAsync);
    };
    addAndMakeVisible(destroyFader);

    setupKnob(grainMixKnob, "grainMix", "Mix");
    setupKnob(grainCloudKnob, "grainCloud", "Cloud");
    setupKnob(grainDriftKnob, "grainDrift", "Drift");
    setupKnob(grainSpaceKnob, "grainSpace", "Space");
    setupKnob(grainMorphKnob, "grainMorph", "Pitch");

    // Freeze toggle
    {
        auto setupFreezeToggle = [this](juce::ToggleButton& btn, std::unique_ptr<ButtonAttachment>& attach,
                                         const juce::String& paramId) {
            btn.setClickingTogglesState(true);
            attach = std::make_unique<ButtonAttachment>(processorRef.apvts, paramId, btn);
            addAndMakeVisible(btn);
        };
        setupFreezeToggle(grainFreezeToggle, grainFreezeAttach, "grainFreeze");
    }

    freezeLabel.setText("F R E E Z E", juce::dontSendNotification);
    freezeLabel.setFont(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(), 9.0f, juce::Font::plain));
    freezeLabel.setColour(juce::Label::textColourId, StardustLookAndFeel::kFgDim);
    freezeLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(freezeLabel);

    setupKnob(chorusMixKnob, "chorusMix", "Mix");
    setupKnob(chorusSpeedKnob, "chorusSpeed", "Speed");
    setupKnob(panOuterKnob, "multiplyPanOuter", "1+2");
    setupKnob(panInnerKnob, "multiplyPanInner", "3+4");

    setupKnob(tapeWowKnob, "tapeWow", "Wow");
    setupKnob(tapeFlutterKnob, "tapeFlutter", "Flutter");
    setupKnob(tapeHissKnob, "tapeHiss", "Hiss");

    // Right side knobs: Input, Output, Dry/Wet
    setupKnob(inputGainKnob, "inputGain", "Input");
    setupKnob(outputGainKnob, "outputGain", "Output");
    setupKnob(masterMixKnob, "masterMix", "Dry/Wet");

    refreshPresetList();
    presetSelector.setRepaintsOnMouseActivity(true);
    presetSelector.setMouseClickGrabsKeyboardFocus(false);
    presetSelector.setInterceptsMouseClicks(false, false);
    addAndMakeVisible(presetSelector);

    // Force ComboBox repaint at 30Hz so hover state always reflects actual mouse position
    startTimerHz(30);

    // Preset navigation buttons
    prevPresetBtn.setButtonText("<");
    prevPresetBtn.setColour(juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    prevPresetBtn.setColour(juce::TextButton::textColourOffId, StardustLookAndFeel::kFg);
    prevPresetBtn.onClick = [this] {
        int idx = processorRef.getCurrentProgram() - 1;
        if (idx < 0) idx = processorRef.getPresetCount() - 1;
        processorRef.loadPreset(idx);
        presetSelector.setSelectedId(idx + 1, juce::dontSendNotification);
        presetSelector.setName("");
        updateDoubleClickDefaults();
        updateFavoriteButton();
    };
    addAndMakeVisible(prevPresetBtn);

    nextPresetBtn.setButtonText(">");
    nextPresetBtn.setColour(juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    nextPresetBtn.setColour(juce::TextButton::textColourOffId, StardustLookAndFeel::kFg);
    nextPresetBtn.onClick = [this] {
        int idx = processorRef.getCurrentProgram() + 1;
        if (idx >= processorRef.getPresetCount()) idx = 0;
        processorRef.loadPreset(idx);
        presetSelector.setSelectedId(idx + 1, juce::dontSendNotification);
        presetSelector.setName("");
        updateDoubleClickDefaults();
        updateFavoriteButton();
    };
    addAndMakeVisible(nextPresetBtn);

    savePresetBtn.setButtonText("...");
    savePresetBtn.setColour(juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    savePresetBtn.setColour(juce::TextButton::textColourOffId, StardustLookAndFeel::kFg);
    savePresetBtn.onClick = [this] {
        juce::PopupMenu menu;
        menu.setLookAndFeel(&lookAndFeel);

        menu.addItem(1, "Save as...");
        int idx = processorRef.getCurrentProgram();
        bool isFactory = processorRef.isFactoryPreset(idx);
        menu.addItem(2, "Rename...", !isFactory);
        menu.addItem(3, "Delete", !isFactory);
        menu.addSeparator();
        menu.addItem(4, "Initialize");

        menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&savePresetBtn),
            [this](int result) {
                if (result == 1)
                {
                    activeDialog = std::make_unique<StardustDialog>("Save Preset", "", std::function<void(const juce::String&)>{});
                    activeDialog->setBankOptions(processorRef.getUserBanks());
                    // Pre-select current preset's bank
                    {
                        int idx2 = processorRef.getCurrentProgram();
                        PresetLockGuard guard(processorRef.presetLock);
                        const auto& presets = processorRef.getAllPresets();
                        if (idx2 >= 0 && idx2 < static_cast<int>(presets.size()))
                        {
                            const auto& currentBank = presets[static_cast<size_t>(idx2)].bank;
                            if (currentBank.isNotEmpty())
                            {
                                auto banks = processorRef.getUserBanks();
                                for (int b = 0; b < static_cast<int>(banks.size()); ++b)
                                    if (banks[static_cast<size_t>(b)] == currentBank)
                                        activeDialog->selectBank(b + 2);
                            }
                        }
                    }
                    activeDialog->onConfirmWithBank = [this](const juce::String& name, const juce::String& bank) {
                        processorRef.saveUserPreset(name, bank);
                        refreshPresetList();
                        for (int i = 0; i < processorRef.getPresetCount(); ++i)
                        {
                            if (processorRef.getProgramName(i) == name)
                            {
                                presetSelector.setSelectedId(i + 1, juce::dontSendNotification);
                                processorRef.loadPreset(i);
                                break;
                            }
                        }
                    };
                    activeDialog->onDismiss = [this] { activeDialog.reset(); };
                    addAndMakeVisible(activeDialog.get());
                    activeDialog->setCentrePosition(getWidth() / 2, getHeight() / 2);
                    activeDialog->toFront(true);
                    activeDialog->grabKeyboardFocus();
                }
                else if (result == 2)
                {
                    int idx2 = processorRef.getCurrentProgram();
                    auto oldName = processorRef.getProgramName(idx2);
                    juce::String presetBank;
                    {
                        PresetLockGuard g(processorRef.presetLock);
                        const auto& presets = processorRef.getAllPresets();
                        if (idx2 >= 0 && idx2 < static_cast<int>(presets.size()))
                            presetBank = presets[static_cast<size_t>(idx2)].bank;
                    }
                    activeDialog = std::make_unique<StardustDialog>("Rename Preset", oldName, [this, oldName, presetBank](const juce::String& newName) {
                        processorRef.saveUserPreset(newName, presetBank);
                        auto dir = StardustProcessor::getUserPresetsDir();
                        if (presetBank.isNotEmpty())
                            dir = dir.getChildFile(presetBank);
                        auto oldFile = dir.getChildFile(oldName + ".xml");
                        if (oldFile.existsAsFile())
                            oldFile.deleteFile();
                        processorRef.refreshPresets();
                        refreshPresetList();
                        presetSelector.setSelectedId(processorRef.getPresetCount(), juce::dontSendNotification);
                        processorRef.loadPreset(processorRef.getPresetCount() - 1);
                    });
                    activeDialog->confirmBtn.setButtonText("Rename");
                    activeDialog->onDismiss = [this] { activeDialog.reset(); };
                    addAndMakeVisible(activeDialog.get());
                    activeDialog->setCentrePosition(getWidth() / 2, getHeight() / 2);
                    activeDialog->toFront(true);
                    activeDialog->grabKeyboardFocus();
                }
                else if (result == 3)
                {
                    int idx2 = processorRef.getCurrentProgram();
                    processorRef.deleteUserPreset(idx2);
                    refreshPresetList();
                    presetSelector.setSelectedId(1, juce::dontSendNotification);
                    processorRef.loadPreset(0);
                }
                else if (result == 4)
                {
                    // Initialize — load factory Default
                    processorRef.loadPreset(0);
                    refreshPresetList();
                    presetSelector.setSelectedId(1, juce::dontSendNotification);
                }
            });
    };
    addAndMakeVisible(savePresetBtn);

    // Scale/gear button
    scaleBtn.setComponentID("scaleBtn");
    scaleBtn.setButtonText("");
    scaleBtn.setColour(juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    scaleBtn.onClick = [this] { showSettings(); };
    addAndMakeVisible(scaleBtn);

    // Library icon button
    libraryBtn.setComponentID("libraryBtn");
    libraryBtn.setButtonText("");
    libraryBtn.setColour(juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    libraryBtn.onClick = [this] { showPresetLibrary(); };
    addAndMakeVisible(libraryBtn);

    favoriteBtn.setComponentID("favoriteBtn");
    favoriteBtn.setColour(juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    favoriteBtn.setColour(juce::TextButton::textColourOffId, StardustLookAndFeel::kFg);
    favoriteBtn.onClick = [this] {
        juce::String name;
        {
            PresetLockGuard guard(processorRef.presetLock);
            const auto& presets = processorRef.getAllPresets();
            int idx = processorRef.getCurrentProgram();
            if (idx < 0 || idx >= static_cast<int>(presets.size())) return;
            name = presets[static_cast<size_t>(idx)].name;
        }
        auto favs = StardustProcessor::loadFavorites();
        if (favs.count(name) > 0) favs.erase(name);
        else                       favs.insert(name);
        StardustProcessor::saveFavorites(favs);
        updateFavoriteButton();
        if (presetLibraryPanel != nullptr && presetLibraryPanel->isVisible())
            presetLibraryPanel->setFavorites(favs);
    };
    addAndMakeVisible(favoriteBtn);
    updateFavoriteButton();

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
    setupToggle(tapeToggle, tapeToggleAttach, "tapeEnabled");

    // Disable and dim knobs when their section toggle is off
    auto dimSection = [](juce::ToggleButton& toggle, std::initializer_list<LabeledKnob*> knobs) {
        toggle.onClick = [&toggle, knobs = std::vector<LabeledKnob*>(knobs)]() {
            const bool on = toggle.getToggleState();
            const float alpha = on ? 1.0f : 0.4f;
            for (auto* k : knobs)
            {
                k->slider.setEnabled(on);
                k->slider.setAlpha(alpha);
                k->label.setAlpha(alpha);
            }
        };
    };
    dimSection(destroyToggle, { &destroyInKnob, &cutoffKnob, &destroyOutKnob, &destroyMixKnob, &filterLfoKnob });
    // Also dim the fader with the destroy section
    {
        auto& toggle = destroyToggle;
        auto* fader = &destroyFader;
        auto origOnClick = toggle.onClick;
        toggle.onClick = [origOnClick, &toggle, fader]() {
            if (origOnClick) origOnClick();
            const bool on = toggle.getToggleState();
            fader->setEnabled(on);
            fader->setAlpha(on ? 1.0f : 0.4f);
        };
    }
    dimSection(granularToggle, { &grainMixKnob, &grainCloudKnob, &grainDriftKnob, &grainSpaceKnob, &grainMorphKnob });
    // Also disable freeze toggle with the granular section
    {
        auto& toggle = granularToggle;
        auto* freezeBtn = &grainFreezeToggle;
        auto* freezeLbl = &freezeLabel;
        auto origOnClick = toggle.onClick;
        toggle.onClick = [origOnClick, &toggle, freezeBtn, freezeLbl]() {
            if (origOnClick) origOnClick();
            const bool on = toggle.getToggleState();
            freezeBtn->setEnabled(on);
            freezeBtn->setAlpha(on ? 1.0f : 0.4f);
            freezeLbl->setAlpha(on ? 1.0f : 0.4f);
        };
    }
    dimSection(multiplyToggle, { &chorusMixKnob, &chorusSpeedKnob, &panOuterKnob, &panInnerKnob });
    dimSection(tapeToggle, { &tapeWowKnob, &tapeFlutterKnob, &tapeHissKnob });

    // Apply initial dim state
    destroyToggle.onClick();
    granularToggle.onClick();
    multiplyToggle.onClick();
    tapeToggle.onClick();

    logoImage = juce::ImageCache::getFromMemory(BinaryData::logo_png, BinaryData::logo_pngSize);

    generateBackgroundTexture();

    // Listen to all parameter changes for dirty state tracking
    for (auto* param : processorRef.getParameters())
    {
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*>(param))
            processorRef.apvts.addParameterListener(ranged->getParameterID(), this);
    }

    // Clear dirty flag — construction/attachment triggers parameterChanged spuriously
    processorRef.presetDirty.store(false);
}

StardustEditor::~StardustEditor()
{
    for (auto* param : processorRef.getParameters())
    {
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*>(param))
            processorRef.apvts.removeParameterListener(ranged->getParameterID(), this);
    }
    setLookAndFeel(nullptr);
}

void StardustEditor::setupKnob(LabeledKnob& knob, const juce::String& paramId,
                               const juce::String& labelText)
{
    knob.slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    knob.slider.setTextBoxStyle(juce::Slider::TextBoxBelow, true, 70, 16);
    knob.slider.setColour(juce::Slider::textBoxTextColourId, StardustLookAndFeel::kFg);
    knob.slider.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    knob.slider.setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);

    knob.slider.setName(paramId);
    knob.attachment = std::make_unique<SliderAttachment>(processorRef.apvts, paramId, knob.slider);

    if (auto* param = processorRef.apvts.getParameter(paramId))
    {
        const float defaultVal = param->convertFrom0to1(param->getDefaultValue());
        knob.slider.setDoubleClickReturnValue(true, defaultVal);
    }

    // Set value formatting AFTER attachment (attachment overwrites textFromValueFunction)
    if (paramId == "grainSize")
        knob.slider.textFromValueFunction = [](double v) {
            return juce::String(static_cast<int>(std::round(v))) + " ms";
        };
    else if (paramId == "grainDensity")
        knob.slider.textFromValueFunction = [](double v) {
            return juce::String(static_cast<int>(std::round(v)));
        };
    else if (paramId == "outputGain" || paramId == "inputGain"
             || paramId == "destroyIn" || paramId == "destroyOut")
        knob.slider.textFromValueFunction = [](double v) {
            if (v > 0.05) return juce::String("+") + juce::String(v, 1) + " dB";
            if (v < -0.05) return juce::String(v, 1) + " dB";
            return juce::String("0.0 dB");
        };
    else if (paramId == "grainPitch")
        knob.slider.textFromValueFunction = [](double v) {
            const int st = static_cast<int>(std::round(v));
            if (st > 0) return juce::String("+") + juce::String(st) + " st";
            if (st < 0) return juce::String(st) + " st";
            return juce::String("0 st");
        };
    else if (paramId == "chorusSpeed")
        knob.slider.textFromValueFunction = [](double v) {
            return juce::String(v, 1) + " Hz";
        };
    else if (paramId == "filterCutoff")
        knob.slider.textFromValueFunction = [](double v) {
            return juce::String(static_cast<int>(std::round(v * 99.0)));
        };
    else if (paramId == "destroyMix" || paramId == "filterLfo"
             || paramId == "grainMix"
             || paramId == "grainScatter" || paramId == "stereoWidth"
             || paramId == "chorusMix"
             || paramId == "multiplyPanOuter" || paramId == "multiplyPanInner"
             || paramId == "masterMix"
             || paramId == "tapeWow" || paramId == "tapeFlutter" || paramId == "tapeHiss"
             || paramId == "grainPosition" || paramId == "grainFeedback"
             || paramId == "grainTexture")
        knob.slider.textFromValueFunction = [](double v) {
            return juce::String(static_cast<int>(std::round(v * 100.0))) + "%";
        };
    else if (paramId == "grainMorph")
        knob.slider.textFromValueFunction = [](double v) {
            const int st = static_cast<int>(std::round((v - 0.5) * 24.0));
            if (st > 0) return juce::String("+") + juce::String(st) + " st";
            if (st < 0) return juce::String(st) + " st";
            return juce::String("0 st");
        };

    // Force text refresh with the new formatter
    knob.slider.updateText();
    paramToKnob[paramId] = &knob;
    addAndMakeVisible(knob.slider);

    // Set monospace font on the value text box
    for (int i = 0; i < knob.slider.getNumChildComponents(); ++i)
    {
        if (auto* label = dynamic_cast<juce::Label*>(knob.slider.getChildComponent(i)))
        {
            label->setFont(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(), 10.0f, juce::Font::plain));
            break;
        }
    }

    knob.label.setText(labelText, juce::dontSendNotification);
    knob.label.setJustificationType(juce::Justification::centred);
    knob.label.setFont(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(), 11.0f, juce::Font::plain));
    knob.label.setColour(juce::Label::textColourId, StardustLookAndFeel::kFg);
    addAndMakeVisible(knob.label);

    // Set value text box font to match "Tune: X st" style (13px bold monospace)
    for (int i = 0; i < knob.slider.getNumChildComponents(); ++i)
    {
        if (auto* label = dynamic_cast<juce::Label*>(knob.slider.getChildComponent(i)))
        {
            label->setFont(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(), 13.0f, juce::Font::bold));
            break;
        }
    }
}

void StardustEditor::layoutKnobInBounds(LabeledKnob& knob, juce::Rectangle<int> bounds)
{
    const auto labelHeight = 14;
    knob.label.setBounds(bounds.removeFromTop(labelHeight));
    knob.slider.setBounds(bounds);
}

void StardustEditor::showPresetDropdown()
{
    // Copy preset metadata under lock, then release before file I/O
    struct PresetInfo { juce::String name; bool isFactory; };
    std::vector<PresetInfo> presetInfos;
    int currentIdx;
    {
        PresetLockGuard guard(processorRef.presetLock);
        const auto& presets = processorRef.getAllPresets();
        currentIdx = processorRef.getCurrentProgram();
        presetInfos.reserve(presets.size());
        for (const auto& p : presets)
            presetInfos.push_back({ p.name, p.isFactory });
    }
    auto favs = StardustProcessor::loadFavorites();

    juce::PopupMenu favMenu, factoryMenu, userMenu;

    for (int i = 0; i < static_cast<int>(presetInfos.size()); ++i)
    {
        const int itemId = i + 1;
        const bool ticked = (i == currentIdx);
        if (favs.count(presetInfos[static_cast<size_t>(i)].name) > 0)
            favMenu.addItem(itemId, presetInfos[static_cast<size_t>(i)].name, true, ticked);
        if (presetInfos[static_cast<size_t>(i)].isFactory)
            factoryMenu.addItem(itemId, presetInfos[static_cast<size_t>(i)].name, true, ticked);
        else
            userMenu.addItem(itemId, presetInfos[static_cast<size_t>(i)].name, true, ticked);
    }

    juce::PopupMenu menu;
    if (favMenu.getNumItems() > 0)
        menu.addSubMenu(juce::String::charToString(0x2605) + " Favorites", favMenu);
    if (factoryMenu.getNumItems() > 0)
        menu.addSubMenu("Factory", factoryMenu);
    if (userMenu.getNumItems() > 0)
        menu.addSubMenu("User", userMenu);

    // Apply our look and feel + position below the preset selector
    menu.setLookAndFeel(&lookAndFeel);
    favMenu.setLookAndFeel(&lookAndFeel);
    factoryMenu.setLookAndFeel(&lookAndFeel);
    userMenu.setLookAndFeel(&lookAndFeel);
    auto opts = juce::PopupMenu::Options()
        .withTargetComponent(&presetSelector)
        .withPreferredPopupDirection(juce::PopupMenu::Options::PopupDirection::downwards)
        .withMinimumWidth(presetSelector.getWidth());

    menu.showMenuAsync(opts, [this](int result) {
        if (result > 0)
        {
            processorRef.loadPreset(result - 1);
            presetSelector.setSelectedId(result, juce::dontSendNotification);
            presetSelector.setName("");
            updateDoubleClickDefaults();
            updateFavoriteButton();
        }
    });
}

void StardustEditor::refreshPresetList()
{
    PresetLockGuard guard(processorRef.presetLock);
    presetSelector.clear(juce::dontSendNotification);
    const auto& presets = processorRef.getAllPresets();

    // Flat list for ComboBox ID mapping (prev/next buttons still use this)
    for (int i = 0; i < static_cast<int>(presets.size()); ++i)
        presetSelector.addItem(presets[static_cast<size_t>(i)].name, i + 1);

    presetSelector.setSelectedId(processorRef.getCurrentProgram() + 1, juce::dontSendNotification);
    updateDoubleClickDefaults();

    // Update library panel if visible
    if (presetLibraryPanel != nullptr && presetLibraryPanel->isVisible())
    {
        std::vector<PresetListItem> factory, user;
        std::map<juce::String, std::vector<PresetListItem>> bankItems;
        for (int i = 0; i < static_cast<int>(presets.size()); ++i)
        {
            PresetListItem item { presets[i].name, i, presets[i].isFactory, false, presets[i].bank };
            if (presets[i].isFactory)
                factory.push_back(item);
            else if (presets[i].bank.isEmpty())
                user.push_back(item);
            else
                bankItems[presets[i].bank].push_back(item);
        }
        presetLibraryPanel->updatePresets(std::move(factory), std::move(user), std::move(bankItems));
        presetLibraryPanel->setCurrentPresetIndex(processorRef.getCurrentProgram());
    }
}

void StardustEditor::showPresetLibrary()
{
    if (presetLibraryPanel == nullptr)
    {
        presetLibraryPanel = std::make_unique<PresetLibraryPanel>();
        presetLibraryPanel->setLookAndFeel(&lookAndFeel);

        presetLibraryPanel->onPresetSelected = [this](int idx) {
            processorRef.loadPreset(idx);
            presetSelector.setSelectedId(idx + 1, juce::dontSendNotification);
            updateDoubleClickDefaults();
            updateFavoriteButton();
            hidePresetLibrary();
        };

        presetLibraryPanel->onClose = [this] { hidePresetLibrary(); };

        presetLibraryPanel->onRenamePreset = [this](int idx, const juce::String& oldName) {
            // Get the bank of the preset being renamed
            juce::String presetBank;
            {
                PresetLockGuard g(processorRef.presetLock);
                const auto& presets = processorRef.getAllPresets();
                if (idx >= 0 && idx < static_cast<int>(presets.size()))
                    presetBank = presets[static_cast<size_t>(idx)].bank;
            }
            activeDialog = std::make_unique<StardustDialog>("Rename Preset", oldName, [this, oldName, presetBank](const juce::String& newName) {
                processorRef.saveUserPreset(newName, presetBank);
                auto dir = StardustProcessor::getUserPresetsDir();
                if (presetBank.isNotEmpty())
                    dir = dir.getChildFile(presetBank);
                auto oldFile = dir.getChildFile(oldName + ".xml");
                if (oldFile.existsAsFile()) oldFile.deleteFile();
                processorRef.refreshPresets();
                refreshPresetList();
            });
            activeDialog->confirmBtn.setButtonText("Rename");
            activeDialog->onDismiss = [this] { activeDialog.reset(); };
            addAndMakeVisible(activeDialog.get());
            activeDialog->setCentrePosition(getWidth() / 2, getHeight() / 2);
            activeDialog->toFront(true);
            activeDialog->grabKeyboardFocus();
        };

        presetLibraryPanel->onDeletePreset = [this](int idx) {
            processorRef.deleteUserPreset(idx);
            refreshPresetList();
            presetSelector.setSelectedId(1, juce::dontSendNotification);
            processorRef.loadPreset(0);
        };

        presetLibraryPanel->onToggleFavorite = [this](const juce::String& name, bool fav) {
            auto favs = StardustProcessor::loadFavorites();
            if (fav) favs.insert(name);
            else     favs.erase(name);
            StardustProcessor::saveFavorites(favs);
            presetLibraryPanel->setFavorites(favs);
        };

        presetLibraryPanel->onImportBank = [this](const juce::File& folder) {
            processorRef.importBank(folder);
            refreshPresetList();
            showPresetLibrary();
        };

        presetLibraryPanel->onDeleteBank = [this](const juce::String& bankName) {
            processorRef.deleteUserBank(bankName);
            refreshPresetList();
            showPresetLibrary();
        };

        presetLibraryPanel->onRenameBank = [this](const juce::String& oldName, const juce::String& newName) {
            processorRef.renameUserBank(oldName, newName);
            refreshPresetList();
            showPresetLibrary();
        };
    }

    // Build items — copy preset data under lock, file I/O after release
    std::vector<PresetListItem> factory, user;
    std::map<juce::String, std::vector<PresetListItem>> bankItems;
    {
        PresetLockGuard guard(processorRef.presetLock);
        const auto& presets = processorRef.getAllPresets();
        for (int i = 0; i < static_cast<int>(presets.size()); ++i)
        {
            PresetListItem item { presets[i].name, i, presets[i].isFactory, false, presets[i].bank };
            if (presets[i].isFactory)
                factory.push_back(item);
            else if (presets[i].bank.isEmpty())
                user.push_back(item);
            else
                bankItems[presets[i].bank].push_back(item);
        }
    }
    auto favs = StardustProcessor::loadFavorites();
    for (auto& item : factory) item.isFavorite = favs.count(item.name) > 0;
    for (auto& item : user) item.isFavorite = favs.count(item.name) > 0;
    for (auto& [bk, items] : bankItems)
        for (auto& item : items) item.isFavorite = favs.count(item.name) > 0;
    presetLibraryPanel->updatePresets(std::move(factory), std::move(user), std::move(bankItems));
    presetLibraryPanel->setCurrentPresetIndex(processorRef.getCurrentProgram());

    // Position: overlay on left side, from galaxy top to controls bottom
    const int panelW = 300;
    const int panelH = controlsBounds.getBottom() - galaxyBounds.getY();
    presetLibraryPanel->setBounds(galaxyBounds.getX(), galaxyBounds.getY(), panelW, panelH);

    addAndMakeVisible(presetLibraryPanel.get());
    presetLibraryPanel->toFront(true);
    presetLibraryPanel->grabKeyboardFocus();
}

void StardustEditor::hidePresetLibrary()
{
    if (presetLibraryPanel != nullptr)
        presetLibraryPanel->setVisible(false);
}

void StardustEditor::showSettings()
{
    if (settingsPanel == nullptr)
    {
        settingsPanel = std::make_unique<SettingsPanel>(*this);
        addAndMakeVisible(*settingsPanel);
    }
    // Center the panel in the editor
    const int panelW = 420;
    const int panelH = 250;
    settingsPanel->setBounds((getWidth() - panelW) / 2, (getHeight() - panelH) / 2, panelW, panelH);
    settingsPanel->setVisible(true);
    settingsPanel->toFront(true);
}

void StardustEditor::hideSettings()
{
    if (settingsPanel != nullptr)
        settingsPanel->setVisible(false);
}

void StardustEditor::applyScale(float scale)
{
    currentScale = scale;
    setScaleFactor(scale);
}

void StardustEditor::resetToFactory()
{
    // Load the first preset (Default)
    processorRef.loadPreset(0);
    refreshPresetList();
    updateDoubleClickDefaults();
    updateFavoriteButton();
}

void StardustEditor::updateFavoriteButton()
{
    PresetLockGuard guard(processorRef.presetLock);
    const auto& presets = processorRef.getAllPresets();
    int idx = processorRef.getCurrentProgram();
    bool isFav = false;
    if (idx >= 0 && idx < static_cast<int>(presets.size()))
    {
        auto favs = StardustProcessor::loadFavorites();
        isFav = favs.count(presets[static_cast<size_t>(idx)].name) > 0;
    }
    favoriteBtn.setButtonText(isFav ? juce::String::charToString(0x2605)    // ★
                                    : juce::String::charToString(0x2606));  // ☆
    favoriteBtn.setColour(juce::TextButton::textColourOffId,
                          isFav ? StardustLookAndFeel::kAccent : StardustLookAndFeel::kFg);
    favoriteBtn.repaint();
}

void StardustEditor::updateDoubleClickDefaults()
{
    PresetLockGuard guard(processorRef.presetLock);
    const int procIdx = processorRef.getCurrentProgram();
    const auto& allPresets = processorRef.getAllPresets();
    if (procIdx < 0 || procIdx >= static_cast<int>(allPresets.size()))
        return;

    const auto& preset = allPresets[static_cast<size_t>(procIdx)];
    for (const auto& [paramId, presetVal] : preset.values)
    {
        auto it = paramToKnob.find(paramId);
        if (it != paramToKnob.end())
            it->second->slider.setDoubleClickReturnValue(true, presetVal);
    }
}

void StardustEditor::generateBackgroundTexture()
{
    constexpr int texW = 128;
    constexpr int texH = 128;
    backgroundTexture = juce::Image(juce::Image::ARGB, texW, texH, true);

    juce::Random rng(42); // fixed seed for deterministic texture

    // Sparse dim dots — lo-fi feel matching the dithered visualizer
    const int numDots = 1350;
    for (int i = 0; i < numDots; ++i)
    {
        const int dx = rng.nextInt(texW);
        const int dy = rng.nextInt(texH);
        const float brightness = 0.02f + rng.nextFloat() * 0.04f;
        const auto alpha = static_cast<uint8_t>(brightness * 255.0f);

        backgroundTexture.setPixelAt(dx, dy, juce::Colour::fromRGBA(255, 255, 255, alpha));
    }
}

void StardustEditor::paint(juce::Graphics& g)
{
    // Static black background
    g.fillAll(juce::Colours::black);

    // ---- Background texture (excludes visualizer screen and bottom bar) ----
    if (backgroundTexture.isValid())
    {
        const auto totalBounds = getLocalBounds();
        const auto screenf = screenBounds.toFloat();
        const auto bbf = bottomBarBounds.toFloat();

        // Dots in the background gaps between sections (not inside panels)
        juce::RectangleList<int> clipRegion(totalBounds);
        clipRegion.subtract(galaxyBounds);
        clipRegion.subtract(controlsBounds);
        clipRegion.subtract(bottomBarBounds);

        juce::Graphics::ScopedSaveState saveState(g);
        g.reduceClipRegion(clipRegion);

        const int texW = backgroundTexture.getWidth();
        const int texH = backgroundTexture.getHeight();

        for (int ty = 0; ty < totalBounds.getHeight(); ty += texH)
        {
            for (int tx = 0; tx < totalBounds.getWidth(); tx += texW)
            {
                g.drawImageAt(backgroundTexture, tx, ty);
            }
        }
    }

    // ---- Controls panel — border only, black bg shows through with dot texture ----
    const auto cpf = controlsBounds.toFloat();

    g.setColour(StardustLookAndFeel::kFgGhost.withAlpha(0.65f));
    g.drawRoundedRectangle(cpf, 4.0f, 2.0f);

    // Grid layout constants (must match resized())
    const int headerH = 14;
    const int headerToKnob = 16;
    const int knobH = 68;
    const int topPad = 16;
    const int sectionBottomPad = 16;
    const int cellPadX = 16;
    const int dividerGap = 1;
    const int panelW = controlsBounds.getWidth();
    const int cellW = (panelW - dividerGap) / 2;
    const int leftX = controlsBounds.getX();
    const int rightX = leftX + cellW + dividerGap;
    const int rightSectionH = topPad + headerH + headerToKnob + knobH + sectionBottomPad;

    const int panelTop = controlsBounds.getY();
    const int rightRow1Top = panelTop;
    const int rightMultiplyH = rightSectionH;
    const int rightMultiplyTop = controlsBounds.getBottom() - rightMultiplyH;

    const int labelOffset = 32;
    const auto drawSectionLabel = [&](const char* name, int cellX, int hdrY) {
        g.setFont(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(), 12.0f, juce::Font::bold));
        g.setColour(StardustLookAndFeel::kAccent);
        g.drawText(name, cellX + cellPadX + labelOffset, hdrY, 200, headerH, juce::Justification::centredLeft);
    };

    // Left column: TAPE at bottom
    const int leftTapeH = rightSectionH;
    const int leftTapeTop = controlsBounds.getBottom() - leftTapeH;

    // Grid dividers
    g.setColour(StardustLookAndFeel::kFgGhost.withAlpha(0.2f));
    const float divX = static_cast<float>(leftX + cellW) + 0.5f;
    g.drawLine(divX, static_cast<float>(panelTop + 8), divX, static_cast<float>(controlsBounds.getBottom() - 8), 1.0f);
    // Horizontal divider on right column between GRANULAR and MULTIPLY
    const float divY1 = static_cast<float>(rightMultiplyTop) - 0.5f;
    g.drawLine(divX + 4, divY1, cpf.getRight() - 8, divY1, 1.0f);
    // Horizontal divider on left column between DESTROY and TAPE
    const float divY3 = static_cast<float>(leftTapeTop) - 0.5f;
    g.drawLine(cpf.getX() + 8, divY3, divX - 4, divY3, 1.0f);

    // Section labels
    drawSectionLabel("D E S T R O Y",       leftX,  panelTop + topPad);
    drawSectionLabel("T A P E",             leftX,  leftTapeTop + topPad);
    drawSectionLabel("G R A N U L A R",     rightX, rightRow1Top + topPad);
    drawSectionLabel("M U L T I P L Y",     rightX, rightMultiplyTop + topPad);

    // ---- Galaxy viewport: VHS display with padded visuals + depth lines ----
    const auto gvf = galaxyBounds.toFloat();
    const auto screenf = screenBounds.toFloat();

    // 1. Galaxy background — dot texture shows through from paint()

    // 2. Padding trapezoids removed — dot texture shows through from paint()

    // 3. Draw ALL borders and lines ON TOP of fills
    const auto borderCol = StardustLookAndFeel::kFgGhost.withAlpha(0.65f);

    // Outer border
    g.setColour(borderCol);
    g.drawRoundedRectangle(gvf, 4.0f, 2.0f);

    // Depth lines
    g.drawLine(gvf.getX(), gvf.getY(), screenf.getX(), screenf.getY(), 2.0f);
    g.drawLine(gvf.getRight(), gvf.getY(), screenf.getRight(), screenf.getY(), 2.0f);
    g.drawLine(gvf.getX(), gvf.getBottom(), screenf.getX(), screenf.getBottom(), 2.0f);
    g.drawLine(gvf.getRight(), gvf.getBottom(), screenf.getRight(), screenf.getBottom(), 2.0f);

    // Preset bar background — drawn here (before children) so icons render on top
    {
        auto presetBarBg = favoriteBtn.getBounds()
            .getUnion(presetSelector.getBounds())
            .getUnion(prevPresetBtn.getBounds())
            .getUnion(nextPresetBtn.getBounds())
            .toFloat().expanded(3.0f, 1.0f);
        g.setColour(juce::Colour(0xFF151515));
        g.fillRoundedRectangle(presetBarBg, 4.0f);
        g.setColour(StardustLookAndFeel::kFgGhost.withAlpha(0.25f));
        g.drawRoundedRectangle(presetBarBg, 4.0f, 1.0f);
    }

    // ---- Bottom bar — border only, dot texture shows through ----
    const auto bbf = bottomBarBounds.toFloat();

    g.setColour(StardustLookAndFeel::kFgGhost.withAlpha(0.65f));
    g.drawRoundedRectangle(bbf, 3.0f, 2.0f);

    // Logo + name on left
    g.setColour(StardustLookAndFeel::kAccent);
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
    g.setColour(StardustLookAndFeel::kAccent);
    g.setFont(juce::FontOptions(10.0f).withStyle("Bold"));
    g.drawText("v" STARDUST_VERSION, bbf.getRight() - 60, bbf.getY(), 50, bbf.getHeight(),
               juce::Justification::centredRight);
}

void StardustEditor::paintOverChildren(juce::Graphics& g)
{
    // Skip overlay drawings if preset library panel is visible (it covers the galaxy area)
    const bool libraryVisible = presetLibraryPanel != nullptr && presetLibraryPanel->isVisible();

    // Clip out opaque overlay panels so paintOverChildren doesn't draw on top of them
    {
        juce::RectangleList<int> clip(getLocalBounds());
        bool needsClip = false;

        if (settingsPanel != nullptr && settingsPanel->isVisible())
        {
            clip.subtract(settingsPanel->getBounds());
            needsClip = true;
        }

        // Clip out any visible StardustDialog children
        for (int i = 0; i < getNumChildComponents(); ++i)
        {
            if (auto* dlg = dynamic_cast<StardustDialog*>(getChildComponent(i)))
            {
                if (dlg->isVisible())
                {
                    clip.subtract(dlg->getBounds());
                    needsClip = true;
                }
            }
        }

        if (needsClip)
            g.reduceClipRegion(clip);
    }

    // Draw inner screen border ON TOP of the starfield child component
    const auto screenf = screenBounds.toFloat();
    const auto gvf = galaxyBounds.toFloat();

    if (!libraryVisible)
    {
        g.setColour(StardustLookAndFeel::kFgGhost.withAlpha(0.65f));
        g.drawRoundedRectangle(screenf, 3.0f, 2.0f);

        // ---- Vertical plugin name (left padding area) ----
        {
            const int leftPadW = screenBounds.getX() - galaxyBounds.getX();
            const float cx = static_cast<float>(galaxyBounds.getX()) + static_cast<float>(leftPadW) * 0.5f;
            const float centerY = static_cast<float>(screenBounds.getY()) +
                static_cast<float>(screenBounds.getHeight()) * 0.5f;

            g.saveState();
            g.addTransform(juce::AffineTransform::rotation(
                -juce::MathConstants<float>::halfPi, cx, centerY));

            g.setFont(juce::FontOptions(juce::Font::getDefaultSansSerifFontName(), 13.0f, juce::Font::bold));
            g.setColour(StardustLookAndFeel::kAccent);
            g.drawText("S  T  A  R  D  U  S  T", static_cast<int>(cx - 80), static_cast<int>(centerY - 8),
                       160, 16, juce::Justification::centred);
            g.restoreState();
        }

        // Star dot pattern is now part of the global background texture

        // Preset bar background + border is drawn in paint() so icons render on top
    }
}

void StardustEditor::parameterChanged(const juce::String& /*parameterID*/, float /*newValue*/)
{
    // Dirty state is now computed by comparing current values to loaded preset in timerCallback
}

void StardustEditor::mouseMove(const juce::MouseEvent& /*e*/)
{
    presetSelector.repaint();
}

void StardustEditor::timerCallback()
{
    presetSelector.repaint();
    repaint(); // refresh signal flow display

    // Sync preset selection with processor (for DAW session restore)
    const int procIdx = processorRef.getCurrentProgram();
    if (presetSelector.getSelectedId() != procIdx + 1)
        presetSelector.setSelectedId(procIdx + 1, juce::dontSendNotification);

    // Grace period after preset load — params may still be settling
    bool dirty = false;
    int grace = processorRef.presetLoadGrace.load();
    if (grace > 0)
    {
        processorRef.presetLoadGrace.compare_exchange_strong(grace, grace - 1);
        // Re-snapshot on last grace frame so we capture settled values
        if (grace == 1)
        {
            PresetLockGuard guard(processorRef.presetLock);
            for (auto& [paramId, normVal] : processorRef.loadedPresetNormValues)
            {
                if (auto* param = processorRef.apvts.getParameter(paramId))
                    normVal = param->getValue();
            }
        }
    }
    else
    {
        PresetLockGuard guard(processorRef.presetLock);
        const auto& snapshot = processorRef.loadedPresetNormValues;
        for (const auto& [paramId, snapshotNorm] : snapshot)
        {
            if (auto* param = processorRef.apvts.getParameter(paramId))
            {
                if (std::abs(param->getValue() - snapshotNorm) > 0.001f)
                {
                    dirty = true;
                    break;
                }
            }
        }
    }

    // Update dirty indicator
    const bool wasDirty = presetSelector.getName() == "dirty";
    if (dirty != wasDirty)
    {
        presetSelector.setName(dirty ? "dirty" : "");
        presetSelector.repaint();
    }
}

void StardustEditor::mouseDown(const juce::MouseEvent& e)
{
    // Dismiss preset library if clicking outside it
    if (presetLibraryPanel != nullptr && presetLibraryPanel->isVisible())
    {
        auto panelBounds = presetLibraryPanel->getBounds();
        if (!panelBounds.contains(e.getPosition()))
        {
            hidePresetLibrary();
            return;
        }
    }

    // Intercept click on preset selector area → show custom dropdown
    if (presetSelector.getBounds().contains(e.getPosition()) && !e.mods.isPopupMenu())
    {
        showPresetDropdown();
        return;
    }

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

void StardustEditor::resized()
{
    const int margin = 8;
    const int gap = 10;

    const int panelW = getWidth() - margin * 2;

    // Bottom bar
    const int barH = 22;
    bottomBarBounds = { margin, getHeight() - margin - barH, panelW, barH };

    // Layout constants for sections (uniform 16px padding on all sides)
    const int knobW = 78;
    const int knobH = 68;
    const int headerH = 14;
    const int headerToKnob = 16;
    const int topPad = 16;
    const int sectionBottomPad = 16;
    const int cellPadX = 16;

    // Grid: 2 columns with a divider gap
    const int dividerGap = 1;
    const int cellW = (panelW - dividerGap) / 2;
    const int leftX = margin;
    const int rightX = margin + cellW + dividerGap;

    // Right column: 3 sections stacked. DESTROY spans full left column.
    const int rightSectionH = topPad + headerH + headerToKnob + knobH + sectionBottomPad;
    const int panelH = rightSectionH * 3 + dividerGap * 2;
    controlsBounds = { margin, bottomBarBounds.getY() - gap - panelH, panelW, panelH };

    // Galaxy viewport: top, between top margin and controls
    const int galaxyY = margin;
    const int galaxyH = controlsBounds.getY() - gap - galaxyY;
    galaxyBounds = { margin, galaxyY, panelW, galaxyH };

    // Screen bounds: padded area inside viewport where visuals render
    const int pad = 64;
    const int topExtraPad = 12;
    screenBounds = galaxyBounds.reduced(pad);
    screenBounds.setTop(screenBounds.getY() + topExtraPad);

    // Starfield renders only in the screen area
    starfield.setBounds(screenBounds);
    starfield.setExcludeRect({});

    // Preset bar: ≡  [☆  Default▾  < >]  ⋯  ⚙
    // Center the bordered group (star + preset + arrows), place outer icons with equal gap from group edges
    const int presetH = 26;
    const int presetY = screenBounds.getBottom() + (galaxyBounds.getBottom() - screenBounds.getBottom() - presetH) / 2;
    const int btnW = 26;
    const int selectorW = 200;
    const int innerGap = 4; // tight spacing inside the bordered group
    const int outerGap = 10; // gap between outer icons and group border edge

    // Bordered group width: star + gap + selector + gap + < + gap + >
    const int groupW = btnW + innerGap + selectorW + innerGap + btnW + innerGap + btnW;
    const int groupX = galaxyBounds.getX() + (galaxyBounds.getWidth() - groupW) / 2;

    // Place bordered group elements
    int gx = groupX;
    favoriteBtn.setBounds(gx, presetY, btnW, presetH);       gx += btnW + innerGap;
    presetSelector.setBounds(gx, presetY, selectorW, presetH);
    presetSelector.setJustificationType(juce::Justification::centred);
                                                               gx += selectorW + innerGap;
    prevPresetBtn.setBounds(gx, presetY, btnW, presetH);     gx += btnW + innerGap;
    nextPresetBtn.setBounds(gx, presetY, btnW, presetH);

    // Outer icons: equal gap from group border (3px expansion accounted for)
    const int groupBorderPad = 3;
    libraryBtn.setBounds(groupX - groupBorderPad - outerGap - btnW, presetY, btnW, presetH);

    int rx = groupX + groupW + groupBorderPad + outerGap;
    savePresetBtn.setBounds(rx, presetY, btnW, presetH);     rx += btnW + outerGap;
    scaleBtn.setBounds(rx, presetY, btnW, presetH);
    deletePresetBtn.setVisible(false);

    presetSelector.onChange = [this] {
        processorRef.loadPreset(presetSelector.getSelectedId() - 1);
        updateDoubleClickDefaults();
        updateFavoriteButton();
    };

    // ---- Grid layout: DESTROY+TAPE left, GRANULAR+MULTIPLY right ----
    const int panelTop = controlsBounds.getY();
    const int rightRow1Top = panelTop;
    // Multiply takes the bottom 1/3 of right column
    const int rightMultiplyH = rightSectionH;
    const int rightMultiplyTop = controlsBounds.getBottom() - rightMultiplyH;
    // Granular takes the top 2/3
    const int rightGranularBot = rightMultiplyTop - dividerGap;

    auto cellGridX = [&](int cellX, int numKnobs) {
        const int totalKnobW = knobW * numKnobs;
        return cellX + cellPadX + (cellW - cellPadX * 2 - totalKnobW) / 2;
    };

    const int toggleH = 14;
    const int toggleW = 26;

    // Left column: DESTROY top, TAPE bottom
    const int leftTapeH = rightSectionH;
    const int leftTapeTop = controlsBounds.getBottom() - leftTapeH;

    // --- LEFT TOP: DESTROY (above TAPE) ---
    {
        const int destroyBot = leftTapeTop - dividerGap;
        const int hdrY = panelTop + topPad;
        const int contentTop = hdrY + headerH + headerToKnob;
        const int contentBot = destroyBot - sectionBottomPad;

        // Knobs in horizontal row at the top
        const int knobRowY = contentTop;
        const int dKnobW = (cellW - cellPadX * 2) / 5;
        const int dOffsetX = leftX + cellPadX;
        layoutKnobInBounds(destroyMixKnob, { dOffsetX,              knobRowY, dKnobW, knobH });
        layoutKnobInBounds(cutoffKnob,     { dOffsetX + dKnobW,     knobRowY, dKnobW, knobH });
        layoutKnobInBounds(filterLfoKnob,  { dOffsetX + dKnobW * 2, knobRowY, dKnobW, knobH });
        layoutKnobInBounds(destroyInKnob,  { dOffsetX + dKnobW * 3, knobRowY, dKnobW, knobH });
        layoutKnobInBounds(destroyOutKnob, { dOffsetX + dKnobW * 4, knobRowY, dKnobW, knobH });

        // Horizontal fader fills space below knobs (full width)
        const int faderX = leftX + cellPadX;
        const int faderW = cellW - cellPadX * 2;
        const int faderTop = knobRowY + knobH + 4;
        const int faderH = contentBot - faderTop;
        destroyFader.setBounds(faderX, faderTop, faderW, faderH);

        destroyToggle.setBounds(leftX + cellPadX, hdrY + 1, toggleW, toggleH);
    }

    // --- LEFT BOTTOM: TAPE (3 knobs) ---
    {
        const int hdrY = leftTapeTop + topPad;
        const int knobY = hdrY + headerH + headerToKnob;
        const int gx = cellGridX(leftX, 3);
        layoutKnobInBounds(tapeWowKnob,     { gx,              knobY, knobW, knobH });
        layoutKnobInBounds(tapeFlutterKnob,  { gx + knobW,      knobY, knobW, knobH });
        layoutKnobInBounds(tapeHissKnob,     { gx + knobW * 2,  knobY, knobW, knobH });
        tapeToggle.setBounds(leftX + cellPadX, hdrY + 1, toggleW, toggleH);
    }

    // --- RIGHT TOP: GRANULAR (2/3 — two rows of knobs) ---
    {
        const int hdrY = rightRow1Top + topPad;
        const int contentBot = rightGranularBot - sectionBottomPad;
        const int gKnobW = (cellW - cellPadX * 2) / 5;
        const int gx = rightX + cellPadX;

        const int row1Y = hdrY + headerH + headerToKnob;
        // 5 macro knobs centered vertically in the 2/3 space
        const int knobMidY = (row1Y + contentBot - knobH) / 2;
        layoutKnobInBounds(grainMixKnob,   { gx,              knobMidY, gKnobW, knobH });
        layoutKnobInBounds(grainCloudKnob, { gx + gKnobW,    knobMidY, gKnobW, knobH });
        layoutKnobInBounds(grainDriftKnob, { gx + gKnobW * 2, knobMidY, gKnobW, knobH });
        layoutKnobInBounds(grainSpaceKnob, { gx + gKnobW * 3, knobMidY, gKnobW, knobH });
        layoutKnobInBounds(grainMorphKnob, { gx + gKnobW * 4, knobMidY, gKnobW, knobH });

        granularToggle.setBounds(rightX + cellPadX, hdrY + 1, toggleW, toggleH);

        // Freeze toggle + label below knobs, centered
        const int freezeToggleW = 36;
        const int freezeToggleH = 18;
        const int freezeLabelW = 60;
        const int freezeTotalW = freezeToggleW + 4 + freezeLabelW;
        const int freezeY = knobMidY + knobH + sectionBottomPad;
        const int freezeX = rightX + (cellW - freezeTotalW) / 2;
        grainFreezeToggle.setBounds(freezeX, freezeY, freezeToggleW, freezeToggleH);
        freezeLabel.setBounds(freezeX + freezeToggleW + 4, freezeY, freezeLabelW, freezeToggleH);
    }

    // --- RIGHT BOTTOM: MULTIPLY (1/3 — 4 knobs, centered) ---
    {
        const int hdrY = rightMultiplyTop + topPad;
        const int knobY = hdrY + headerH + headerToKnob;
        const int gx = cellGridX(rightX, 4);
        layoutKnobInBounds(chorusMixKnob,   { gx,              knobY, knobW, knobH });
        layoutKnobInBounds(chorusSpeedKnob, { gx + knobW,      knobY, knobW, knobH });
        layoutKnobInBounds(panOuterKnob,    { gx + knobW * 2,  knobY, knobW, knobH });
        layoutKnobInBounds(panInnerKnob,    { gx + knobW * 3,  knobY, knobW, knobH });
        multiplyToggle.setBounds(rightX + cellPadX, hdrY + 1, toggleW, toggleH);
    }

    // Right padding: 3 knobs stacked vertically (Input, Output, Dry/Wet)
    {
        const int rightPadW = galaxyBounds.getRight() - screenBounds.getRight();
        const int rightCenterX = screenBounds.getRight() + rightPadW / 2;
        const int knobSize = 68;
        const int knobGap = 4;
        const int totalKnobH = knobSize * 3 + knobGap * 2;
        const int knobStartY = screenBounds.getY() + (screenBounds.getHeight() - totalKnobH) / 2;
        const int knobX = rightCenterX - knobSize / 2;

        layoutKnobInBounds(inputGainKnob,  { knobX, knobStartY, knobSize, knobSize });
        layoutKnobInBounds(outputGainKnob, { knobX, knobStartY + knobSize + knobGap, knobSize, knobSize });
        layoutKnobInBounds(masterMixKnob,  { knobX, knobStartY + (knobSize + knobGap) * 2, knobSize, knobSize });
    }
    // Left padding: signal flow display is drawn in paintOverChildren
}
