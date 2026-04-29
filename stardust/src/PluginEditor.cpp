#include "PluginEditor.h"
#include "BinaryData.h"
#include <array>
#include <set>

namespace
{
constexpr int kFixedSectionCount = 2;
}

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

    // Detect bipolar knob (min < 0 and max > 0)
    const bool isBipolar = (slider.getMinimum() < 0.0 && slider.getMaximum() > 0.0);
    const bool isHovered = slider.isEnabled() && (slider.isMouseOver(true) || slider.isMouseButtonDown());

    // Background track arc — brightens on hover
    {
        juce::Path bgArc;
        bgArc.addCentredArc(centreX, centreY, radius, radius, 0.0f,
                             rotaryStartAngle, rotaryEndAngle, true);
        g.setColour(isHovered ? kFgGhost.withAlpha(0.82f) : kFgGhost.withAlpha(0.62f));
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
        g.setColour(kFgGhost.withAlpha(isHovered ? 0.50f : 0.34f));
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
        const float fw = static_cast<float>(width);

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
        juce::GlyphArrangement valueGlyphs;
        valueGlyphs.addLineOfText(font, valueText, 0.0f, 0.0f);
        const int valueGlyphCount = valueGlyphs.getNumGlyphs();
        const float textW = valueGlyphCount > 0
            ? valueGlyphs.getBoundingBox(0, valueGlyphCount, true).getWidth()
            : 0.0f;
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

    // Compact horizontal bars used by sidebar mix strips.
    if (slider.getComponentID() == "sidebarMix")
    {
        const float fx = static_cast<float>(x);
        const float fy = static_cast<float>(y);
        const float fw = static_cast<float>(width);
        const float fh = static_cast<float>(height);
        const float trackY2 = fy + fh * 0.5f;
        const float trackLeft2  = fx + 7.0f;
        const float trackRight2 = fx + fw - 7.0f;
        const float trackW2 = trackRight2 - trackLeft2;

        const bool active = slider.isMouseOver(true) || slider.isMouseButtonDown();

        const double min = slider.getMinimum();
        const double max = slider.getMaximum();
        const float norm = max > min
            ? juce::jlimit(0.0f, 1.0f, static_cast<float>((slider.getValue() - min) / (max - min)))
            : 0.0f;
        const float thumbPos = trackLeft2 + norm * trackW2;
        const bool atMinimum = norm <= 0.001f;

        // Value fill. Bipolar gain controls fill from centre; macro/mix fill from left.
        const bool bipolar = slider.getMinimum() < 0.0 && slider.getMaximum() > 0.0;
        const float fillStart = bipolar ? (trackLeft2 + trackW2 * 0.5f) : trackLeft2;
        const float left = std::min(fillStart, thumbPos);
        const float right = std::max(fillStart, thumbPos);

        if (!atMinimum)
        {
            g.setColour(kFgGhost.withAlpha(0.14f));
            g.fillRoundedRectangle(trackLeft2, trackY2 - 1.5f, trackW2, 3.0f, 1.5f);
        }

        if (!atMinimum && right > left)
        {
            g.setColour(kAccent);
            g.fillRoundedRectangle(left, trackY2 - 1.5f, right - left, 3.0f, 1.5f);
        }

        if (bipolar)
        {
            const float centre = trackLeft2 + trackW2 * 0.5f;
            g.setColour(kFgDim.withAlpha(0.45f));
            g.fillRect(centre - 0.5f, trackY2 - 5.0f, 1.0f, 10.0f);
        }

        // Thumb dot
        const float dotR = active ? 4.2f : 3.4f;
        g.setColour(active ? kAccent : kAccent.withAlpha(0.80f));
        g.fillEllipse(thumbPos - dotR, trackY2 - dotR, dotR * 2.0f, dotR * 2.0f);

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

    if (button.getComponentID() == "characterFlavorBtn")
    {
        const float cornerR = bounds.getHeight() * 0.5f;
        const bool isOn = button.getToggleState();
        const float hover = shouldDrawButtonAsHighlighted ? 1.0f : 0.0f;

        g.setColour(isOn ? juce::Colour(0xFF151515) : juce::Colours::black.withAlpha(0.86f));
        g.fillRoundedRectangle(bounds.reduced(1.0f), cornerR);
        g.setColour(isOn ? kAccent.withAlpha(0.70f) : kFgGhost.withAlpha(0.32f + hover * 0.20f));
        g.drawRoundedRectangle(bounds.reduced(1.0f), cornerR, isOn ? 1.5f : 1.0f);

        if (hover > 0.0f)
        {
            g.setColour(kAccent.withAlpha(0.08f));
            g.fillRoundedRectangle(bounds.reduced(2.0f), cornerR);
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
    else if (button.getComponentID() == "characterFlavorBtn")
    {
        const bool isOn = button.getToggleState();
        g.setColour(isOn ? kAccent : (shouldDrawButtonAsHighlighted ? kFg : kFgDim));
        g.setFont(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(), 9.0f,
                                    isOn ? juce::Font::bold : juce::Font::plain));
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
    g.drawText(window.getName(), juce::roundToInt(bounds.getX()) + 14, juce::roundToInt(bounds.getY()) + 8,
               juce::roundToInt(bounds.getWidth()) - 28, 20,
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
        juce::GlyphArrangement displayGlyphs;
        displayGlyphs.addLineOfText(font, displayText, 0.0f, 0.0f);
        const int displayGlyphCount = displayGlyphs.getNumGlyphs();
        const float textW = displayGlyphCount > 0
            ? displayGlyphs.getBoundingBox(0, displayGlyphCount, true).getWidth()
            : 0.0f;
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

void StardustLookAndFeel::positionComboBoxText(juce::ComboBox& /*box*/, juce::Label& label)
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
    g.drawText("SETTINGS", juce::roundToInt(bounds.getX()), juce::roundToInt(bounds.getY()) + 10,
               juce::roundToInt(bounds.getWidth()), 20,
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
    starfield(p.apvts)
{
    setLookAndFeel(&lookAndFeel);
    setSize(620, 560);

    addAndMakeVisible(starfield);
    starfield.setInterceptsMouseClicks(false, false);
    starfield.setExcludeRect({});
    starfield.toBack();



    setupKnob(destroyDriveKnob, "destroyIn", "Crush");
    setupKnob(destroyBitsKnob, "destroyBits", "Bit");
    setupKnob(destroyJitterKnob, "destroyJitter", "Grit");

    setupKnob(exciterDriveKnob, "exciterDrive", "Heat");
    setupKnob(exciterToneKnob, "exciterTone", "Freq");

    // Rate knob — rotary attached to destroyFader param, integer Hz steps
    setupKnob(destroyRateKnob, "destroyFader", "Rate");
    destroyRateKnob.slider.setNumDecimalPlacesToDisplay(0);
    destroyRateKnob.slider.textFromValueFunction = [](double v) {
        return juce::String(juce::roundToInt(v)) + " Hz";
    };
    destroyRateKnob.slider.valueFromTextFunction = [](const juce::String& t) {
        float hz = t.getFloatValue();
        if (t.containsIgnoreCase("k"))
            hz *= 1000.0f;
        return static_cast<double>(juce::jlimit(250.0f, 96000.0f, std::round(hz)));
    };
    destroyRateKnob.slider.updateText();

    {
        static const char* kCharacterModes[] = { "Dust", "Glass", "Rust", "Heat", "Broken", "Glow" };
        auto* characterModeParam = dynamic_cast<juce::AudioParameterChoice*>(
            processorRef.apvts.getParameter("characterMode"));
        auto updateFlavorButtons = [this, characterModeParam]() {
            const int sel = characterModeParam ? characterModeParam->getIndex() : 0;
            for (int i = 0; i < 6; ++i)
                characterFlavorBtn[i].setToggleState(i == sel, juce::dontSendNotification);
        };
        for (int i = 0; i < 6; ++i)
        {
            characterModeCombo.addItem(kCharacterModes[i], i + 1);
            characterFlavorBtn[i].setButtonText(kCharacterModes[i]);
            characterFlavorBtn[i].setClickingTogglesState(false);
            characterFlavorBtn[i].setColour(juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
            characterFlavorBtn[i].setColour(juce::TextButton::buttonOnColourId, StardustLookAndFeel::kAccent.withAlpha(0.16f));
            characterFlavorBtn[i].setColour(juce::TextButton::textColourOffId, StardustLookAndFeel::kFgDim);
            characterFlavorBtn[i].setColour(juce::TextButton::textColourOnId, StardustLookAndFeel::kAccent);
            characterFlavorBtn[i].setComponentID("characterFlavorBtn");
            characterFlavorBtn[i].setInterceptsMouseClicks(false, false);
            characterFlavorBtn[i].onClick = [i, updateFlavorButtons, characterModeParam]() {
                if (characterModeParam != nullptr)
                    characterModeParam->setValueNotifyingHost(characterModeParam->convertTo0to1(static_cast<float>(i)));
                updateFlavorButtons();
            };
            addAndMakeVisible(characterFlavorBtn[i]);
        }
        characterModeCombo.setJustificationType(juce::Justification::centred);
        characterModeCombo.setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xFF151515));
        characterModeCombo.setColour(juce::ComboBox::textColourId, StardustLookAndFeel::kAccent);
        characterModeCombo.setColour(juce::ComboBox::outlineColourId, StardustLookAndFeel::kFgGhost.withAlpha(0.35f));
        characterModeCombo.setColour(juce::ComboBox::focusedOutlineColourId, StardustLookAndFeel::kAccent.withAlpha(0.4f));
        characterModeCombo.setColour(juce::ComboBox::arrowColourId, StardustLookAndFeel::kFgDim);
        characterModeAttach = std::make_unique<ComboBoxAttachment>(processorRef.apvts, "characterMode", characterModeCombo);
        addAndMakeVisible(characterModeCombo);
        updateFlavorButtons();
    }

    advancedToggleBtn.setVisible(false);

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
    setupMixStrip(destroyMixStrip,      destroyMixStripAttach,      "destroyMix");
    setupMixStrip(exciterMixStrip,      exciterMixStripAttach,      "exciterMix");
    for (auto* mixKnob : { &destroyMixStrip, &exciterMixStrip })
    {
        mixKnob->setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        mixKnob->setTextBoxStyle(juce::Slider::TextBoxBelow, false, 72, 18);
        mixKnob->setComponentID("");
        mixKnob->setMouseDragSensitivity(180);
        mixKnob->setPopupDisplayEnabled(false, false, nullptr);
        mixKnob->setColour(juce::Slider::textBoxTextColourId,       StardustLookAndFeel::kFg);
        mixKnob->setColour(juce::Slider::textBoxOutlineColourId,    StardustLookAndFeel::kFgGhost.withAlpha(0.22f));
        mixKnob->setColour(juce::Slider::textBoxBackgroundColourId, StardustLookAndFeel::kFgGhost.withAlpha(0.06f));
        mixKnob->textFromValueFunction = [](double v) {
            return juce::String(static_cast<int>(std::round(v * 100.0))) + "%";
        };
        mixKnob->valueFromTextFunction = [](const juce::String& t) {
            return t.getDoubleValue() / 100.0;
        };
        mixKnob->updateText();
        for (int i = 0; i < mixKnob->getNumChildComponents(); ++i)
        {
            if (auto* lbl = dynamic_cast<juce::Label*>(mixKnob->getChildComponent(i)))
            {
                lbl->setFont(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(), 13.0f, juce::Font::bold));
                lbl->setEditable(false, true, false);
                break;
            }
        }
    }
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
    stopTimer();

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

void StardustEditor::LabeledKnob::mouseDown(const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu())
    {
        // Forward right-clicks to the editor so the remove dropdown works
        // even when clicking in the knob's padding area
        if (auto* parent = getParentComponent())
            parent->mouseDown(e.getEventRelativeTo(parent));
        return;
    }
    juce::Component::mouseDown(e);
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
    if (paramId == "destroyIn")
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
    else if (paramId == "exciterTone")
    {
        knob.slider.textFromValueFunction = [](double v) {
            if (v >= 1000.0) return juce::String(v / 1000.0, 1) + " kHz";
            return juce::String(juce::roundToInt(v)) + " Hz";
        };
        knob.slider.valueFromTextFunction = [](const juce::String& t) {
            auto text = t.trim().toLowerCase();
            if (text.endsWith("khz"))
                return juce::jlimit(800.0, 16000.0, text.upToLastOccurrenceOf("k", false, false).getDoubleValue() * 1000.0);
            return juce::jlimit(800.0, 16000.0, text.getDoubleValue());
        };
    }
    else if (paramId == "destroyMix"
             || paramId == "characterAmount"
             || paramId == "exciterDrive"
             || paramId == "exciterMix")
    {
        knob.slider.textFromValueFunction = [](double v) {
            return juce::String(static_cast<int>(std::round(v * 100.0))) + "%";
        };
        knob.slider.valueFromTextFunction = [](const juce::String& t) {
            return t.getDoubleValue() / 100.0; // "50%" → 0.5
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
            return static_cast<double>(juce::jlimit(1.0f, 24.0f, t.getFloatValue()));
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
    struct PresetInfo { juce::String name; bool isFactory; juce::String bank; };
    std::vector<PresetInfo> presetInfos;
    int currentIdx;
    {
        PresetLockGuard guard(processorRef.presetLock);
        const auto& presets = processorRef.getAllPresets();
        currentIdx = processorRef.getCurrentProgram();
        presetInfos.reserve(presets.size());
        for (const auto& p : presets)
            presetInfos.push_back({ p.name, p.isFactory, p.bank });
    }
    auto favs = StardustProcessor::loadFavorites();

    // Favorites menu
    juce::PopupMenu favMenu;
    for (int i = 0; i < static_cast<int>(presetInfos.size()); ++i)
        if (favs.count(presetInfos[static_cast<size_t>(i)].name) > 0)
            favMenu.addItem(i + 1, presetInfos[static_cast<size_t>(i)].name, true, i == currentIdx);

    // Factory bank sub-menus (Init stays at top level)
    juce::PopupMenu factoryMenu;
    for (const auto& bankName : PresetLibraryPanel::kFactoryBankOrder)
    {
        juce::PopupMenu bankMenu;
        for (int i = 0; i < static_cast<int>(presetInfos.size()); ++i)
            if (presetInfos[static_cast<size_t>(i)].isFactory && presetInfos[static_cast<size_t>(i)].bank == bankName)
                bankMenu.addItem(i + 1, presetInfos[static_cast<size_t>(i)].name, true, i == currentIdx);
        if (bankMenu.getNumItems() > 0)
            factoryMenu.addSubMenu(bankName, bankMenu);
    }

    // User presets
    juce::PopupMenu userMenu;
    for (int i = 0; i < static_cast<int>(presetInfos.size()); ++i)
        if (!presetInfos[static_cast<size_t>(i)].isFactory && presetInfos[static_cast<size_t>(i)].bank.isEmpty())
            userMenu.addItem(i + 1, presetInfos[static_cast<size_t>(i)].name, true, i == currentIdx);

    // User bank sub-menus
    std::set<juce::String> userBanks;
    for (const auto& p : presetInfos)
        if (!p.isFactory && p.bank.isNotEmpty())
            userBanks.insert(p.bank);
    for (const auto& bankName : userBanks)
    {
        juce::PopupMenu bankMenu;
        for (int i = 0; i < static_cast<int>(presetInfos.size()); ++i)
            if (!presetInfos[static_cast<size_t>(i)].isFactory && presetInfos[static_cast<size_t>(i)].bank == bankName)
                bankMenu.addItem(i + 1, presetInfos[static_cast<size_t>(i)].name, true, i == currentIdx);
        userMenu.addSubMenu(bankName, bankMenu);
    }

    juce::PopupMenu menu;
    if (favMenu.getNumItems() > 0)
        menu.addSubMenu(juce::String::charToString(0x2605) + " Favorites", favMenu);

    // Init preset at top level
    for (int i = 0; i < static_cast<int>(presetInfos.size()); ++i)
        if (presetInfos[static_cast<size_t>(i)].isFactory && presetInfos[static_cast<size_t>(i)].bank.isEmpty())
            menu.addItem(i + 1, presetInfos[static_cast<size_t>(i)].name, true, i == currentIdx);

    if (factoryMenu.getNumItems() > 0)
        menu.addSubMenu("Factory", factoryMenu);
    if (userMenu.getNumItems() > 0)
        menu.addSubMenu("User", userMenu);

    // Apply our look and feel + position below the preset selector
    menu.setLookAndFeel(&lookAndFeel);
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

    // Build hierarchical menu: Init, then factory sub-menus, then user presets
    auto* rootMenu = presetSelector.getRootMenu();

    // Uncategorized factory presets (Init)
    for (int i = 0; i < static_cast<int>(presets.size()); ++i)
        if (presets[static_cast<size_t>(i)].isFactory && presets[static_cast<size_t>(i)].bank.isEmpty())
            rootMenu->addItem(i + 1, presets[static_cast<size_t>(i)].name);

    // Factory banks as sub-menus (fisheye character order)
    for (const auto& bankName : PresetLibraryPanel::kFactoryBankOrder)
    {
        juce::PopupMenu subMenu;
        for (int i = 0; i < static_cast<int>(presets.size()); ++i)
            if (presets[static_cast<size_t>(i)].isFactory && presets[static_cast<size_t>(i)].bank == bankName)
                subMenu.addItem(i + 1, presets[static_cast<size_t>(i)].name);
        if (subMenu.getNumItems() > 0)
            rootMenu->addSubMenu(bankName, subMenu);
    }

    // User presets (no bank) — separated from factory
    bool addedUserSep = false;
    for (int i = 0; i < static_cast<int>(presets.size()); ++i)
    {
        const auto idx = static_cast<size_t>(i);
        if (!presets[idx].isFactory && presets[idx].bank.isEmpty())
        {
            if (!addedUserSep) { rootMenu->addSeparator(); addedUserSep = true; }
            rootMenu->addItem(i + 1, presets[idx].name);
        }
    }

    // User banks as sub-menus
    std::set<juce::String> userBanks;
    for (const auto& p : presets)
        if (!p.isFactory && p.bank.isNotEmpty())
            userBanks.insert(p.bank);
    for (const auto& bankName : userBanks)
    {
        juce::PopupMenu subMenu;
        for (int i = 0; i < static_cast<int>(presets.size()); ++i)
            if (!presets[static_cast<size_t>(i)].isFactory && presets[static_cast<size_t>(i)].bank == bankName)
                subMenu.addItem(i + 1, presets[static_cast<size_t>(i)].name);
        rootMenu->addSubMenu(bankName, subMenu);
    }

    presetSelector.setSelectedId(processorRef.getCurrentProgram() + 1, juce::dontSendNotification);
    updateDoubleClickDefaults();

    // Update library panel if visible
    if (presetLibraryPanel != nullptr && presetLibraryPanel->isVisible())
    {
        std::vector<PresetListItem> factory, user;
        std::map<juce::String, std::vector<PresetListItem>> factoryBankItems, bankItems;
        for (int i = 0; i < static_cast<int>(presets.size()); ++i)
        {
            const auto idx = static_cast<size_t>(i);
            PresetListItem item { presets[idx].name, i, presets[idx].isFactory, false, presets[idx].bank };
            if (presets[idx].isFactory)
            {
                if (presets[idx].bank.isEmpty())
                    factory.push_back(item);
                else
                    factoryBankItems[presets[idx].bank].push_back(item);
            }
            else if (presets[idx].bank.isEmpty())
                user.push_back(item);
            else
                bankItems[presets[idx].bank].push_back(item);
        }
        presetLibraryPanel->updatePresets(std::move(factory), std::move(factoryBankItems),
                                          std::move(user), std::move(bankItems));
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
    std::map<juce::String, std::vector<PresetListItem>> factoryBankItems, bankItems;
    {
        PresetLockGuard guard(processorRef.presetLock);
        const auto& presets = processorRef.getAllPresets();
        for (int i = 0; i < static_cast<int>(presets.size()); ++i)
        {
            const auto idx = static_cast<size_t>(i);
            PresetListItem item { presets[idx].name, i, presets[idx].isFactory, false, presets[idx].bank };
            if (presets[idx].isFactory)
            {
                if (presets[idx].bank.isEmpty())
                    factory.push_back(item);
                else
                    factoryBankItems[presets[idx].bank].push_back(item);
            }
            else if (presets[idx].bank.isEmpty())
                user.push_back(item);
            else
                bankItems[presets[idx].bank].push_back(item);
        }
    }
    auto favs = StardustProcessor::loadFavorites();
    for (auto& item : factory) item.isFavorite = favs.count(item.name) > 0;
    for (auto& item : user) item.isFavorite = favs.count(item.name) > 0;
    for (auto& [bk, items] : factoryBankItems)
        for (auto& item : items) item.isFavorite = favs.count(item.name) > 0;
    for (auto& [bk, items] : bankItems)
        for (auto& item : items) item.isFavorite = favs.count(item.name) > 0;
    presetLibraryPanel->updatePresets(std::move(factory), std::move(factoryBankItems),
                                      std::move(user), std::move(bankItems));
    presetLibraryPanel->setCurrentPresetIndex(processorRef.getCurrentProgram());

    // Position: overlay on left side using stable editor bounds.
    const int panelW = 300;
    const int panelH = bottomBarBounds.getY() - galaxyBounds.getY();
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

}


// ============================================================================
// FX chain strip helpers
// ============================================================================

juce::Rectangle<int> StardustEditor::stripRowBounds(int row) const
{
    const int safeRow = juce::jlimit(0, kFixedSectionCount - 1, row);
    const int sH = controlsBounds.getHeight() / kFixedSectionCount;
    return { controlsBounds.getX(), controlsBounds.getY() + safeRow * sH,
             controlsBounds.getWidth(), sH };
}

juce::Rectangle<int> StardustEditor::sectionKnobBounds(int row) const
{
    // Center knob area: left sidebar (name pill) + full-width control zone.
    static constexpr int kLeftW  = 118; // badge + name pill
    static constexpr int kDragW  =   0;
    const int safeRow = juce::jlimit(0, kFixedSectionCount - 1, row);
    const int sH = controlsBounds.getHeight() / kFixedSectionCount;
    return { controlsBounds.getX() + kLeftW,
             controlsBounds.getY() + safeRow * sH,
             controlsBounds.getWidth() - kLeftW - kDragW,
             sH };
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

    // Jitter | Rate | Bits | Drive
    layoutKnobInBounds(destroyJitterKnob,  { ox + kw * 0, ky, kw, kKnobH });
    layoutKnobInBounds(destroyRateKnob,    { ox + kw * 1, ky, kw, kKnobH });
    layoutKnobInBounds(destroyBitsKnob,    { ox + kw * 2, ky, kw, kKnobH });
    layoutKnobInBounds(destroyDriveKnob,   { ox + kw * 3, ky, kw, kKnobH });
}

void StardustEditor::layoutExciterSection(juce::Rectangle<int> ap)
{
    static constexpr int kKnobH = 68;
    const int padX = 8;
    const int availW = ap.getWidth() - padX * 2;
    const int ox = ap.getX() + padX;
    const int ky = ap.getY() + (ap.getHeight() - kKnobH) / 2;
    const int kw = availW / 2;

    layoutKnobInBounds(exciterDriveKnob, { ox + kw * 0, ky, kw, kKnobH });
    layoutKnobInBounds(exciterToneKnob,  { ox + kw * 1, ky, kw, kKnobH });
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
    auto deg = [](float degrees) {
        return degrees * juce::MathConstants<float>::pi / 180.0f;
    };
    static constexpr float kFlavorAnglesDeg[6] = { -90.0f, -30.0f, 30.0f, 90.0f, 150.0f, 210.0f };

    // Static black background
    g.fillAll(juce::Colours::black);

    // ---- Advanced controls panel ----
    if (!controlsBounds.isEmpty())
    {
        auto drawRowLabel = [&](const juce::Rectangle<int>& rowAnchor, const juce::String& title) {
            if (rowAnchor.isEmpty())
                return;

            g.setFont(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(), 9.0f, juce::Font::bold));
            g.setColour(StardustLookAndFeel::kFgDim);
            g.drawText(title, controlsBounds.getX() + 2, rowAnchor.getCentreY() - 6,
                       24, 12, juce::Justification::centredLeft);
        };

        auto drawMixLabel = [&](const juce::Slider& slider, const juce::String& title) {
            const auto b = slider.getBounds();
            if (b.isEmpty())
                return;

            g.setFont(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(), 10.0f, juce::Font::plain));
            g.setColour(StardustLookAndFeel::kFg);
            g.drawText(title, b.getX(), b.getY() - 14, b.getWidth(), 14, juce::Justification::centred);
        };

        drawRowLabel(destroyJitterKnob.getBounds(), "GRIT");
        drawRowLabel(destroyDriveKnob.getBounds(), "AIR");
        drawMixLabel(destroyMixStrip, "Mix");
        drawMixLabel(exciterMixStrip, "Mix");
    }

    // ---- Center fisheye visualizer ----
    const auto screenf = screenBounds.toFloat();

    g.setColour(juce::Colours::black);
    g.fillEllipse(screenf.expanded(2.0f));

    const auto lensCentre = screenf.getCentre();
    const float selectorRadius = screenf.getWidth() * 0.5f + 44.0f;
    {
        g.setColour(StardustLookAndFeel::kFgGhost.withAlpha(0.28f));
        g.drawEllipse(lensCentre.x - selectorRadius, lensCentre.y - selectorRadius,
                      selectorRadius * 2.0f, selectorRadius * 2.0f, 1.2f);
    }

    for (int i = 0; i < 6; ++i)
    {
        const auto b = characterFlavorBtn[i].getBounds().toFloat();
        if (b.isEmpty())
            continue;

        const float angle = deg(kFlavorAnglesDeg[i]);
        const bool isSelected = characterFlavorBtn[i].getToggleState();
        const float active = isSelected ? characterFlavorAnim[i] : 0.0f;
        const float hover = (i == hoverCharacter) ? 1.0f : 0.0f;
        const float alpha = juce::jlimit(0.0f, 1.0f, active * 0.45f + hover * 0.18f);
        const float px = lensCentre.x + std::cos(angle) * selectorRadius;
        const float py = lensCentre.y + std::sin(angle) * selectorRadius;

        if (isSelected || hover > 0.0f)
        {
            g.setColour(isSelected ? StardustLookAndFeel::kAccent.withAlpha(0.70f)
                                   : StardustLookAndFeel::kFgGhost.withAlpha(0.38f));
            g.fillEllipse(px - 1.8f, py - 1.8f, 3.6f, 3.6f);
        }

        if (alpha > 0.001f)
        {
            const float glowR = 8.0f + active * 4.0f;
            g.setColour(StardustLookAndFeel::kAccent.withAlpha(alpha * 0.12f));
            g.fillEllipse(px - glowR, py - glowR, glowR * 2.0f, glowR * 2.0f);
            g.setColour(StardustLookAndFeel::kAccent.withAlpha(alpha * 0.60f));
            g.drawEllipse(px - glowR * 0.55f, py - glowR * 0.55f, glowR * 1.1f, glowR * 1.1f, 1.1f);
        }

        if (active > 0.01f)
        {
            juce::Path activeArc;
            const float juceAngle = angle + juce::MathConstants<float>::halfPi;
            activeArc.addCentredArc(lensCentre.x, lensCentre.y,
                                    selectorRadius, selectorRadius, 0.0f,
                                    juceAngle - deg(20.0f), juceAngle + deg(20.0f), true);
            g.setColour(StardustLookAndFeel::kAccent.withAlpha(0.22f + active * 0.45f));
            g.strokePath(activeArc, juce::PathStrokeType(3.0f, juce::PathStrokeType::curved,
                                                         juce::PathStrokeType::rounded));
        }
    }

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
        g.drawImage(logoImage, juce::roundToInt(bbf.getX()) + 8, juce::roundToInt(bbf.getY()) + 2,
                    static_cast<int>(logoW), static_cast<int>(logoH),
                    0, 0, logoImage.getWidth(), logoImage.getHeight());
    }

    g.setFont(juce::FontOptions(12.0f).withStyle("Bold"));
    g.drawText("Stardust", juce::roundToInt(bbf.getX() + 12.0f + logoW), juce::roundToInt(bbf.getY()),
               juce::roundToInt(bbf.getWidth() * 0.5f), juce::roundToInt(bbf.getHeight()),
               juce::Justification::centredLeft);

    // Version on right
    g.setColour(StardustLookAndFeel::kAccent);
    g.setFont(juce::FontOptions(10.0f).withStyle("Bold"));
    g.drawText("v" STARDUST_VERSION, juce::roundToInt(bbf.getRight()) - 60, juce::roundToInt(bbf.getY()),
               50, juce::roundToInt(bbf.getHeight()),
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

    // Draw fisheye lens border ON TOP of the starfield child component
    const auto screenf = screenBounds.toFloat();

    if (!libraryVisible)
    {
        g.setColour(juce::Colours::black.withAlpha(0.70f));
        g.drawEllipse(screenf.reduced(0.5f), 7.0f);
        g.setColour(StardustLookAndFeel::kFgGhost.withAlpha(0.58f));
        g.drawEllipse(screenf.reduced(1.5f), 4.0f);

        const float characterAmount = juce::jlimit(0.0f, 1.0f,
            processorRef.apvts.getRawParameterValue("characterAmount")->load(std::memory_order_relaxed));
        if (characterAmount > 0.001f || draggingCharacterAmount)
        {
            juce::Path amountArc;
            amountArc.addCentredArc(screenf.getCentreX(), screenf.getCentreY(),
                                    screenf.getWidth() * 0.5f + 7.0f,
                                    screenf.getHeight() * 0.5f + 7.0f,
                                    0.0f,
                                    -juce::MathConstants<float>::pi,
                                    -juce::MathConstants<float>::pi
                                        + juce::MathConstants<float>::twoPi * characterAmount,
                                    true);
            g.setColour(StardustLookAndFeel::kAccent.withAlpha(draggingCharacterAmount ? 0.86f : 0.42f));
            g.strokePath(amountArc, juce::PathStrokeType(2.0f, juce::PathStrokeType::curved,
                                                        juce::PathStrokeType::rounded));
        }

        const bool showCharacterReadout = draggingCharacterAmount || screenBounds.contains(getMouseXYRelative());
        if (showCharacterReadout)
        {
            const auto readout = juce::Rectangle<float>(
                screenf.getCentreX() - 62.0f, screenf.getCentreY() - 9.0f, 124.0f, 18.0f);
            g.setColour(juce::Colours::black.withAlpha(0.58f));
            g.fillRoundedRectangle(readout, 4.0f);
            g.setColour(StardustLookAndFeel::kAccent);
            g.drawRoundedRectangle(readout, 4.0f, 1.0f);
            g.setFont(juce::FontOptions(9.0f, juce::Font::bold));
            g.setColour(StardustLookAndFeel::kAccent);
            g.drawText("Character " + juce::String(juce::roundToInt(characterAmount * 100.0f)) + "%",
                       readout.toNearestInt(), juce::Justification::centred, false);
        }

        // Preset bar background + border is drawn in paint() so icons render on top
    }

}

void StardustEditor::parameterChanged(const juce::String& parameterID, float newValue)
{
    if (parameterID == "characterMode")
    {
        const int sel = juce::jlimit(0, 5, static_cast<int>(std::round(newValue)));
        auto safeThis = juce::Component::SafePointer<StardustEditor>(this);
        juce::MessageManager::callAsync([safeThis, sel]() {
            if (safeThis == nullptr) return;
            for (int i = 0; i < 6; ++i)
                safeThis->characterFlavorBtn[i].setToggleState(i == sel, juce::dontSendNotification);
        });
    }
    else if (parameterID == "characterAmount")
    {
        auto safeThis = juce::Component::SafePointer<StardustEditor>(this);
        juce::MessageManager::callAsync([safeThis]() {
            if (safeThis == nullptr) return;
            safeThis->repaint(safeThis->screenBounds.expanded(18));
        });
    }
    // Dirty state is now computed by comparing current values to loaded preset in timerCallback
}

void StardustEditor::mouseMove(const juce::MouseEvent& e)
{
    presetSelector.repaint();

    int newHoverRow = -1;
    const auto pos = e.getPosition();
    const int newHoverCharacter = characterModeAt(pos);
    const bool overFisheye = screenBounds.contains(pos);
    for (int row = 0; row < kFixedSectionCount; ++row)
    {
        auto rb = stripRowBounds(row);
        if (rb.contains(pos))
        {
            newHoverRow = row;
            break;
        }
    }
    setMouseCursor(overFisheye ? juce::MouseCursor::DraggingHandCursor
                               : juce::MouseCursor::NormalCursor);
    if (newHoverRow != hoverRow || newHoverCharacter != hoverCharacter)
    {
        hoverRow  = newHoverRow;
        hoverCharacter = newHoverCharacter;
        repaint();
    }
}

void StardustEditor::mouseExit(const juce::MouseEvent& /*e*/)
{
    if (hoverRow != -1 || hoverCharacter != -1)
    {
        hoverRow = -1;
        hoverCharacter = -1;
        setMouseCursor(juce::MouseCursor::NormalCursor);
        repaint();
    }
}

void StardustEditor::timerCallback()
{
    // Update strip hover state using current mouse position (works across child components)
    {
        const auto mp = getMouseXYRelative();
        int newHoverRow = -1;
        if (isMouseOver(true))
        {
            hoverCharacter = characterModeAt(mp);
            for (int row = 0; row < kFixedSectionCount; ++row)
            {
                auto rb = stripRowBounds(row);
                if (rb.contains(mp))
                {
                    newHoverRow = row;
                    break;
                }
            }
        }
        else
        {
            hoverCharacter = -1;
        }
        hoverRow = newHoverRow;
    }

    bool characterAnimChanged = false;
    for (int i = 0; i < 6; ++i)
    {
        const float target = characterFlavorBtn[i].getToggleState() ? 1.0f : 0.0f;
        const float previous = characterFlavorAnim[i];
        characterFlavorAnim[i] += (target - characterFlavorAnim[i]) * 0.22f;
        characterAnimChanged = characterAnimChanged || std::abs(characterFlavorAnim[i] - previous) > 0.001f;
    }

    presetSelector.repaint();
    if (characterAnimChanged || hoverRow != -1 || hoverCharacter != -1)
        repaint(); // refresh radial highlights and hover feedback

    // Sync preset selection with processor (for DAW session restore)
    const int procIdx = processorRef.getCurrentProgram();
    if (presetSelector.getSelectedId() != procIdx + 1)
        presetSelector.setSelectedId(procIdx + 1, juce::dontSendNotification);

    // Grace period after preset load — params may still be settling
    bool dirty = false;
    int grace = processorRef.presetLoadGrace.load();
    if (grace > 0)
    {
        processorRef.presetLoadGrace.fetch_sub(1);
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

    if (beginCharacterAmountDrag(e.getPosition()))
        return;

    if (selectCharacterModeAt(e.getPosition()))
        return;

    AudioProcessorEditor::mouseDown(e);
}

void StardustEditor::mouseDrag(const juce::MouseEvent& e)
{
    if (draggingCharacterAmount)
    {
        updateCharacterAmountFromFisheye(e.getPosition());
        return;
    }

    if (selectCharacterModeAt(e.getPosition()))
        return;

    AudioProcessorEditor::mouseDrag(e);
}

void StardustEditor::mouseUp(const juce::MouseEvent& e)
{
    if (draggingCharacterAmount)
    {
        draggingCharacterAmount = false;
        repaint(screenBounds.expanded(18));
        return;
    }

    AudioProcessorEditor::mouseUp(e);
}

int StardustEditor::characterModeAt(juce::Point<int> position) const
{
    if (screenBounds.isEmpty())
        return -1;

    const auto centre = screenBounds.toFloat().getCentre();
    const float dx = static_cast<float>(position.x) - centre.x;
    const float dy = static_cast<float>(position.y) - centre.y;
    const float distance = std::sqrt(dx * dx + dy * dy);
    const float selectorRadius = screenBounds.toFloat().getWidth() * 0.5f + 44.0f;

    if (std::abs(distance - selectorRadius) > 24.0f)
        return -1;

    float angleDeg = std::atan2(dy, dx) * 180.0f / juce::MathConstants<float>::pi;
    static constexpr float kFlavorAnglesDeg[6] = { -90.0f, -30.0f, 30.0f, 90.0f, 150.0f, 210.0f };

    int best = 0;
    float bestDelta = 999.0f;
    for (int i = 0; i < 6; ++i)
    {
        float delta = std::abs(angleDeg - kFlavorAnglesDeg[i]);
        delta = std::min(delta, 360.0f - delta);
        if (delta < bestDelta)
        {
            bestDelta = delta;
            best = i;
        }
    }

    if (bestDelta > 30.0f)
        return -1;

    return best;
}

bool StardustEditor::selectCharacterModeAt(juce::Point<int> position)
{
    const int best = characterModeAt(position);
    if (best < 0)
        return false;

    if (auto* characterModeParam = dynamic_cast<juce::AudioParameterChoice*>(
            processorRef.apvts.getParameter("characterMode")))
    {
        characterModeParam->setValueNotifyingHost(
            characterModeParam->convertTo0to1(static_cast<float>(best)));
        for (int i = 0; i < 6; ++i)
            characterFlavorBtn[i].setToggleState(i == best, juce::dontSendNotification);
        repaint();
        return true;
    }

    return false;
}

bool StardustEditor::beginCharacterAmountDrag(juce::Point<int> position)
{
    if (!screenBounds.contains(position))
        return false;

    draggingCharacterAmount = true;
    updateCharacterAmountFromFisheye(position);
    return true;
}

void StardustEditor::updateCharacterAmountFromFisheye(juce::Point<int> position)
{
    if (auto* characterAmountParam = dynamic_cast<juce::AudioParameterFloat*>(
            processorRef.apvts.getParameter("characterAmount")))
    {
        const float amount = juce::jlimit(0.0f, 1.0f,
            1.0f - static_cast<float>(position.y - screenBounds.getY())
                    / static_cast<float>(juce::jmax(1, screenBounds.getHeight())));
        characterAmountParam->setValueNotifyingHost(characterAmountParam->convertTo0to1(amount));
        repaint(screenBounds.expanded(18));
    }
}

void StardustEditor::resized()
{
    const int margin = 12;
    const int panelW = getWidth() - margin * 2;
    const int barH = 22;
    bottomBarBounds = { margin, getHeight() - margin - barH, panelW, barH };

    // Compact fisheye layout: central lens, radial character buttons, lower fine controls.
    const int topY = margin;
    const int presetH = 26;
    const int btnW = 26;
    const int selectorW = 190;
    int presetX = (getWidth() - (btnW + 4 + selectorW + 4 + btnW + 4 + btnW)) / 2;
    favoriteBtn.setBounds(presetX, topY, btnW, presetH); presetX += btnW + 4;
    presetSelector.setBounds(presetX, topY, selectorW, presetH);
    presetSelector.setJustificationType(juce::Justification::centred);
    presetX += selectorW + 4;
    prevPresetBtn.setBounds(presetX, topY, btnW, presetH); presetX += btnW + 4;
    nextPresetBtn.setBounds(presetX, topY, btnW, presetH);
    libraryBtn.setBounds(margin, topY, btnW, presetH);
    savePresetBtn.setBounds(getWidth() - margin - btnW * 2 - 8, topY, btnW, presetH);
    scaleBtn.setBounds(getWidth() - margin - btnW, topY, btnW, presetH);
    deletePresetBtn.setVisible(false);

    presetSelector.onChange = [this] {
        processorRef.loadPreset(presetSelector.getSelectedId() - 1);
        updateDoubleClickDefaults();
        updateFavoriteButton();
        resized();
    };

    const int mainTop = 84;
    const int lensSize = 220;
    screenBounds = { (getWidth() - lensSize) / 2, mainTop + 22, lensSize, lensSize };
    galaxyBounds = screenBounds.expanded(12);
    starfield.setBounds(screenBounds);
    starfield.setExcludeRect({});

    characterModeCombo.setBounds({0, 0, 0, 0});
    const int flavorW = 72;
    const int flavorH = 22;
    const auto lensCentre = screenBounds.toFloat().getCentre();
    const float flavorRadius = static_cast<float>(lensSize) * 0.5f + 46.0f;
    static constexpr float kFlavorAnglesDeg[6] = { -90.0f, -30.0f, 30.0f, 90.0f, 150.0f, 210.0f };
    for (int i = 0; i < 6; ++i)
    {
        characterFlavorBtn[i].setTransform({});
        const float angle = kFlavorAnglesDeg[i] * juce::MathConstants<float>::pi / 180.0f;
        const float cx = lensCentre.x + std::cos(angle) * flavorRadius;
        const float cy = lensCentre.y + std::sin(angle) * flavorRadius;
        characterFlavorBtn[i].setBounds(juce::roundToInt(cx - static_cast<float>(flavorW) * 0.5f),
                                        juce::roundToInt(cy - static_cast<float>(flavorH) * 0.5f),
                                        flavorW, flavorH);
    }

    advancedToggleBtn.setBounds({0, 0, 0, 0});

    // Hide all parameter controls before placing the active release controls.
    for (auto* k : { &destroyDriveKnob, &destroyBitsKnob, &destroyRateKnob, &destroyJitterKnob,
                     &exciterDriveKnob, &exciterToneKnob })
        k->setBounds({0, 0, 0, 0});

    for (auto* s : { &destroyMixStrip, &exciterMixStrip })
        s->setBounds({0, 0, 0, 0});

    const int fineW = panelW;
    const int fineH = 138;
    controlsBounds = { bottomBarBounds.getX(), bottomBarBounds.getY() - fineH - 8, fineW, fineH };
    const int innerX = controlsBounds.getX() + 28;
    const int innerW = controlsBounds.getWidth() - 56;
    const int topW = innerW / 4;
    const int bottomW = innerW / 4;
    const int knobH = 62;
    const int row1Y = controlsBounds.getY() + 8;
    const int row2Y = controlsBounds.getY() + 78;
    layoutKnobInBounds(destroyJitterKnob, { innerX + topW * 0, row1Y, topW, knobH });
    layoutKnobInBounds(destroyRateKnob,   { innerX + topW * 1, row1Y, topW, knobH });
    layoutKnobInBounds(destroyBitsKnob,   { innerX + topW * 2, row1Y, topW, knobH });
    destroyMixStrip.setBounds(innerX + topW * 3, row1Y + 14, topW, knobH - 14);

    layoutKnobInBounds(destroyDriveKnob,  { innerX + bottomW * 0, row2Y, bottomW, knobH });
    layoutKnobInBounds(exciterDriveKnob,  { innerX + bottomW * 1, row2Y, bottomW, knobH });
    layoutKnobInBounds(exciterToneKnob,   { innerX + bottomW * 2, row2Y, bottomW, knobH });
    exciterMixStrip.setBounds(innerX + bottomW * 3, row2Y + 14, bottomW, knobH - 14);
}
