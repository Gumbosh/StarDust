#include "PluginEditor.h"
#include "BinaryData.h"
#include <set>

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

    // Tick marks — 3 generic positions for all knobs except Rate
    if (slider.getName() != "destroyFader")
    {
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

    // Sidebar mix strip — compact left-fill bar with small dot thumb
    if (slider.getComponentID() == "sidebarMix")
    {
        const float fx = static_cast<float>(x);
        const float fy = static_cast<float>(y);
        const float fw = static_cast<float>(width);
        const float fh = static_cast<float>(height);
        const float trackY2 = fy + fh * 0.5f;
        const float trackLeft2  = fx + 5.0f;
        const float trackRight2 = fx + fw - 5.0f;
        const float trackW2 = trackRight2 - trackLeft2;

        const bool active = slider.isMouseOver(true) || slider.isMouseButtonDown();

        // Background track
        g.setColour(kFgGhost.withAlpha(0.22f));
        g.fillRoundedRectangle(trackLeft2, trackY2 - 1.5f, trackW2, 3.0f, 1.5f);

        // Value fill — from left
        const float fillW = sliderPos - trackLeft2;
        if (fillW > 0.0f)
        {
            g.setColour(kAccent.withAlpha(active ? 0.75f : 0.55f));
            g.fillRoundedRectangle(trackLeft2, trackY2 - 1.5f, fillW, 3.0f, 1.5f);
        }

        // Thumb dot
        const float dotR = active ? 4.5f : 3.5f;
        g.setColour(active ? kAccent : kAccent.withAlpha(0.80f));
        g.fillEllipse(sliderPos - dotR, trackY2 - dotR, dotR * 2.0f, dotR * 2.0f);

        return;
    }

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

    // Noise-speed segmented radio buttons
    if (button.getComponentID() == "tapeNoiseSpeedBtn")
    {
        const float cornerR = 3.0f;
        const bool isOn = button.getToggleState();
        if (isOn)
        {
            g.setColour(kAccent.withAlpha(0.18f));
            g.fillRoundedRectangle(bounds.reduced(1.0f), cornerR);
            g.setColour(kAccent.withAlpha(0.55f));
            g.drawRoundedRectangle(bounds.reduced(1.0f), cornerR, 1.0f);
        }
        else
        {
            g.setColour(kFgGhost.withAlpha(shouldDrawButtonAsHighlighted ? 0.18f : 0.08f));
            g.fillRoundedRectangle(bounds.reduced(1.0f), cornerR);
            g.setColour(kFgGhost.withAlpha(0.30f));
            g.drawRoundedRectangle(bounds.reduced(1.0f), cornerR, 1.0f);
        }
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
    else if (button.getComponentID() == "tapeNoiseSpeedBtn")
    {
        const bool isOn = button.getToggleState();
        g.setColour(isOn ? kAccent : (shouldDrawButtonAsHighlighted ? kFg : kFgDim));
        g.setFont(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(), 10.0f, juce::Font::plain));
        g.drawText(button.getButtonText(), bounds, juce::Justification::centred);
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
    setSize(860, 720);

    addAndMakeVisible(starfield);
    starfield.setExcludeRect({});
    starfield.toBack();



    setupKnob(destroyBitsKnob, "destroyBits", "Bits");
    setupKnob(hazeColorKnob, "hazeColor", "Color");
    setupKnob(destroyInKnob, "destroyIn", "Drive");
    setupKnob(destroyOutKnob, "destroyOut", "Output");

    // Rate knob — rotary attached to destroyFader param, shows kHz value
    setupKnob(destroyRateKnob, "destroyFader", "Rate");
    destroyRateKnob.slider.textFromValueFunction = [](double v) {
        const float hz = static_cast<float>(v);
        if (hz >= 10000.f)
            return juce::String(hz / 1000.f, 1) + " kHz";
        if (hz >= 1000.f)
            return juce::String(juce::roundToInt(hz / 100.f) * 100) + " Hz";
        return juce::String(juce::roundToInt(hz)) + " Hz";
    };
    destroyRateKnob.slider.valueFromTextFunction = [](const juce::String& t) {
        float hz = t.getFloatValue();
        if (t.containsIgnoreCase("k"))
            hz *= 1000.0f;
        return static_cast<double>(juce::jlimit(250.0f, 96000.0f, hz));
    };
    destroyRateKnob.slider.updateText();

    // Haze type buttons: Pink / White / Vinyl
    {
        static const char* kHazeLabels[] = { "Pink", "White", "Vinyl" };
        auto* hazeTypeParam = dynamic_cast<juce::AudioParameterChoice*>(
            processorRef.apvts.getParameter("hazeType"));
        auto updateHazeButtons = [this, hazeTypeParam]() {
            int sel = hazeTypeParam ? hazeTypeParam->getIndex() : 0;
            for (int i = 0; i < 3; ++i)
                hazeTypeBtn[i].setToggleState(i == sel, juce::dontSendNotification);
        };
        for (int i = 0; i < 3; ++i)
        {
            hazeTypeBtn[i].setButtonText(kHazeLabels[i]);
            hazeTypeBtn[i].setClickingTogglesState(false);
            hazeTypeBtn[i].setColour(juce::TextButton::buttonColourId,   juce::Colours::transparentBlack);
            hazeTypeBtn[i].setColour(juce::TextButton::buttonOnColourId, StardustLookAndFeel::kAccent.withAlpha(0.15f));
            hazeTypeBtn[i].setColour(juce::TextButton::textColourOffId,  StardustLookAndFeel::kFgDim);
            hazeTypeBtn[i].setColour(juce::TextButton::textColourOnId,   StardustLookAndFeel::kAccent);
            hazeTypeBtn[i].setComponentID("tapeNoiseSpeedBtn");
            hazeTypeBtn[i].onClick = [this, i, updateHazeButtons, hazeTypeParam]() {
                if (hazeTypeParam)
                    hazeTypeParam->setValueNotifyingHost(hazeTypeParam->convertTo0to1(static_cast<float>(i)));
                updateHazeButtons();
            };
            addAndMakeVisible(hazeTypeBtn[i]);
        }
        updateHazeButtons();
        processorRef.apvts.addParameterListener("hazeType", this);
        hazeTypeLabel.setText("Type", juce::dontSendNotification);
        hazeTypeLabel.setJustificationType(juce::Justification::centredLeft);
        hazeTypeLabel.setFont(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(), 10.0f, juce::Font::plain));
        hazeTypeLabel.setColour(juce::Label::textColourId, StardustLookAndFeel::kFgDim);
        addAndMakeVisible(hazeTypeLabel);
    }

    setupKnob(grainMixKnob, "grainMix", "Mix");
    setupKnob(grainCloudKnob, "grainCloud", "Amount");
    setupKnob(grainScatterKnob, "grainDrift", "Spread");
    setupKnob(grainSpaceKnob, "grainSpace", "Space");
    setupKnob(grainReverbSizeKnob, "grainReverbSize", "R.Size");

    // Morph knob — continuous pitch control (±12st, bipolar arc)
    setupKnob(grainMorphKnob, "grainMorph", "Pitch");

    // Size knob (BPM-linked note divisions)
    setupKnob(grainSizeSyncKnob, "grainSizeSync", "Size");

    // REV: reverse probability knob (0–100%)
    setupKnob(grainRevKnob, "grainReverse", "Rev");

    setupKnob(grainFeedbackKnob, "grainFeedback", "Feedback");

    // Grain shape buttons: Hann / Gauss / Tri / Trap
    {
        static const char* kShapeLabels[] = { "Hann", "Gauss", "Tri", "Trap" };
        auto* shapeParam = dynamic_cast<juce::AudioParameterChoice*>(
            processorRef.apvts.getParameter("grainShape"));
        auto updateShapeButtons = [this, shapeParam]() {
            int sel = shapeParam ? shapeParam->getIndex() : 0;
            for (int i = 0; i < 4; ++i)
                grainShapeBtn[i].setToggleState(i == sel, juce::dontSendNotification);
        };
        for (int i = 0; i < 4; ++i)
        {
            grainShapeBtn[i].setButtonText(kShapeLabels[i]);
            grainShapeBtn[i].setClickingTogglesState(false);
            grainShapeBtn[i].setColour(juce::TextButton::buttonColourId,   juce::Colours::transparentBlack);
            grainShapeBtn[i].setColour(juce::TextButton::buttonOnColourId, StardustLookAndFeel::kAccent.withAlpha(0.15f));
            grainShapeBtn[i].setColour(juce::TextButton::textColourOffId,  StardustLookAndFeel::kFgDim);
            grainShapeBtn[i].setColour(juce::TextButton::textColourOnId,   StardustLookAndFeel::kAccent);
            grainShapeBtn[i].setComponentID("tapeNoiseSpeedBtn");
            grainShapeBtn[i].onClick = [this, i, updateShapeButtons, shapeParam]() {
                if (shapeParam)
                    shapeParam->setValueNotifyingHost(
                        shapeParam->convertTo0to1(static_cast<float>(i)));
                updateShapeButtons();
            };
            addAndMakeVisible(grainShapeBtn[i]);
        }
        updateShapeButtons();
        processorRef.apvts.addParameterListener("grainShape", this);

        grainShapeLabel.setText("Shape", juce::dontSendNotification);
        grainShapeLabel.setJustificationType(juce::Justification::centredLeft);
        grainShapeLabel.setFont(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(), 10.0f, juce::Font::plain));
        grainShapeLabel.setColour(juce::Label::textColourId, StardustLookAndFeel::kFgDim);
        addAndMakeVisible(grainShapeLabel);
    }

    // Pitch quantization buttons: Off / Chr / Maj / Min / Pent
    {
        static const char* kQuantLabels[] = { "Off", "Chr", "Maj", "Min", "Pnt" };
        auto* quantParam = dynamic_cast<juce::AudioParameterChoice*>(
            processorRef.apvts.getParameter("grainPitchQuant"));
        auto updateQuantButtons = [this, quantParam]() {
            int sel = quantParam ? quantParam->getIndex() : 0;
            for (int i = 0; i < 5; ++i)
                grainQuantBtn[i].setToggleState(i == sel, juce::dontSendNotification);
        };
        for (int i = 0; i < 5; ++i)
        {
            grainQuantBtn[i].setButtonText(kQuantLabels[i]);
            grainQuantBtn[i].setClickingTogglesState(false);
            grainQuantBtn[i].setColour(juce::TextButton::buttonColourId,   juce::Colours::transparentBlack);
            grainQuantBtn[i].setColour(juce::TextButton::buttonOnColourId, StardustLookAndFeel::kAccent.withAlpha(0.15f));
            grainQuantBtn[i].setColour(juce::TextButton::textColourOffId,  StardustLookAndFeel::kFgDim);
            grainQuantBtn[i].setColour(juce::TextButton::textColourOnId,   StardustLookAndFeel::kAccent);
            grainQuantBtn[i].setComponentID("tapeNoiseSpeedBtn");
            grainQuantBtn[i].onClick = [this, i, updateQuantButtons, quantParam]() {
                if (quantParam)
                    quantParam->setValueNotifyingHost(
                        quantParam->convertTo0to1(static_cast<float>(i)));
                updateQuantButtons();
            };
            addAndMakeVisible(grainQuantBtn[i]);
        }
        updateQuantButtons();
        processorRef.apvts.addParameterListener("grainPitchQuant", this);

        grainQuantLabel.setText("Quant", juce::dontSendNotification);
        grainQuantLabel.setJustificationType(juce::Justification::centredLeft);
        grainQuantLabel.setFont(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(), 10.0f, juce::Font::plain));
        grainQuantLabel.setColour(juce::Label::textColourId, StardustLookAndFeel::kFgDim);
        addAndMakeVisible(grainQuantLabel);
    }

    setupKnob(chorusMixKnob, "chorusMix", "Mix");
    setupKnob(chorusSpeedKnob, "chorusSpeed", "Speed");
    setupKnob(panOuterKnob, "multiplyPanOuter", "1+2");
    setupKnob(panInnerKnob, "multiplyPanInner", "3+4");
    setupKnob(multiplyDepthKnob, "multiplyDepth", "Depth");
    multiplyDepthKnob.slider.textFromValueFunction = [](double v) {
        const float ms = 0.01f + static_cast<float>(v) * 1.49f;
        return juce::String(ms, 2) + " ms";
    };
    multiplyDepthKnob.slider.valueFromTextFunction = [](const juce::String& t) {
        const float ms = t.getFloatValue();
        return static_cast<double>(juce::jlimit(0.0f, 1.0f, (ms - 0.01f) / 1.49f));
    };
    multiplyDepthKnob.slider.updateText();
    setupKnob(multiplyToneKnob, "multiplyTone", "Tone");
    setupKnob(multiplyFeedbackKnob, "multiplyFeedback", "Feedback");
    multiplyFeedbackKnob.slider.textFromValueFunction = [](double v) {
        return juce::String(static_cast<int>(std::round(v * 100.0))) + "%";
    };
    multiplyFeedbackKnob.slider.updateText();
    setupKnob(multiplyShimmerKnob, "multiplyShimmer", "Shimmer");

    {
        static const char* kLfoLabels[] = { "Sine", "Tri", "Rnd" };
        auto* lfoParam = processorRef.apvts.getParameter("multiplyLfoShape");
        const int initLfo = lfoParam
            ? juce::jlimit(0, 2, static_cast<int>(dynamic_cast<juce::AudioParameterChoice*>(lfoParam)->getIndex()))
            : 0;
        auto updateLfoButtons = [this]() {
            if (auto* pc = dynamic_cast<juce::AudioParameterChoice*>(processorRef.apvts.getParameter("multiplyLfoShape")))
                for (int i = 0; i < 3; ++i)
                    multiplyLfoBtn[i].setToggleState(pc->getIndex() == i, juce::dontSendNotification);
        };
        for (int i = 0; i < 3; ++i)
        {
            multiplyLfoBtn[i].setToggleState(i == initLfo, juce::dontSendNotification);
            multiplyLfoBtn[i].setButtonText(kLfoLabels[i]);
            multiplyLfoBtn[i].setClickingTogglesState(false);
            multiplyLfoBtn[i].setColour(juce::TextButton::buttonColourId,   juce::Colours::transparentBlack);
            multiplyLfoBtn[i].setColour(juce::TextButton::buttonOnColourId, StardustLookAndFeel::kAccent.withAlpha(0.15f));
            multiplyLfoBtn[i].setColour(juce::TextButton::textColourOffId,  StardustLookAndFeel::kFgDim);
            multiplyLfoBtn[i].setColour(juce::TextButton::textColourOnId,   StardustLookAndFeel::kAccent);
            multiplyLfoBtn[i].setComponentID("tapeNoiseSpeedBtn");
            multiplyLfoBtn[i].onClick = [this, i, updateLfoButtons, lfoParam]() {
                if (auto* pc = dynamic_cast<juce::AudioParameterChoice*>(lfoParam))
                    pc->setValueNotifyingHost(pc->convertTo0to1(static_cast<float>(i)));
                updateLfoButtons();
            };
            addAndMakeVisible(multiplyLfoBtn[i]);
        }
        processorRef.apvts.addParameterListener("multiplyLfoShape", this);
        multiplyLfoLabel.setText("LFO", juce::dontSendNotification);
        multiplyLfoLabel.setJustificationType(juce::Justification::centredLeft);
        multiplyLfoLabel.setFont(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(), 10.0f, juce::Font::plain));
        multiplyLfoLabel.setColour(juce::Label::textColourId, StardustLookAndFeel::kFgDim);
        addAndMakeVisible(multiplyLfoLabel);
    }

    // Vintage mode buttons: Custom / Juno-60 / Dim-D / Tri-Cho / Flanger
    {
        static const char* kVintageLabels[] = { "Custom", "Juno", "Dim-D", "Tri", "Flngr" };
        auto* vintageParam = dynamic_cast<juce::AudioParameterChoice*>(
            processorRef.apvts.getParameter("multiplyVintage"));
        auto updateVintageButtons = [this, vintageParam]() {
            int sel = vintageParam ? vintageParam->getIndex() : 0;
            for (int i = 0; i < 5; ++i)
                multiplyVintageBtn[i].setToggleState(i == sel, juce::dontSendNotification);
        };
        for (int i = 0; i < 5; ++i)
        {
            multiplyVintageBtn[i].setButtonText(kVintageLabels[i]);
            multiplyVintageBtn[i].setClickingTogglesState(false);
            multiplyVintageBtn[i].setColour(juce::TextButton::buttonColourId,   juce::Colours::transparentBlack);
            multiplyVintageBtn[i].setColour(juce::TextButton::buttonOnColourId, StardustLookAndFeel::kAccent.withAlpha(0.15f));
            multiplyVintageBtn[i].setColour(juce::TextButton::textColourOffId,  StardustLookAndFeel::kFgDim);
            multiplyVintageBtn[i].setColour(juce::TextButton::textColourOnId,   StardustLookAndFeel::kAccent);
            multiplyVintageBtn[i].setComponentID("tapeNoiseSpeedBtn");
            multiplyVintageBtn[i].onClick = [this, i, updateVintageButtons, vintageParam]() {
                if (vintageParam)
                    vintageParam->setValueNotifyingHost(
                        vintageParam->convertTo0to1(static_cast<float>(i)));
                updateVintageButtons();
            };
            addAndMakeVisible(multiplyVintageBtn[i]);
        }
        updateVintageButtons();
        processorRef.apvts.addParameterListener("multiplyVintage", this);
        multiplyVintageLabel.setText("Mode", juce::dontSendNotification);
        multiplyVintageLabel.setJustificationType(juce::Justification::centredLeft);
        multiplyVintageLabel.setFont(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(), 10.0f, juce::Font::plain));
        multiplyVintageLabel.setColour(juce::Label::textColourId, StardustLookAndFeel::kFgDim);
        addAndMakeVisible(multiplyVintageLabel);
    }

    multiplyTempoSyncAttach = std::make_unique<ButtonAttachment>(processorRef.apvts, "multiplyTempoSync", multiplyTempoSyncBtn);
    multiplyTempoSyncBtn.setButtonText("Sync");
    multiplyTempoSyncBtn.setColour(juce::ToggleButton::textColourId, StardustLookAndFeel::kFgDim);
    multiplyTempoSyncBtn.setColour(juce::ToggleButton::tickColourId, StardustLookAndFeel::kAccent);
    multiplyTempoSyncBtn.setColour(juce::ToggleButton::tickDisabledColourId, StardustLookAndFeel::kFgGhost);
    addAndMakeVisible(multiplyTempoSyncBtn);

    setupKnob(tapeDriveKnob, "tapeDrive", "Drive");
    setupKnob(tapeInputKnob, "tapeInput", "Input");
    setupKnob(tapeWowKnob, "tapeWow", "Wobble");
    setupKnob(tapeGlueKnob, "tapeGlue", "Glue");
    setupKnob(tapeNoiseKnob, "tapeNoise", "Noise");
    setupKnob(tapeMixKnob, "tapeMix", "Mix");
    setupKnob(tapeOutputKnob, "tapeOutput", "Output");
    setupKnob(multiplyDriftKnob, "multiplyDrift", "Drift");

    setupKnob(distortionDriveKnob, "distortionDrive", "Drive");
    setupKnob(distortionToneKnob, "distortionTone", "Tone");
    setupKnob(distortionBiasKnob, "distortionBias", "Bias");
    setupKnob(distortionAsymKnob, "distortionAsym", "Asym");
    setupKnob(reverbMixKnob,       "reverbMix",       "Mix");
    setupKnob(reverbSizeKnob,      "reverbSize",      "Size");
    setupKnob(reverbDecayKnob,     "reverbDecay",     "Decay");
    setupKnob(reverbDampKnob,      "reverbDamp",      "Damp");
    setupKnob(reverbPreDelayKnob,  "reverbPreDelay",  "Pre-Dly");
    setupKnob(reverbDiffusionKnob, "reverbDiffusion", "Diffuse");
    setupKnob(reverbWidthKnob,     "reverbWidth",     "Width");

    tapeNoiseSpeedLabel.setText("Noise IPS", juce::dontSendNotification);
    tapeNoiseSpeedLabel.setJustificationType(juce::Justification::centredLeft);
    tapeNoiseSpeedLabel.setFont(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(), 10.0f, juce::Font::plain));
    tapeNoiseSpeedLabel.setColour(juce::Label::textColourId, StardustLookAndFeel::kFgDim);
    addAndMakeVisible(tapeNoiseSpeedLabel);

    {
        static const char* kLabels[] = { "7.5", "15", "30" };
        auto* noiseSpeedParam = dynamic_cast<juce::AudioParameterChoice*>(
            processorRef.apvts.getParameter("tapeNoiseSpeed"));
        auto updateNoiseButtons = [this, noiseSpeedParam]() {
            int sel = noiseSpeedParam ? noiseSpeedParam->getIndex() : 1;
            for (int i = 0; i < 3; ++i)
                tapeNoiseSpeedBtn[i].setToggleState(i == sel, juce::dontSendNotification);
        };
        for (int i = 0; i < 3; ++i)
        {
            tapeNoiseSpeedBtn[i].setButtonText(kLabels[i]);
            tapeNoiseSpeedBtn[i].setClickingTogglesState(false);
            tapeNoiseSpeedBtn[i].setColour(juce::TextButton::buttonColourId,   juce::Colours::transparentBlack);
            tapeNoiseSpeedBtn[i].setColour(juce::TextButton::buttonOnColourId, StardustLookAndFeel::kAccent.withAlpha(0.15f));
            tapeNoiseSpeedBtn[i].setColour(juce::TextButton::textColourOffId,  StardustLookAndFeel::kFgDim);
            tapeNoiseSpeedBtn[i].setColour(juce::TextButton::textColourOnId,   StardustLookAndFeel::kAccent);
            tapeNoiseSpeedBtn[i].setComponentID("tapeNoiseSpeedBtn");
            tapeNoiseSpeedBtn[i].onClick = [this, i, updateNoiseButtons, noiseSpeedParam]() {
                if (noiseSpeedParam)
                    noiseSpeedParam->setValueNotifyingHost(
                        noiseSpeedParam->convertTo0to1(static_cast<float>(i)));
                updateNoiseButtons();
            };
            addAndMakeVisible(tapeNoiseSpeedBtn[i]);
        }
        updateNoiseButtons();
        processorRef.apvts.addParameterListener("tapeNoiseSpeed", this);
    }

    // Tape formulation buttons: 456 / GP9 / SM900 / LH / SM468

    // Tape EQ standard buttons: NAB / IEC

    // Distortion mode buttons: Soft / Tube / Hard / Trans / Satin / Vari-Mu
    {
        static const char* kModeLabels[] = { "Soft", "Tube", "Hard", "Trans", "Satin", "V-Mu" };
        auto* modeParam = dynamic_cast<juce::AudioParameterChoice*>(
            processorRef.apvts.getParameter("distortionMode"));
        auto updateModeButtons = [this, modeParam]() {
            int sel = modeParam ? modeParam->getIndex() : 0;
            for (int i = 0; i < 6; ++i)
                distortionModeBtn[i].setToggleState(i == sel, juce::dontSendNotification);
        };
        for (int i = 0; i < 6; ++i)
        {
            distortionModeBtn[i].setButtonText(kModeLabels[i]);
            distortionModeBtn[i].setClickingTogglesState(false);
            distortionModeBtn[i].setColour(juce::TextButton::buttonColourId,   juce::Colours::transparentBlack);
            distortionModeBtn[i].setColour(juce::TextButton::buttonOnColourId, StardustLookAndFeel::kAccent.withAlpha(0.15f));
            distortionModeBtn[i].setColour(juce::TextButton::textColourOffId,  StardustLookAndFeel::kFgDim);
            distortionModeBtn[i].setColour(juce::TextButton::textColourOnId,   StardustLookAndFeel::kAccent);
            distortionModeBtn[i].setComponentID("tapeNoiseSpeedBtn"); // reuses same look-and-feel style
            distortionModeBtn[i].onClick = [this, i, updateModeButtons, modeParam]() {
                if (modeParam)
                    modeParam->setValueNotifyingHost(
                        modeParam->convertTo0to1(static_cast<float>(i)));
                updateModeButtons();
            };
            addAndMakeVisible(distortionModeBtn[i]);
        }
        updateModeButtons();
        processorRef.apvts.addParameterListener("distortionMode", this);

        distortionModeLabel.setText("Mode", juce::dontSendNotification);
        distortionModeLabel.setJustificationType(juce::Justification::centredLeft);
        distortionModeLabel.setFont(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(), 10.0f, juce::Font::plain));
        distortionModeLabel.setColour(juce::Label::textColourId, StardustLookAndFeel::kFgDim);
        addAndMakeVisible(distortionModeLabel);
    }

    // Right side knobs: Input, Output, Dry/Wet
    setupKnob(inputGainKnob, "inputGain", "Input");
    setupKnob(outputGainKnob, "outputGain", "Output");
    setupKnob(masterMixKnob, "masterMix", "Dry/Wet");

    // Sidebar mix strip sliders (LinearHorizontal, positioned below name pill in each row)
    auto setupMixStrip = [&](juce::Slider& s, std::unique_ptr<SliderAttachment>& a, const juce::String& paramId)
    {
        s.setSliderStyle(juce::Slider::LinearHorizontal);
        s.setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);
        s.setComponentID("sidebarMix");
        s.textFromValueFunction = [](double v) {
            return juce::String(static_cast<int>(std::round(v * 100.0))) + "%";
        };
        s.setPopupDisplayEnabled(true, true, this, 400);
        a = std::make_unique<SliderAttachment>(processorRef.apvts, paramId, s);
        addAndMakeVisible(s);
    };
    setupMixStrip(destroyMixStrip,  destroyMixStripAttach,  "destroyMix");
    setupMixStrip(grainMixStrip,    grainMixStripAttach,    "grainMix");
    setupMixStrip(multiplyMixStrip, multiplyMixStripAttach, "chorusMix");
    setupMixStrip(tapeMixStrip,     tapeMixStripAttach,     "tapeMix");
    setupMixStrip(hazeMixStrip,     hazeMixStripAttach,     "hazeMix");

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
        resized();
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
        resized();
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
                    resized();
                }
                else if (result == 4)
                {
                    // Initialize — load factory Default
                    processorRef.loadPreset(0);
                    refreshPresetList();
                    presetSelector.setSelectedId(1, juce::dontSendNotification);
                    resized();
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
    setupToggle(distortionToggle, distortionToggleAttach, "distortionEnabled");
    setupToggle(reverbToggle, reverbToggleAttach, "reverbEnabled");
    setupToggle(hazeToggle, hazeToggleAttach, "hazeEnabled");

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
    dimSection(destroyToggle, { &destroyBitsKnob, &destroyRateKnob,
                                &destroyInKnob, &destroyOutKnob });
    dimSection(granularToggle, { &grainMixKnob, &grainCloudKnob, &grainScatterKnob,
                                  &grainSpaceKnob, &grainReverbSizeKnob, &grainMorphKnob, &grainFeedbackKnob });
    {
        auto origOnClick = granularToggle.onClick;
        auto oldOrigOnClick = origOnClick; // already captured by prior lambda
        origOnClick = [this, oldOrigOnClick]() {
            if (oldOrigOnClick) oldOrigOnClick();
            const bool on = granularToggle.getToggleState();
            const float alpha = on ? 1.0f : 0.4f;
            grainShapeLabel.setAlpha(alpha);
            for (auto& b : grainShapeBtn) b.setAlpha(alpha);
            grainQuantLabel.setAlpha(alpha);
            for (auto& b : grainQuantBtn) b.setAlpha(alpha);
        };
        granularToggle.onClick = origOnClick;
    }
    {
        auto origOnClick = granularToggle.onClick;
        granularToggle.onClick = [this, origOnClick]() {
            if (origOnClick) origOnClick();
            const bool on = granularToggle.getToggleState();
            const float alpha = on ? 1.0f : 0.4f;
        };
    }
    dimSection(multiplyToggle, { &chorusMixKnob, &chorusSpeedKnob, &multiplyDepthKnob, &multiplyToneKnob, &multiplyFeedbackKnob, &multiplyShimmerKnob, &panOuterKnob, &panInnerKnob, &multiplyDriftKnob });
    dimSection(tapeToggle, { &tapeInputKnob, &tapeDriveKnob, &tapeGlueKnob, &tapeNoiseKnob, &tapeMixKnob, &tapeWowKnob, &tapeOutputKnob });
    {
        auto origOnClick = multiplyToggle.onClick;
        multiplyToggle.onClick = [this, origOnClick]() {
            if (origOnClick) origOnClick();
            const bool on = multiplyToggle.getToggleState();
            const float alpha = on ? 1.0f : 0.4f;
            multiplyLfoLabel.setAlpha(alpha);
            for (auto& b : multiplyLfoBtn) b.setAlpha(alpha);
            multiplyVintageLabel.setAlpha(alpha);
            for (auto& b : multiplyVintageBtn) b.setAlpha(alpha);
            multiplyTempoSyncBtn.setAlpha(alpha);
        };
    }
    dimSection(distortionToggle, { &distortionDriveKnob, &distortionToneKnob, &distortionBiasKnob, &distortionAsymKnob });
    {
        auto origOnClick = distortionToggle.onClick;
        distortionToggle.onClick = [this, origOnClick]() {
            if (origOnClick) origOnClick();
            const bool on = distortionToggle.getToggleState();
            const float alpha = on ? 1.0f : 0.4f;
            distortionModeLabel.setAlpha(alpha);
            for (auto& b : distortionModeBtn) b.setAlpha(alpha);
        };
    }
    {
        auto& toggle = tapeToggle;
        auto* noiseLbl = &tapeNoiseSpeedLabel;
        auto origOnClick = toggle.onClick;
        toggle.onClick = [origOnClick, &toggle, noiseLbl, this]() {
            if (origOnClick) origOnClick();
            const bool on = toggle.getToggleState();
            const float alpha = on ? 1.0f : 0.4f;
            noiseLbl->setAlpha(alpha);
            for (auto& b : tapeNoiseSpeedBtn) { b.setEnabled(on); b.setAlpha(alpha); }
        };
    }

    // Apply initial dim state
    destroyToggle.onClick();
    granularToggle.onClick();
    multiplyToggle.onClick();
    tapeToggle.onClick();
    distortionToggle.onClick();

    dimSection(reverbToggle, { &reverbMixKnob, &reverbSizeKnob, &reverbDecayKnob, &reverbDampKnob, &reverbPreDelayKnob, &reverbDiffusionKnob, &reverbWidthKnob });
    reverbToggle.onClick();

    dimSection(hazeToggle, { &hazeColorKnob });
    {
        auto origOnClick = hazeToggle.onClick;
        hazeToggle.onClick = [this, origOnClick]() {
            if (origOnClick) origOnClick();
            const bool on = hazeToggle.getToggleState();
            const float alpha = on ? 1.0f : 0.4f;
            hazeTypeLabel.setAlpha(alpha);
            for (auto& b : hazeTypeBtn) { b.setEnabled(on); b.setAlpha(alpha); }
        };
    }
    hazeToggle.onClick();

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

    // Initialise chain slot state from APVTS
    syncChainSlots();
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

// ============================================================================
// LabeledKnob
// ============================================================================

StardustEditor::LabeledKnob::LabeledKnob()
{
    addAndMakeVisible(slider);
    addAndMakeVisible(label);
}

void StardustEditor::LabeledKnob::resized()
{
    auto b = getLocalBounds();
    label.setBounds(b.removeFromTop(14));
    slider.setBounds(b);

}

// ============================================================================

void StardustEditor::setupKnob(LabeledKnob& knob, const juce::String& paramId,
                               const juce::String& labelText)
{
    knob.slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    // isReadOnly=false → double-click on value opens inline text editor
    knob.slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 72, 18);
    knob.slider.setColour(juce::Slider::textBoxTextColourId,       StardustLookAndFeel::kFg);
    knob.slider.setColour(juce::Slider::textBoxOutlineColourId,    StardustLookAndFeel::kFgGhost.withAlpha(0.22f));
    knob.slider.setColour(juce::Slider::textBoxBackgroundColourId, StardustLookAndFeel::kFgGhost.withAlpha(0.06f));

    knob.slider.setName(paramId);
    knob.attachment = std::make_unique<SliderAttachment>(processorRef.apvts, paramId, knob.slider);

    if (auto* param = processorRef.apvts.getParameter(paramId))
    {
        const float defaultVal = param->convertFrom0to1(param->getDefaultValue());
        knob.slider.setDoubleClickReturnValue(true, defaultVal);
    }

    // Set value formatting AFTER attachment (attachment overwrites textFromValueFunction)
    if (paramId == "grainSize")
    {
        knob.slider.textFromValueFunction = [](double v) {
            return juce::String(static_cast<int>(std::round(v))) + " ms";
        };
        knob.slider.valueFromTextFunction = [](const juce::String& t) {
            return static_cast<double>(t.getIntValue());
        };
    }
    else if (paramId == "grainDensity" || paramId == "grainVoices")
    {
        knob.slider.textFromValueFunction = [](double v) {
            return juce::String(static_cast<int>(std::round(v)));
        };
        knob.slider.valueFromTextFunction = [](const juce::String& t) {
            return static_cast<double>(t.getIntValue());
        };
    }
    else if (paramId == "outputGain" || paramId == "inputGain"
             || paramId == "destroyIn" || paramId == "destroyOut"
             || paramId == "tapeOutput")
    {
        knob.slider.textFromValueFunction = [](double v) {
            if (v > 0.05) return juce::String("+") + juce::String(v, 1) + " dB";
            if (v < -0.05) return juce::String(v, 1) + " dB";
            return juce::String("0.0 dB");
        };
        knob.slider.valueFromTextFunction = [](const juce::String& t) {
            return t.getDoubleValue(); // "+3.5 dB" → 3.5 via getDoubleValue stops at space
        };
    }
    else if (paramId == "grainPitch")
    {
        knob.slider.textFromValueFunction = [](double v) {
            const int st = static_cast<int>(std::round(v));
            if (st > 0) return juce::String("+") + juce::String(st) + " st";
            if (st < 0) return juce::String(st) + " st";
            return juce::String("0 st");
        };
        knob.slider.valueFromTextFunction = [](const juce::String& t) {
            return static_cast<double>(t.getIntValue());
        };
    }
    else if (paramId == "chorusSpeed")
    {
        knob.slider.textFromValueFunction = [](double v) {
            return juce::String(v, 1) + " Hz";
        };
        knob.slider.valueFromTextFunction = [](const juce::String& t) {
            return t.getDoubleValue();
        };
    }
    else if (paramId == "filterCutoff")
    {
        knob.slider.textFromValueFunction = [](double v) {
            return juce::String(static_cast<int>(std::round(v * 99.0)));
        };
        knob.slider.valueFromTextFunction = [](const juce::String& t) {
            return t.getDoubleValue() / 99.0;
        };
    }
    else if (paramId == "grainInterval")
    {
        knob.slider.textFromValueFunction = [](double v) {
            const int st = static_cast<int>(std::round(v));
            return juce::String(st) + " st";
        };
        knob.slider.valueFromTextFunction = [](const juce::String& t) {
            return static_cast<double>(t.getIntValue());
        };
    }
    else if (paramId == "destroyMix" || paramId == "filterLfo"
             || paramId == "grainMix"
             || paramId == "grainScatter" || paramId == "stereoWidth"
             || paramId == "chorusMix"
             || paramId == "multiplyPanOuter" || paramId == "multiplyPanInner"
             || paramId == "masterMix"
             || paramId == "tapeDrive" || paramId == "tapeInput" || paramId == "tapeGlue"
             || paramId == "tapeNoise" || paramId == "tapeMix"
             || paramId == "grainPosition" || paramId == "grainFeedback"
             || paramId == "grainReverse"
             || paramId == "grainTexture")
    {
        knob.slider.textFromValueFunction = [](double v) {
            return juce::String(static_cast<int>(std::round(v * 100.0))) + "%";
        };
        knob.slider.valueFromTextFunction = [](const juce::String& t) {
            return t.getDoubleValue() / 100.0; // "50%" → 0.5
        };
    }
    else if (paramId == "grainMorph")
    {
        knob.slider.textFromValueFunction = [](double v) {
            const int st = static_cast<int>(std::round((v - 0.5) * 24.0));
            if (st > 0) return juce::String("+") + juce::String(st) + " st";
            if (st < 0) return juce::String(st) + " st";
            return juce::String("0 st");
        };
        knob.slider.valueFromTextFunction = [](const juce::String& t) {
            return t.getDoubleValue() / 24.0 + 0.5;
        };
    }
    else if (paramId == "grainSizeSync")
    {
        knob.slider.textFromValueFunction = [](double v) {
            static const char* kNames[] = {
                "Free", "1/1", "1/2", "1/4", "1/8", "1/16", "1/32",
                "1/4.", "1/4T", "1/8.", "1/8T"
            };
            const int idx = juce::jlimit(0, 10, static_cast<int>(std::round(v)));
            return juce::String(kNames[idx]);
        };
        knob.slider.valueFromTextFunction = [](const juce::String& t) {
            static const char* kNames[] = {
                "Free", "1/1", "1/2", "1/4", "1/8", "1/16", "1/32",
                "1/4.", "1/4T", "1/8.", "1/8T"
            };
            const juce::String trimmed = t.trim();
            for (int i = 0; i < 11; ++i)
                if (trimmed.equalsIgnoreCase(kNames[i]))
                    return static_cast<double>(i);
            return juce::jlimit(0.0, 10.0, t.getDoubleValue());
        };
    }

    else if (paramId == "destroyBits")
    {
        knob.slider.textFromValueFunction = [](double v) {
            if (v >= 10.0)  return juce::String(v, 1) + " bit";
            if (v >= 5.0)   return juce::String(v, 2) + " bit";
            return juce::String(v, 3) + " bit";
        };
        knob.slider.valueFromTextFunction = [](const juce::String& t) {
            return static_cast<double>(juce::jlimit(3.0f, 24.0f, t.getFloatValue()));
        };
    }
    else if (paramId == "grainCloud" || paramId == "grainDrift" || paramId == "grainSpace")
    {
        knob.slider.textFromValueFunction = [](double v) {
            return juce::String(static_cast<int>(std::round(v * 100.0))) + "%";
        };
        knob.slider.valueFromTextFunction = [](const juce::String& t) {
            return t.getDoubleValue() / 100.0;
        };
    }

    // Force text refresh with the new formatter
    knob.slider.updateText();
    paramToKnob[paramId] = &knob;

    // Set font + edit behaviour on the slider's internal text-box Label
    for (int i = 0; i < knob.slider.getNumChildComponents(); ++i)
    {
        if (auto* lbl = dynamic_cast<juce::Label*>(knob.slider.getChildComponent(i)))
        {
            lbl->setFont(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(), 13.0f, juce::Font::bold));
            // double-click opens editor; focus-loss accepts (don't discard)
            lbl->setEditable(false, true, false);
            break;
        }
    }

    knob.label.setText(labelText, juce::dontSendNotification);
    knob.label.setJustificationType(juce::Justification::centred);
    knob.label.setFont(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(), 11.0f, juce::Font::plain));
    knob.label.setColour(juce::Label::textColourId, StardustLookAndFeel::kFg);

    // Register with editor — LabeledKnob is now a Component that owns slider+label
    addAndMakeVisible(knob);
}

void StardustEditor::layoutKnobInBounds(LabeledKnob& knob, juce::Rectangle<int> bounds)
{
    knob.setBounds(bounds);
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
            resized();
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
            resized();
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
            resized();
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
    resized();
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

    // Re-sync chain order after preset load
    syncChainSlots();
}


// ============================================================================
// FX chain strip helpers
// ============================================================================

juce::Rectangle<int> StardustEditor::stripRowBounds(int row) const
{
    const int sH = controlsBounds.getHeight() / 4;
    return { controlsBounds.getX(), controlsBounds.getY() + row * sH,
             controlsBounds.getWidth(), sH };
}

juce::Rectangle<int> StardustEditor::sectionKnobBounds(int row) const
{
    // Center knob area: left sidebar (name pill) + right drag column
    static constexpr int kLeftW  = 118; // badge + name pill
    static constexpr int kDragW  =  30; // right drag handle column
    const int sH = controlsBounds.getHeight() / 4;
    return { controlsBounds.getX() + kLeftW,
             controlsBounds.getY() + row * sH,
             controlsBounds.getWidth() - kLeftW - kDragW,
             sH };
}

juce::ToggleButton& StardustEditor::toggleForSection(int fxId)
{
    switch (fxId)
    {
        case 1: return destroyToggle;
        case 2: return granularToggle;
        case 3: return multiplyToggle;
        case 4: return tapeToggle;
        case 5: return distortionToggle;
        case 6: return reverbToggle;
        case 7: return hazeToggle;
        default: return destroyToggle;
    }
}

int StardustEditor::firstNonEmptyRow() const
{
    for (int r = 0; r < 4; ++r)
        if (chainSlots[r] != 0) return r;
    return 0;
}

void StardustEditor::syncChainSlots()
{
    for (int i = 0; i < 4; ++i)
    {
        const juce::String id = "chainSlot" + juce::String(i);
        chainSlots[i] = static_cast<int>(*processorRef.apvts.getRawParameterValue(id));
    }

    // Deduplicate: clear any later duplicate of the same effect type so the
    // UI only shows each effect once (matching the single-instance DSP design)
    std::set<int> seen;
    for (int i = 0; i < 4; ++i)
    {
        if (chainSlots[i] == 0) continue;
        if (!seen.insert(chainSlots[i]).second)
            chainSlots[i] = 0; // duplicate → treat as empty
    }

    // Keep activeSection valid for cosmetic highlighting
    if (activeSection >= 4 || chainSlots[activeSection] == 0)
        activeSection = firstNonEmptyRow();
}

void StardustEditor::commitChainSlots()
{
    for (int i = 0; i < 4; ++i)
    {
        const juce::String id = "chainSlot" + juce::String(i);
        if (auto* p = dynamic_cast<juce::AudioParameterInt*>(processorRef.apvts.getParameter(id)))
            p->setValueNotifyingHost(p->convertTo0to1(static_cast<float>(chainSlots[i])));
    }
}

void StardustEditor::showAddEffectMenu(int row)
{
    // Which effect types are already present in other slots?
    std::set<int> usedFx;
    for (int i = 0; i < 4; ++i)
        if (i != row && chainSlots[i] != 0)
            usedFx.insert(chainSlots[i]);

    // Submenus with arrow — matches preset selector style
    // Items are disabled if that effect type is already in the chain
    juce::PopupMenu degradeMenu, colorMenu, spaceMenu;
    degradeMenu.addItem(1, "CRUSH",     !usedFx.count(1));
    degradeMenu.addItem(7, "HAZE",      !usedFx.count(7));
    colorMenu.addItem(4,  "OXIDE-456",  !usedFx.count(4));
    colorMenu.addItem(5,  "SATURATE",   !usedFx.count(5));
    spaceMenu.addItem(2,  "GRANULAR",   !usedFx.count(2));
    spaceMenu.addItem(3,  "CHORUS",     !usedFx.count(3));
    spaceMenu.addItem(6,  "REVERB",     !usedFx.count(6));

    juce::PopupMenu menu;
    menu.addSubMenu("Degrade", degradeMenu);
    menu.addSubMenu("Color",   colorMenu);
    menu.addSubMenu("Space",   spaceMenu);

    menu.setLookAndFeel(&lookAndFeel);
    degradeMenu.setLookAndFeel(&lookAndFeel);
    colorMenu.setLookAndFeel(&lookAndFeel);
    spaceMenu.setLookAndFeel(&lookAndFeel);
    // Anchor to centre of full 92px section
    const int sH = controlsBounds.getHeight() / 4;
    const auto rb = stripRowBounds(row);
    const int cx = getScreenX() + rb.getCentreX();
    const int cy = getScreenY() + rb.getY() + sH / 2;
    const auto targetScreen = juce::Rectangle<int>(cx - 1, cy, 2, 1);
    menu.showMenuAsync(juce::PopupMenu::Options()
                           .withTargetComponent(this)
                           .withTargetScreenArea(targetScreen)
                           .withPreferredPopupDirection(juce::PopupMenu::Options::PopupDirection::downwards),
        [this, row](int result) {
            if (result > 0)
            {
                chainSlots[row] = result;
                activeSection = row;
                commitChainSlots();
                // Ensure the toggle is ON when an effect is added to the chain
                toggleForSection(result).setToggleState(true, juce::sendNotification);
                resized();
                repaint();
            }
        });
}

// ============================================================================
// Layout helpers — one per section, called from resized()
// ============================================================================

void StardustEditor::layoutCrushSection(juce::Rectangle<int> ap)
{
    static constexpr int kKnobH = 68;
    const int padX = 8;
    const int availW = ap.getWidth() - padX * 2;
    const int ox = ap.getX() + padX;
    const int ky = ap.getY() + (ap.getHeight() - kKnobH) / 2;
    const int kw = availW / 4;

    // Bits | Rate | Drive | Output
    layoutKnobInBounds(destroyBitsKnob, { ox + kw * 0, ky, kw, kKnobH });
    layoutKnobInBounds(destroyRateKnob, { ox + kw * 1, ky, kw, kKnobH });
    layoutKnobInBounds(destroyInKnob,   { ox + kw * 2, ky, kw, kKnobH });
    layoutKnobInBounds(destroyOutKnob,  { ox + kw * 3, ky, kw, kKnobH });
}

void StardustEditor::layoutHazeSection(juce::Rectangle<int> ap)
{
    static constexpr int kKnobH = 68;
    const int padX = 8;
    const int availW = ap.getWidth() - padX * 2;
    const int ox = ap.getX() + padX;
    const int ky = ap.getY() + (ap.getHeight() - kKnobH) / 2;
    const int kw = availW / 5;

    // Color | [Type buttons]
    layoutKnobInBounds(hazeColorKnob, { ox + kw * 0, ky, kw, kKnobH });

    hazeTypeLabel.setVisible(true);
    hazeTypeLabel.setBounds(ox + kw * 1, ky, kw * 2, 12);
    const int bh = (kKnobH - 12) / 3;
    for (int i = 0; i < 3; ++i)
    {
        hazeTypeBtn[i].setVisible(true);
        hazeTypeBtn[i].setBounds(ox + kw * 1, ky + 12 + i * bh, kw * 2, bh);
    }
}

void StardustEditor::layoutGranularSection(juce::Rectangle<int> ap)
{
    static constexpr int kKnobH = 68;
    const int padX = 8;
    const int availW = ap.getWidth() - padX * 2;
    const int ox = ap.getX() + padX;
    const int ky = ap.getY() + (ap.getHeight() - kKnobH) / 2;
    const int kw = availW / 8;

    // Cloud | Scatter | Space | R.Size | Feedback | Morph | [Shape] | [Quant]
    layoutKnobInBounds(grainCloudKnob,      { ox + kw * 0, ky, kw, kKnobH });
    layoutKnobInBounds(grainScatterKnob,    { ox + kw * 1, ky, kw, kKnobH });
    layoutKnobInBounds(grainSpaceKnob,      { ox + kw * 2, ky, kw, kKnobH });
    layoutKnobInBounds(grainReverbSizeKnob, { ox + kw * 3, ky, kw, kKnobH });
    layoutKnobInBounds(grainFeedbackKnob,   { ox + kw * 4, ky, kw, kKnobH });
    layoutKnobInBounds(grainMorphKnob,      { ox + kw * 5, ky, kw, kKnobH });

    // Shape buttons stacked in col 6 (label + 4 buttons)
    grainShapeLabel.setVisible(true);
    grainShapeLabel.setBounds(ox + kw * 6, ky, kw, 12);
    const int bh4 = (kKnobH - 12) / 4;
    for (int i = 0; i < 4; ++i)
    {
        grainShapeBtn[i].setVisible(true);
        grainShapeBtn[i].setBounds(ox + kw * 6, ky + 12 + i * bh4, kw, bh4);
    }

    // Quant buttons stacked in col 7 (label + 5 buttons)
    grainQuantLabel.setVisible(true);
    grainQuantLabel.setBounds(ox + kw * 7, ky, kw, 12);
    const int bh5 = (kKnobH - 12) / 5;
    for (int i = 0; i < 5; ++i)
    {
        grainQuantBtn[i].setVisible(true);
        grainQuantBtn[i].setBounds(ox + kw * 7, ky + 12 + i * bh5, kw, bh5);
    }

    // Hidden knobs (APVTS attached, accessible via mod matrix / presets)
    grainSizeSyncKnob.setBounds(0, 0, 0, 0);
    grainRevKnob.setBounds(0, 0, 0, 0);
}

void StardustEditor::layoutMultiplySection(juce::Rectangle<int> ap)
{
    static constexpr int kKnobH = 68;
    const int padX = 8;
    const int availW = ap.getWidth() - padX * 2;
    const int ox = ap.getX() + padX;
    const int ky = ap.getY() + (ap.getHeight() - kKnobH) / 2;
    const int kw = availW / 9;
    // Speed | Depth | Tone | Drift | Shimmer | PanOut | PanIn | [Feedback+LFO] | [Vintage]
    const int startX = ox;

    layoutKnobInBounds(chorusSpeedKnob,      { startX,          ky, kw, kKnobH });
    layoutKnobInBounds(multiplyDepthKnob,    { startX + kw,     ky, kw, kKnobH });
    layoutKnobInBounds(multiplyToneKnob,     { startX + kw * 2, ky, kw, kKnobH });
    layoutKnobInBounds(multiplyDriftKnob,    { startX + kw * 3, ky, kw, kKnobH });
    layoutKnobInBounds(multiplyShimmerKnob,  { startX + kw * 4, ky, kw, kKnobH });
    layoutKnobInBounds(panOuterKnob,         { startX + kw * 5, ky, kw, kKnobH });
    layoutKnobInBounds(panInnerKnob,         { startX + kw * 6, ky, kw, kKnobH });

    // Col 7: Feedback knob (upper 40px) + LFO buttons (lower 28px)
    const int col7X = startX + kw * 7;
    const int fbH = 40;
    layoutKnobInBounds(multiplyFeedbackKnob, { col7X, ky, kw, fbH });
    const int lfoY = ky + fbH;
    const int lfoH = kKnobH - fbH;
    multiplyLfoLabel.setVisible(true);
    multiplyLfoLabel.setBounds(col7X, lfoY, kw, 10);
    const int bh = (lfoH - 10) / 3;
    for (int i = 0; i < 3; ++i)
    {
        multiplyLfoBtn[i].setVisible(true);
        multiplyLfoBtn[i].setBounds(col7X, lfoY + 10 + i * bh, kw, bh);
    }

    // Col 8: Vintage mode buttons (5 options: Custom/Juno/Dim-D/Tri/Flanger)
    const int col8X = startX + kw * 8;
    multiplyVintageLabel.setVisible(true);
    multiplyVintageLabel.setBounds(col8X, ky, availW - kw * 8, 12);
    const int vbh = (kKnobH - 12) / 5;
    for (int i = 0; i < 5; ++i)
    {
        multiplyVintageBtn[i].setVisible(true);
        multiplyVintageBtn[i].setBounds(col8X, ky + 12 + i * vbh, availW - kw * 8, vbh);
    }

    // Tempo sync toggle below Speed knob label area (small)
    multiplyTempoSyncBtn.setVisible(true);
    multiplyTempoSyncBtn.setBounds(startX, ky + kKnobH - 14, kw, 14);
}

void StardustEditor::layoutTapeSection(juce::Rectangle<int> ap)
{
    static constexpr int kKnobH = 68;
    const int padX = 8;
    const int availW = ap.getWidth() - padX * 2;
    const int ox = ap.getX() + padX;
    const int ky = ap.getY() + (ap.getHeight() - kKnobH) / 2;
    const int kw = availW / 7; // Input | Drive | Wobble | Glue | Noise | Output | IPS

    layoutKnobInBounds(tapeInputKnob,  { ox + kw * 0, ky, kw, kKnobH });
    layoutKnobInBounds(tapeDriveKnob,  { ox + kw * 1, ky, kw, kKnobH });
    layoutKnobInBounds(tapeWowKnob,    { ox + kw * 2, ky, kw, kKnobH });
    layoutKnobInBounds(tapeGlueKnob,   { ox + kw * 3, ky, kw, kKnobH });
    layoutKnobInBounds(tapeNoiseKnob,  { ox + kw * 4, ky, kw, kKnobH });
    layoutKnobInBounds(tapeOutputKnob, { ox + kw * 5, ky, kw, kKnobH });

    // IPS speed buttons in col 6
    const int col6W = availW - kw * 6;
    tapeNoiseSpeedLabel.setVisible(true);
    tapeNoiseSpeedLabel.setBounds(ox + kw * 6, ky, col6W, 12);
    const int bh = (kKnobH - 12) / 3;
    for (int i = 0; i < 3; ++i)
    {
        tapeNoiseSpeedBtn[i].setVisible(true);
        tapeNoiseSpeedBtn[i].setBounds(ox + kw * 6, ky + 12 + i * bh, col6W, bh);
    }
}

void StardustEditor::layoutDistortionSection(juce::Rectangle<int> ap)
{
    static constexpr int kKnobH = 68;
    const int padX = 8;
    const int availW = ap.getWidth() - padX * 2;
    const int ox = ap.getX() + padX;
    const int ky = ap.getY() + (ap.getHeight() - kKnobH) / 2;
    const int kw = availW / 6;
    // Drive | Tone | Bias | Asym | [Mode buttons] — 5 of 6 slots
    const int startX = ox + (availW - kw * 5) / 2;

    layoutKnobInBounds(distortionDriveKnob, { startX,          ky, kw, kKnobH });
    layoutKnobInBounds(distortionToneKnob,  { startX + kw,     ky, kw, kKnobH });
    layoutKnobInBounds(distortionBiasKnob,  { startX + kw * 2, ky, kw, kKnobH });
    layoutKnobInBounds(distortionAsymKnob,  { startX + kw * 3, ky, kw, kKnobH });

    // Mode buttons stacked in last column (Soft/Tube/Hard/Trans/Satin/V-Mu — 6 equal buttons)
    distortionModeLabel.setVisible(true);
    distortionModeLabel.setBounds(startX + kw * 4, ky, kw, 12);
    const int bh = (kKnobH - 12) / 6;
    for (int i = 0; i < 6; ++i)
    {
        distortionModeBtn[i].setVisible(true);
        distortionModeBtn[i].setBounds(startX + kw * 4, ky + 12 + i * bh, kw, bh);
    }
}

void StardustEditor::layoutReverbSection(juce::Rectangle<int> ap)
{
    static constexpr int kKnobH = 68;
    const int padX = 8;
    const int availW = ap.getWidth() - padX * 2;
    const int ox = ap.getX() + padX;
    const int ky = ap.getY() + (ap.getHeight() - kKnobH) / 2;
    const int kw = availW / 7;
    // Mix | Size | Decay | Damp | Pre-Dly | Diffuse | Width
    const int startX = ox + (availW - kw * 7) / 2;

    layoutKnobInBounds(reverbMixKnob,       { startX,          ky, kw, kKnobH });
    layoutKnobInBounds(reverbSizeKnob,      { startX + kw,     ky, kw, kKnobH });
    layoutKnobInBounds(reverbDecayKnob,     { startX + kw * 2, ky, kw, kKnobH });
    layoutKnobInBounds(reverbDampKnob,      { startX + kw * 3, ky, kw, kKnobH });
    layoutKnobInBounds(reverbPreDelayKnob,  { startX + kw * 4, ky, kw, kKnobH });
    layoutKnobInBounds(reverbDiffusionKnob, { startX + kw * 5, ky, kw, kKnobH });
    layoutKnobInBounds(reverbWidthKnob,     { startX + kw * 6, ky, kw, kKnobH });
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

    // ---- Chain strip rows ----
    static const juce::String kFxNames[8] = { "", "C R U S H", "G R A N U L A R", "C H O R U S", "O X I D E - 4 5 6", "S A T U R A T E", "R E V E R B", "H A Z E" };
    const auto monoFont10 = juce::FontOptions(juce::Font::getDefaultMonospacedFontName(), 10.0f, juce::Font::bold);
    const auto monoFont12 = juce::FontOptions(juce::Font::getDefaultMonospacedFontName(), 12.0f, juce::Font::bold);

    static constexpr int kLeftW  = 118; // left sidebar: badge + name pill
    static constexpr int kDragW  =  30; // right drag column
    static constexpr int kPillX  =  18; // pill x offset from row left (after badge)
    static constexpr int kPillW  =  92; // name pill width
    static constexpr int kPillH  =  28; // name pill height

    // Compact effect names for the pill
    static const juce::String kPillNames[8] = { "", "CRUSH", "GRANULAR", "CHORUS", "OXIDE-456", "SATURATE", "REVERB", "HAZE" };

    for (int row = 0; row < 4; ++row)
    {
        const auto rb = stripRowBounds(row); // full 92px row
        const int fx = chainSlots[row];
        const bool isActive = (row == activeSection && fx != 0);
        const bool isEmpty  = (fx == 0);
        const bool isHovered = (row == hoverRow);

        // Section background
        g.setColour(isActive ? StardustLookAndFeel::kAccent.withAlpha(0.07f)
                             : StardustLookAndFeel::kFgGhost.withAlpha(0.03f));
        g.fillRect(rb);

        if (isEmpty)
        {
            if (isHovered)
            {
                g.setColour(StardustLookAndFeel::kAccent.withAlpha(0.04f));
                g.fillRect(rb);
            }
            g.setFont(monoFont12);
            g.setColour(isHovered ? StardustLookAndFeel::kAccent
                                  : StardustLookAndFeel::kFg.withAlpha(0.75f));
            g.drawText("+ ADD EFFECT", rb.reduced(40, 0), juce::Justification::centred);
        }
        else
        {
            // ---- Left sidebar: name pill ----
            const bool effectOn = toggleForSection(fx).getToggleState();

            // Name pill — bordered rectangle, lights up when enabled
            const int pillY = rb.getCentreY() - kPillH / 2;
            const auto pillRect = juce::Rectangle<float>(
                static_cast<float>(rb.getX() + kPillX), static_cast<float>(pillY),
                static_cast<float>(kPillW),              static_cast<float>(kPillH));

            g.setColour(effectOn ? StardustLookAndFeel::kAccent.withAlpha(0.10f)
                                 : StardustLookAndFeel::kFgGhost.withAlpha(0.06f));
            g.fillRoundedRectangle(pillRect, 3.0f);

            g.setColour(effectOn ? StardustLookAndFeel::kAccent.withAlpha(0.75f)
                                 : StardustLookAndFeel::kFgGhost.withAlpha(0.45f));
            g.drawRoundedRectangle(pillRect, 3.0f, 1.0f);

            g.setFont(monoFont10);
            g.setColour(effectOn ? StardustLookAndFeel::kAccent : StardustLookAndFeel::kFgDim);
            g.drawText(kPillNames[fx], static_cast<int>(pillRect.getX()),
                       static_cast<int>(pillRect.getY()),
                       static_cast<int>(pillRect.getWidth()),
                       static_cast<int>(pillRect.getHeight()),
                       juce::Justification::centred);

            // Left divider between sidebar and knobs
            g.setColour(StardustLookAndFeel::kFgGhost.withAlpha(0.18f));
            g.drawVerticalLine(rb.getX() + kLeftW, static_cast<float>(rb.getY() + 8),
                               static_cast<float>(rb.getBottom() - 8));

            // ---- Right drag column ----
            // Subtle background tint
            g.setColour(StardustLookAndFeel::kFgGhost.withAlpha(0.05f));
            g.fillRect(rb.getRight() - kDragW, rb.getY(), kDragW, rb.getHeight());

            // Right divider
            g.setColour(StardustLookAndFeel::kFgGhost.withAlpha(0.18f));
            g.drawVerticalLine(rb.getRight() - kDragW, static_cast<float>(rb.getY() + 8),
                               static_cast<float>(rb.getBottom() - 8));

            // ≡ drag handle centered in right column
            g.setColour(StardustLookAndFeel::kFg.withAlpha(0.65f));
            const int dragColCx = rb.getRight() - kDragW / 2;
            for (int i = 0; i < 3; ++i)
                g.fillRect(dragColCx - 5, rb.getCentreY() - 4 + i * 4, 10, 1);
        }

        // Section separator at bottom of row (skip last row)
        if (row < 3)
        {
            g.setColour(StardustLookAndFeel::kFgGhost.withAlpha(0.18f));
            g.drawHorizontalLine(rb.getBottom() - 1, static_cast<float>(rb.getX() + 4),
                                 static_cast<float>(rb.getRight() - 4));
        }
    }

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

    // Drag ghost for FX chain reorder
    if (dragSourceRow >= 0)
    {
        static const juce::String kGhostNames[8] = { "", "CRUSH", "GRANULAR", "CHORUS", "OXIDE-456", "SATURATE", "REVERB", "HAZE" };
        static constexpr int kGhostLeftW = 118;
        static constexpr int kGhostDragW =  30;
        static constexpr int kGhostPillX =  18;
        static constexpr int kGhostPillW =  92;
        static constexpr int kGhostPillH =  28;

        const int fx = chainSlots[dragSourceRow];
        auto rb = stripRowBounds(dragSourceRow).withY(dragPos.y - stripRowBounds(0).getHeight() / 2);

        // Ghost background
        g.setColour(StardustLookAndFeel::kBg.withAlpha(0.88f));
        g.fillRect(rb);

        // Name pill
        const int pillY = rb.getCentreY() - kGhostPillH / 2;
        const auto pillRect = juce::Rectangle<float>(
            static_cast<float>(rb.getX() + kGhostPillX), static_cast<float>(pillY),
            static_cast<float>(kGhostPillW),              static_cast<float>(kGhostPillH));

        g.setColour(StardustLookAndFeel::kAccent.withAlpha(0.15f));
        g.fillRoundedRectangle(pillRect, 3.0f);
        g.setColour(StardustLookAndFeel::kAccent.withAlpha(0.85f));
        g.drawRoundedRectangle(pillRect, 3.0f, 1.0f);

        g.setFont(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(), 10.0f, juce::Font::bold));
        g.setColour(StardustLookAndFeel::kAccent);
        g.drawText(kGhostNames[fx],
                   static_cast<int>(pillRect.getX()), static_cast<int>(pillRect.getY()),
                   static_cast<int>(pillRect.getWidth()), static_cast<int>(pillRect.getHeight()),
                   juce::Justification::centred);

        // Left divider
        g.setColour(StardustLookAndFeel::kFgGhost.withAlpha(0.25f));
        g.drawVerticalLine(rb.getX() + kGhostLeftW, static_cast<float>(rb.getY() + 8),
                           static_cast<float>(rb.getBottom() - 8));

        // Right drag column
        g.setColour(StardustLookAndFeel::kFgGhost.withAlpha(0.08f));
        g.fillRect(rb.getRight() - kGhostDragW, rb.getY(), kGhostDragW, rb.getHeight());
        g.setColour(StardustLookAndFeel::kFgGhost.withAlpha(0.25f));
        g.drawVerticalLine(rb.getRight() - kGhostDragW, static_cast<float>(rb.getY() + 8),
                           static_cast<float>(rb.getBottom() - 8));
        g.setColour(StardustLookAndFeel::kAccent.withAlpha(0.8f));
        const int dragCx = rb.getRight() - kGhostDragW / 2;
        for (int i = 0; i < 3; ++i)
            g.fillRect(dragCx - 5, rb.getCentreY() - 4 + i * 4, 10, 1);

        // Outer border
        g.setColour(StardustLookAndFeel::kAccent.withAlpha(0.35f));
        g.drawRect(rb.toFloat(), 1.0f);

        // Drop target line
        if (dragHoverRow >= 0)
        {
            const int lineY = stripRowBounds(dragHoverRow).getY();
            g.setColour(StardustLookAndFeel::kAccent.withAlpha(0.7f));
            g.drawHorizontalLine(lineY, static_cast<float>(controlsBounds.getX() + 4),
                                 static_cast<float>(controlsBounds.getRight() - 4));
        }
    }
}

void StardustEditor::parameterChanged(const juce::String& parameterID, float newValue)
{
    if (parameterID == "tapeNoiseSpeed")
    {
        const int sel = juce::jlimit(0, 2, static_cast<int>(std::round(newValue)));
        juce::MessageManager::callAsync([this, sel]() {
            for (int i = 0; i < 3; ++i)
                tapeNoiseSpeedBtn[i].setToggleState(i == sel, juce::dontSendNotification);
        });
    }
    else if (parameterID == "distortionMode")
    {
        const int sel = juce::jlimit(0, 5, static_cast<int>(std::round(newValue)));
        juce::MessageManager::callAsync([this, sel]() {
            for (int i = 0; i < 6; ++i)
                distortionModeBtn[i].setToggleState(i == sel, juce::dontSendNotification);
            // Asym is only meaningful in Tube mode (1)
            distortionAsymKnob.setAlpha(sel == 1 ? 1.0f : 0.35f);
        });
    }
    else if (parameterID == "grainShape")
    {
        const int sel = juce::jlimit(0, 3, static_cast<int>(std::round(newValue)));
        juce::MessageManager::callAsync([this, sel]() {
            for (int i = 0; i < 4; ++i)
                grainShapeBtn[i].setToggleState(i == sel, juce::dontSendNotification);
        });
    }
    else if (parameterID == "grainPitchQuant")
    {
        const int sel = juce::jlimit(0, 4, static_cast<int>(std::round(newValue)));
        juce::MessageManager::callAsync([this, sel]() {
            for (int i = 0; i < 5; ++i)
                grainQuantBtn[i].setToggleState(i == sel, juce::dontSendNotification);
        });
    }
    else if (parameterID == "multiplyLfoShape")
    {
        const int sel = juce::jlimit(0, 2, static_cast<int>(std::round(newValue)));
        juce::MessageManager::callAsync([this, sel]() {
            for (int i = 0; i < 3; ++i)
                multiplyLfoBtn[i].setToggleState(i == sel, juce::dontSendNotification);
        });
    }
    else if (parameterID == "hazeType")
    {
        const int sel = juce::jlimit(0, 2, static_cast<int>(std::round(newValue)));
        juce::MessageManager::callAsync([this, sel]() {
            for (int i = 0; i < 3; ++i)
                hazeTypeBtn[i].setToggleState(i == sel, juce::dontSendNotification);
        });
    }
    // Dirty state is now computed by comparing current values to loaded preset in timerCallback
}

void StardustEditor::mouseMove(const juce::MouseEvent& e)
{
    presetSelector.repaint();

    int newHoverRow = -1;
    bool newHoverOnX = false;
    const auto pos = e.getPosition();
    for (int row = 0; row < 4; ++row)
    {
        auto rb = stripRowBounds(row);
        if (rb.contains(pos))
        {
            newHoverRow = row;
            newHoverOnX = false; // no X button
            break;
        }
    }
    if (newHoverRow != hoverRow || newHoverOnX != hoverOnX)
    {
        hoverRow  = newHoverRow;
        hoverOnX  = newHoverOnX;
        repaint();
    }
}

void StardustEditor::mouseExit(const juce::MouseEvent& /*e*/)
{
    if (hoverRow != -1)
    {
        hoverRow  = -1;
        hoverOnX  = false;
        repaint();
    }
}

void StardustEditor::timerCallback()
{
    // Update strip hover state using current mouse position (works across child components)
    {
        const auto mp = getMouseXYRelative();
        int newHoverRow = -1;
        bool newHoverOnX = false;
        if (isMouseOver(true))
        {
            for (int row = 0; row < 4; ++row)
            {
                auto rb = stripRowBounds(row);
                if (rb.contains(mp))
                {
                    newHoverRow = row;
                    newHoverOnX = false; // no X button
                    break;
                }
            }
        }
        hoverRow = newHoverRow;
        hoverOnX = newHoverOnX;
    }

    presetSelector.repaint();
    repaint(); // refresh signal flow display

    // Feed active grain positions into the starfield for visualization
    {
        std::array<GranularEngine::GrainDisplayInfo, GranularEngine::kMaxGrains> grainInfo;
        processorRef.getGranularEngine().getGrainDisplayInfo(grainInfo);
        std::array<StarfieldBackground::GrainParticle, StarfieldBackground::kMaxGrainParticles> particles;
        for (int i = 0; i < GranularEngine::kMaxGrains; ++i)
            particles[i] = { grainInfo[i].normPos, grainInfo[i].phase, grainInfo[i].active };
        starfield.setGrainData(particles, false);
    }

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


    // Chain strip row hit-testing
    const auto pos = e.getPosition();
    static constexpr int kDragW  =  30; // right drag column
    static constexpr int kPillX  =  18;
    static constexpr int kPillW  =  92;
    static constexpr int kPillH  =  28;
    for (int row = 0; row < 4; ++row)
    {
        auto rb = stripRowBounds(row); // full 92px row
        if (!rb.contains(pos)) continue;

        const int fx = chainSlots[row];
        if (fx == 0)
        {
            showAddEffectMenu(row);
            return;
        }

        // Right-click anywhere on row → context menu
        if (e.mods.isPopupMenu())
        {
            juce::PopupMenu menu;
            menu.setLookAndFeel(&lookAndFeel);
            menu.addItem(1, "Remove Effect");
            const auto target = juce::Rectangle<int>(pos.x + getScreenX(), pos.y + getScreenY(), 1, 1);
            menu.showMenuAsync(juce::PopupMenu::Options()
                                   .withTargetComponent(this)
                                   .withTargetScreenArea(target),
                [this, row](int result) {
                    if (result == 1)
                    {
                        chainSlots[row] = 0;
                        commitChainSlots();
                        resized();
                        repaint();
                    }
                });
            return;
        }

        // Right drag column → start drag
        if (pos.x >= rb.getRight() - kDragW)
        {
            dragSourceRow = row;
            dragPos = pos;
            return;
        }

        // Name pill zone → toggle effect on/off
        const int pillY = rb.getCentreY() - kPillH / 2;
        const juce::Rectangle<int> pillZone(rb.getX() + kPillX, pillY, kPillW, kPillH);
        if (pillZone.contains(pos))
        {
            auto& tog = toggleForSection(fx);
            tog.setToggleState(!tog.getToggleState(), juce::sendNotification);
            repaint();
            return;
        }

        // Empty space in the row → show remove menu (same as right-click)
        {
            juce::PopupMenu menu;
            menu.setLookAndFeel(&lookAndFeel);
            menu.addItem(1, "Remove Effect");
            const auto target = juce::Rectangle<int>(pos.x + getScreenX(), pos.y + getScreenY(), 1, 1);
            menu.showMenuAsync(juce::PopupMenu::Options()
                                   .withTargetComponent(this)
                                   .withTargetScreenArea(target),
                [this, row](int result) {
                    if (result == 1)
                    {
                        chainSlots[row] = 0;
                        commitChainSlots();
                        resized();
                        repaint();
                    }
                });
        }
        return;
    }

    AudioProcessorEditor::mouseDown(e);
}

void StardustEditor::mouseDrag(const juce::MouseEvent& e)
{
    if (dragSourceRow < 0) return;
    dragPos = e.getPosition();
    const int sH = controlsBounds.getHeight() / 4;
    const int rawRow = juce::jlimit(0, 3, (dragPos.y - controlsBounds.getY()) / sH);
    dragHoverRow = (rawRow != dragSourceRow) ? rawRow : -1;
    repaint();
}

void StardustEditor::mouseUp(const juce::MouseEvent& e)
{
    if (dragSourceRow >= 0 && dragHoverRow >= 0)
    {
        std::swap(chainSlots[dragSourceRow], chainSlots[dragHoverRow]);
        commitChainSlots();
        resized();
    }
    dragSourceRow = -1;
    dragHoverRow  = -1;
    repaint();
}

void StardustEditor::resized()
{
    const int margin = 8;
    const int gap = 10;

    const int panelW = getWidth() - margin * 2;

    // Bottom bar
    const int barH = 22;
    bottomBarBounds = { margin, getHeight() - margin - barH, panelW, barH };

    // 4 equal sections: 24px header + 68px knobs each = 92px per section
    const int panelH = 4 * 92;
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
        resized();
    };

    // ---- FX chain strip + active section panel ----
    syncChainSlots();

    const int toggleH = 14;
    const int toggleW = 26;
    const int numStripRows = 4;

    // Toggle buttons are kept for APVTS binding but rendered as name-pill in paint()
    juce::ignoreUnused(toggleH, toggleW);
    for (int fx = 1; fx <= 7; ++fx)
        toggleForSection(fx).setBounds({0, 0, 0, 0});

    // Hide all section knobs first, then lay out each occupied slot
    for (auto* k : { &destroyBitsKnob, &destroyRateKnob,
                     &destroyInKnob, &destroyOutKnob,
                     &hazeColorKnob,
                     &grainMixKnob, &grainCloudKnob, &grainScatterKnob, &grainSpaceKnob,
                     &grainReverbSizeKnob, &grainMorphKnob, &grainSizeSyncKnob, &grainRevKnob, &grainFeedbackKnob,
                     &chorusMixKnob, &chorusSpeedKnob, &multiplyDepthKnob, &multiplyToneKnob,
                     &multiplyFeedbackKnob, &multiplyShimmerKnob, &multiplyDriftKnob, &panOuterKnob, &panInnerKnob,
                     &tapeDriveKnob, &tapeInputKnob, &tapeGlueKnob, &tapeNoiseKnob,
                     &tapeMixKnob, &tapeOutputKnob, &tapeWowKnob,
                     &distortionDriveKnob, &distortionToneKnob, &distortionBiasKnob, &distortionAsymKnob,
                     &reverbMixKnob, &reverbSizeKnob, &reverbDecayKnob, &reverbDampKnob, &reverbPreDelayKnob, &reverbDiffusionKnob, &reverbWidthKnob })
        k->setBounds({0, 0, 0, 0});

    // Hide all sidebar mix sliders first
    for (auto* s : { &destroyMixStrip, &grainMixStrip, &multiplyMixStrip, &tapeMixStrip, &hazeMixStrip })
        s->setBounds({0, 0, 0, 0});

    hazeTypeLabel.setVisible(false);
    for (auto& b : hazeTypeBtn) b.setVisible(false);
    tapeNoiseSpeedLabel.setVisible(false);
    for (auto& b : tapeNoiseSpeedBtn) b.setVisible(false);
    distortionModeLabel.setVisible(false);
    for (auto& b : distortionModeBtn) b.setVisible(false);
    grainShapeLabel.setVisible(false);
    for (auto& b : grainShapeBtn) b.setVisible(false);
    grainQuantLabel.setVisible(false);
    for (auto& b : grainQuantBtn) b.setVisible(false);
    multiplyLfoLabel.setVisible(false);
    for (auto& b : multiplyLfoBtn) b.setVisible(false);
    multiplyVintageLabel.setVisible(false);
    for (auto& b : multiplyVintageBtn) b.setVisible(false);
    multiplyTempoSyncBtn.setVisible(false);

    // Pill constants (mirror paint() constants)
    static constexpr int kSbPillX = 18;
    static constexpr int kSbPillW = 92;
    static constexpr int kSbPillH = 28;
    juce::Slider* mixStripForFx[8] = { nullptr, &destroyMixStrip, &grainMixStrip,
                                        &multiplyMixStrip, &tapeMixStrip, nullptr, nullptr, &hazeMixStrip };

    // Layout all occupied slots simultaneously
    for (int row = 0; row < 4; ++row)
    {
        const int fx = chainSlots[row];
        if (fx == 0) continue;
        const auto kp = sectionKnobBounds(row);
        switch (fx)
        {
            case 1: layoutCrushSection(kp);      break;
            case 2: layoutGranularSection(kp);   break;
            case 3: layoutMultiplySection(kp);   break;
            case 4: layoutTapeSection(kp);       break;
            case 5: layoutDistortionSection(kp); break;
            case 6: layoutReverbSection(kp);     break;
            case 7: layoutHazeSection(kp);       break;
            default: break;
        }

        // Position sidebar mix slider below name pill (not all sections have one)
        const auto rb = stripRowBounds(row);
        const int sliderY = rb.getCentreY() + kSbPillH / 2 + 5;
        if (auto* strip = mixStripForFx[fx])
            strip->setBounds(rb.getX() + kSbPillX, sliderY, kSbPillW, 14);
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
