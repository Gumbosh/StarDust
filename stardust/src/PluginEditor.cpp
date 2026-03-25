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
    setColour(juce::AlertWindow::outlineColourId, kFgGhost.withAlpha(0.5f));

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

void StardustLookAndFeel::drawLinearSlider(juce::Graphics& g, int x, int y,
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

void StardustLookAndFeel::drawButtonBackground(juce::Graphics& g, juce::Button& button,
                                                const juce::Colour& /*backgroundColour*/,
                                                bool shouldDrawButtonAsHighlighted,
                                                bool /*shouldDrawButtonAsDown*/)
{
    auto bounds = button.getLocalBounds().toFloat();

    bool isLibraryBtn = button.getComponentID() == "libraryBtn";
    bool isFavoriteBtn = button.getComponentID() == "favoriteBtn";
    bool isPresetBarBtn = button.getButtonText() == "..."
                          || button.getButtonText() == "<"
                          || button.getButtonText() == ">"
                          || isLibraryBtn
                          || isFavoriteBtn;
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
        const float lineW = 10.0f;
        const float gap = 3.5f;
        g.setColour(shouldDrawButtonAsHighlighted ? kAccent : kFg);
        for (int i = -1; i <= 1; ++i)
        {
            const float ly = cy + static_cast<float>(i) * gap;
            g.drawLine(cx - lineW * 0.5f, ly, cx + lineW * 0.5f, ly, 1.5f);
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
    else
    {
        g.setColour(shouldDrawButtonAsHighlighted ? kAccent : button.findColour(juce::TextButton::textColourOffId));
        g.setFont(juce::FontOptions(16.0f).withStyle("Bold"));
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

    // Hover highlight — same as preset bar buttons
    if (box.isMouseOver(true))
    {
        g.setColour(kAccent.withAlpha(0.08f));
        g.fillRoundedRectangle(bounds.reduced(1.0f), 3.0f);
    }

    // Measure text width to place chevron right after centered text
    auto font = getComboBoxFont(box);
    g.setFont(font);
    const bool isDirty = box.getName() == "dirty";
    auto displayText = box.getText() + (isDirty ? " *" : "");
    const float textW = font.getStringWidthFloat(displayText);
    const float centerX = static_cast<float>(width) * 0.5f;

    // Draw text centered in full width
    g.setColour(box.findColour(juce::ComboBox::textColourId));
    g.drawText(displayText, 0, 0, width, height, juce::Justification::centred);
    const float arrowGap = 6.0f;

    // Arrow — chevron right after centered text
    const float arrowSize = 6.0f;
    const float arrowX = centerX + textW * 0.5f + arrowGap + arrowSize * 0.5f;
    const float arrowY = static_cast<float>(height) * 0.5f;

    juce::Path arrow;
    arrow.startNewSubPath(arrowX - arrowSize * 0.5f, arrowY - arrowSize * 0.25f);
    arrow.lineTo(arrowX, arrowY + arrowSize * 0.25f);
    arrow.lineTo(arrowX + arrowSize * 0.5f, arrowY - arrowSize * 0.25f);

    g.setColour(kAccent.withAlpha(0.6f));
    g.strokePath(arrow, juce::PathStrokeType(1.5f, juce::PathStrokeType::curved,
                                              juce::PathStrokeType::rounded));
}

juce::Font StardustLookAndFeel::getComboBoxFont(juce::ComboBox& /*box*/)
{
    return juce::Font(juce::FontOptions(13.0f).withStyle("Bold"));
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
    setSize(280, 130);
    setWantsKeyboardFocus(true);

    titleLabel.setText(title, juce::dontSendNotification);
    titleLabel.setFont(juce::FontOptions(13.0f).withStyle("Bold"));
    titleLabel.setColour(juce::Label::textColourId, StardustLookAndFeel::kAccent);
    titleLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(titleLabel);

    textInput.setText(defaultText);
    textInput.setFont(juce::FontOptions(12.0f));
    textInput.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xFF111111));
    textInput.setColour(juce::TextEditor::textColourId, StardustLookAndFeel::kAccent);
    textInput.setColour(juce::TextEditor::outlineColourId, StardustLookAndFeel::kFgGhost.withAlpha(0.4f));
    textInput.setColour(juce::TextEditor::focusedOutlineColourId, StardustLookAndFeel::kAccent.withAlpha(0.4f));
    textInput.setColour(juce::CaretComponent::caretColourId, StardustLookAndFeel::kAccent);
    textInput.setJustification(juce::Justification::centred);
    addAndMakeVisible(textInput);

    confirmBtn.setButtonText("Save");
    confirmBtn.setColour(juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    confirmBtn.setColour(juce::TextButton::textColourOffId, StardustLookAndFeel::kFg);
    confirmBtn.onClick = [this] { doConfirm(); };
    addAndMakeVisible(confirmBtn);

    cancelBtn.setButtonText("Cancel");
    cancelBtn.setColour(juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    cancelBtn.setColour(juce::TextButton::textColourOffId, StardustLookAndFeel::kFgDim);
    cancelBtn.onClick = [this] {
        if (auto* parent = getParentComponent())
            parent->removeChildComponent(this);
        delete this;
    };
    addAndMakeVisible(cancelBtn);
}

void StardustDialog::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    // Dark background
    g.setColour(juce::Colour(0xFF050505));
    g.fillRoundedRectangle(bounds, 4.0f);

    // Border — same as plugin sections
    g.setColour(StardustLookAndFeel::kFgGhost.withAlpha(0.35f));
    g.drawRoundedRectangle(bounds, 4.0f, 2.0f);

    // Divider under title
    g.setColour(StardustLookAndFeel::kFgGhost.withAlpha(0.2f));
    g.drawHorizontalLine(32, bounds.getX() + 12.0f, bounds.getRight() - 12.0f);

    // Divider above buttons
    g.drawHorizontalLine(getHeight() - 40, bounds.getX() + 12.0f, bounds.getRight() - 12.0f);
}

void StardustDialog::resized()
{
    auto area = getLocalBounds().reduced(14, 0);
    titleLabel.setBounds(area.removeFromTop(32));
    area.removeFromTop(6);
    textInput.setBounds(area.removeFromTop(26).reduced(10, 0));

    if (bankCombo != nullptr)
    {
        area.removeFromTop(6);
        bankCombo->setBounds(area.removeFromTop(24).reduced(10, 0));

        if (newBankInput != nullptr && newBankInput->isVisible())
        {
            area.removeFromTop(4);
            newBankInput->setBounds(area.removeFromTop(24).reduced(10, 0));
        }
    }

    auto btnArea = getLocalBounds().reduced(14, 0).removeFromBottom(36);
    btnArea.removeFromBottom(4);
    const int btnW = 80;
    const int gap = 10;
    const int totalW = btnW * 2 + gap;
    const int startX = btnArea.getX() + (btnArea.getWidth() - totalW) / 2;
    cancelBtn.setBounds(startX, btnArea.getY(), btnW, 28);
    confirmBtn.setBounds(startX + btnW + gap, btnArea.getY(), btnW, 28);
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

void StardustDialog::doConfirm()
{
    auto text = textInput.getText().trim();
    text = text.removeCharacters("/\\:*?\"<>|");
    text = text.substring(0, 64);
    if (text.isEmpty()) return;

    if (onConfirmWithBank)
        onConfirmWithBank(text, getSelectedBank());
    else if (callback)
        callback(text);

    if (auto* parent = getParentComponent())
        parent->removeChildComponent(this);
    delete this;
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
    bankCombo->setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xFF111111));
    bankCombo->setColour(juce::ComboBox::textColourId, StardustLookAndFeel::kFg);
    bankCombo->setColour(juce::ComboBox::outlineColourId, StardustLookAndFeel::kFgGhost.withAlpha(0.4f));
    bankCombo->setColour(juce::ComboBox::arrowColourId, StardustLookAndFeel::kFg);
    addAndMakeVisible(bankCombo.get());

    newBankInput = std::make_unique<juce::TextEditor>();
    newBankInput->setFont(juce::FontOptions(11.0f));
    newBankInput->setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xFF111111));
    newBankInput->setColour(juce::TextEditor::textColourId, StardustLookAndFeel::kAccent);
    newBankInput->setColour(juce::TextEditor::outlineColourId, StardustLookAndFeel::kFgGhost.withAlpha(0.4f));
    newBankInput->setColour(juce::TextEditor::focusedOutlineColourId, StardustLookAndFeel::kAccent.withAlpha(0.4f));
    newBankInput->setColour(juce::CaretComponent::caretColourId, StardustLookAndFeel::kAccent);
    newBankInput->setTextToShowWhenEmpty("Bank name...", StardustLookAndFeel::kFgDim);
    newBankInput->setVisible(false);
    addAndMakeVisible(newBankInput.get());

    bankCombo->onChange = [this] {
        const bool isNewBank = (bankCombo->getSelectedId() == static_cast<int>(bankOptions.size()) + 2);
        newBankInput->setVisible(isNewBank);
        resized();
    };

    // Resize to fit the extra row(s)
    setSize(280, 170);
    resized();
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
// LevelMeter
// ============================================================================

LevelMeter::LevelMeter(std::atomic<float>& levelSource) : level(levelSource)
{
    startTimerHz(30);
}

void LevelMeter::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    g.setColour(StardustLookAndFeel::kInset);
    g.fillRoundedRectangle(bounds, 1.0f);

    const float meterHeight = bounds.getHeight() * displayLevel;
    auto meterArea = bounds;
    auto meterBounds = meterArea.removeFromBottom(meterHeight);

    if (displayLevel < 0.7f)
        g.setColour(StardustLookAndFeel::kFgGhost.brighter(0.3f));
    else if (displayLevel < 0.9f)
        g.setColour(StardustLookAndFeel::kFgDim);
    else
        g.setColour(StardustLookAndFeel::kAccent);

    g.fillRoundedRectangle(meterBounds, 1.0f);

    // Peak hold indicator
    if (peakLevel > 0.01f)
    {
        const float peakY = bounds.getBottom() - bounds.getHeight() * peakLevel;
        g.setColour(StardustLookAndFeel::kAccent.withAlpha(0.8f));
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
// StardustEditor
// ============================================================================

StardustEditor::StardustEditor(StardustProcessor& p)
    : AudioProcessorEditor(&p),
      processorRef(p),
      starfield(p.apvts, p.outputLevelLeft, p.outputLevelRight),
      inputMeterL(p.inputLevelLeft),
      inputMeterR(p.inputLevelRight),
      outputMeterL(p.outputLevelLeft),
      outputMeterR(p.outputLevelRight)
{
    setLookAndFeel(&lookAndFeel);
    setSize(560, 670);

    addAndMakeVisible(starfield);
    starfield.setExcludeRect({});
    starfield.toBack();

    setupKnob(driveKnob, "drive", "DRIVE");
    setupKnob(toneKnob, "tone", "TONE");

    setupKnob(bitsKnob, "bitDepth", "BITS");
    setupKnob(rateKnob, "sampleRate", "RATE");
    setupKnob(cutoffKnob, "filterCutoff", "CUTOFF");
    setupKnob(mixKnob, "mix", "MIX");

    setupKnob(grainMixKnob, "grainMix", "GRAIN");
    setupKnob(grainDensityKnob, "grainDensity", "DENSITY");
    setupKnob(grainSizeKnob, "grainSize", "SIZE");
    setupKnob(grainScatterKnob, "grainScatter", "SCATTER");
    setupKnob(widthKnob, "stereoWidth", "WIDTH");

    setupKnob(chorusMixKnob, "chorusMix", "MULTIPLY");
    setupKnob(panOuterKnob, "multiplyPanOuter", "1+2");
    setupKnob(panInnerKnob, "multiplyPanInner", "3+4");

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
    tuneLabel.setFont(juce::FontOptions(13.0f).withStyle("Bold"));
    tuneLabel.setColour(juce::Label::textColourId, StardustLookAndFeel::kFg);
    addAndMakeVisible(tuneLabel);

    tuneValueLabel.setText("0.0 st", juce::dontSendNotification);
    tuneValueLabel.setJustificationType(juce::Justification::centredRight);
    tuneValueLabel.setFont(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(), 13.0f, juce::Font::plain));
    tuneValueLabel.setColour(juce::Label::textColourId, StardustLookAndFeel::kAccent);
    addAndMakeVisible(tuneValueLabel);

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
                    auto* dlg = new StardustDialog("Save Preset", "", {});
                    dlg->setBankOptions(processorRef.getUserBanks());
                    dlg->onConfirmWithBank = [this](const juce::String& name, const juce::String& bank) {
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
                    addAndMakeVisible(dlg);
                    dlg->setCentrePosition(getWidth() / 2, getHeight() / 2);
                    dlg->toFront(true);
                    dlg->grabKeyboardFocus();
                }
                else if (result == 2)
                {
                    int idx2 = processorRef.getCurrentProgram();
                    auto oldName = processorRef.getProgramName(idx2);
                    juce::String presetBank;
                    {
                        SpinLockGuard g(processorRef.presetLock);
                        const auto& presets = processorRef.getAllPresets();
                        if (idx2 >= 0 && idx2 < static_cast<int>(presets.size()))
                            presetBank = presets[static_cast<size_t>(idx2)].bank;
                    }
                    auto* dlg = new StardustDialog("Rename Preset", oldName, [this, oldName, presetBank](const juce::String& newName) {
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
                    dlg->confirmBtn.setButtonText("Rename");
                    addAndMakeVisible(dlg);
                    dlg->setCentrePosition(getWidth() / 2, getHeight() / 2);
                    dlg->toFront(true);
                    dlg->grabKeyboardFocus();
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
        const auto& presets = processorRef.getAllPresets();
        int idx = processorRef.getCurrentProgram();
        if (idx < 0 || idx >= static_cast<int>(presets.size())) return;
        const auto& name = presets[static_cast<size_t>(idx)].name;
        auto favs = StardustProcessor::loadFavorites();
        if (favs.count(name) > 0) favs.erase(name);
        else                       favs.insert(name);
        StardustProcessor::saveFavorites(favs);
        updateFavoriteButton();
        // Update library panel if open
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
    setupToggle(distortionToggle, distortionToggleAttach, "distortionEnabled");
    setupToggle(destroyToggle, destroyToggleAttach, "destroyEnabled");
    setupToggle(granularToggle, granularToggleAttach, "granularEnabled");
    setupToggle(multiplyToggle, multiplyToggleAttach, "multiplyEnabled");

    addAndMakeVisible(inputMeterL);
    addAndMakeVisible(inputMeterR);
    addAndMakeVisible(outputMeterL);
    addAndMakeVisible(outputMeterR);

    logoImage = juce::ImageCache::getFromMemory(BinaryData::logo_png, BinaryData::logo_pngSize);

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
    knob.slider.setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);
    knob.attachment = std::make_unique<SliderAttachment>(processorRef.apvts, paramId, knob.slider);
    // Double-click resets to loaded preset value (set in updateDoubleClickDefaults)
    if (auto* param = processorRef.apvts.getParameter(paramId))
    {
        const float defaultVal = param->convertFrom0to1(param->getDefaultValue());
        knob.slider.setDoubleClickReturnValue(true, defaultVal);
        knob.slider.setPopupDisplayEnabled(true, true, this);
    }
    paramToKnob[paramId] = &knob;
    addAndMakeVisible(knob.slider);

    knob.label.setText(labelText, juce::dontSendNotification);
    knob.label.setJustificationType(juce::Justification::centred);
    knob.label.setFont(juce::FontOptions(9.0f).withStyle("Bold"));
    knob.label.setColour(juce::Label::textColourId, StardustLookAndFeel::kFg);
    addAndMakeVisible(knob.label);
}

void StardustEditor::layoutKnobInBounds(LabeledKnob& knob, juce::Rectangle<int> bounds)
{
    const auto labelHeight = 14;
    knob.slider.setBounds(bounds.removeFromTop(bounds.getHeight() - labelHeight));
    knob.label.setBounds(bounds);
}

void StardustEditor::showPresetDropdown()
{
    const auto& presets = processorRef.getAllPresets();
    auto favs = StardustProcessor::loadFavorites();

    juce::PopupMenu favMenu, factoryMenu, userMenu;

    for (int i = 0; i < static_cast<int>(presets.size()); ++i)
    {
        const int itemId = i + 1;
        const bool ticked = (i == processorRef.getCurrentProgram());
        if (favs.count(presets[static_cast<size_t>(i)].name) > 0)
            favMenu.addItem(itemId, presets[static_cast<size_t>(i)].name, true, ticked);
        if (presets[static_cast<size_t>(i)].isFactory)
            factoryMenu.addItem(itemId, presets[static_cast<size_t>(i)].name, true, ticked);
        else
            userMenu.addItem(itemId, presets[static_cast<size_t>(i)].name, true, ticked);
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
            updateDoubleClickDefaults();
            updateFavoriteButton();
        }
    });
}

void StardustEditor::refreshPresetList()
{
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
                SpinLockGuard g(processorRef.presetLock);
                const auto& presets = processorRef.getAllPresets();
                if (idx >= 0 && idx < static_cast<int>(presets.size()))
                    presetBank = presets[static_cast<size_t>(idx)].bank;
            }
            auto* dlg = new StardustDialog("Rename Preset", oldName, [this, oldName, presetBank](const juce::String& newName) {
                processorRef.saveUserPreset(newName, presetBank);
                auto dir = StardustProcessor::getUserPresetsDir();
                if (presetBank.isNotEmpty())
                    dir = dir.getChildFile(presetBank);
                auto oldFile = dir.getChildFile(oldName + ".xml");
                if (oldFile.existsAsFile()) oldFile.deleteFile();
                processorRef.refreshPresets();
                refreshPresetList();
            });
            dlg->confirmBtn.setButtonText("Rename");
            addAndMakeVisible(dlg);
            dlg->setCentrePosition(getWidth() / 2, getHeight() / 2);
            dlg->toFront(true);
            dlg->grabKeyboardFocus();
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

    // Build items
    auto favs = StardustProcessor::loadFavorites();
    std::vector<PresetListItem> factory, user;
    std::map<juce::String, std::vector<PresetListItem>> bankItems;
    const auto& presets = processorRef.getAllPresets();
    for (int i = 0; i < static_cast<int>(presets.size()); ++i)
    {
        PresetListItem item { presets[i].name, i, presets[i].isFactory,
                              favs.count(presets[i].name) > 0, presets[i].bank };
        if (presets[i].isFactory)
            factory.push_back(item);
        else if (presets[i].bank.isEmpty())
            user.push_back(item);
        else
            bankItems[presets[i].bank].push_back(item);
    }
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

void StardustEditor::updateFavoriteButton()
{
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

void StardustEditor::paint(juce::Graphics& g)
{
    // Static black background
    g.fillAll(juce::Colours::black);

    // ---- Controls panel — rounded border matching viewport style ----
    const auto cpf = controlsBounds.toFloat();

    g.setColour(juce::Colours::black);
    g.fillRoundedRectangle(cpf, 4.0f);

    // Section labels
    g.setFont(juce::FontOptions(8.0f).withStyle("Bold"));
    g.setColour(StardustLookAndFeel::kFgDim.withAlpha(0.6f));

    const int innerX = controlsBounds.getX() + 14;
    const int labelOffset = 22;
    const auto drawSection = [&](const char* name, int yOff) {
        g.setFont(juce::FontOptions(11.0f).withStyle("Bold"));
        g.setColour(StardustLookAndFeel::kFg);
        g.drawText(name, innerX + labelOffset, controlsBounds.getY() + yOff - 1, 90, 14, juce::Justification::centredLeft);
    };
    const auto drawDivider = [&](float yOff) {
        g.setColour(StardustLookAndFeel::kFgGhost.withAlpha(0.2f));
        g.drawHorizontalLine(static_cast<int>(cpf.getY() + yOff), cpf.getX() + 12.0f, cpf.getRight() - 12.0f);
    };

    // Section labels + dividers
    drawSection("DESTROY", 28);
    drawDivider(165.0f);
    drawSection("GRANULAR", 168);
    drawDivider(257.0f);
    drawSection("DISTORTION", 260);
    drawDivider(343.0f);
    drawSection("MULTIPLY", 346);

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
    const auto borderCol = StardustLookAndFeel::kFgGhost.withAlpha(0.35f);

    // Outer border
    g.setColour(borderCol);
    g.drawRoundedRectangle(gvf, 4.0f, 2.0f);

    // Depth lines
    g.drawLine(gvf.getX(), gvf.getY(), screenf.getX(), screenf.getY(), 2.0f);
    g.drawLine(gvf.getRight(), gvf.getY(), screenf.getRight(), screenf.getY(), 2.0f);
    g.drawLine(gvf.getX(), gvf.getBottom(), screenf.getX(), screenf.getBottom(), 2.0f);
    g.drawLine(gvf.getRight(), gvf.getBottom(), screenf.getRight(), screenf.getBottom(), 2.0f);

    // ---- Bottom bar — black with border, logo+name left, version right ----
    const auto bbf = bottomBarBounds.toFloat();

    g.setColour(juce::Colours::black);
    g.fillRoundedRectangle(bbf, 3.0f);
    g.setColour(StardustLookAndFeel::kFgGhost.withAlpha(0.35f));
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

    // Draw inner screen border ON TOP of the starfield child component
    const auto screenf = screenBounds.toFloat();
    const auto gvf = galaxyBounds.toFloat();

    if (!libraryVisible)
    {
        g.setColour(StardustLookAndFeel::kFgGhost.withAlpha(0.35f));
        g.drawRoundedRectangle(screenf, 3.0f, 2.0f);

        // IN/OUT labels in the padding gaps
        g.setFont(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(), 10.0f, juce::Font::bold));
        g.setColour(StardustLookAndFeel::kAccent);

        // "IN" label above left meters
        g.drawText("IN", static_cast<int>(gvf.getX()) + 2, screenBounds.getY() + 2, 24, 14, juce::Justification::centred);

        // "OUT" label above right meters
        g.drawText("OUT", galaxyBounds.getRight() - 28, screenBounds.getY() + 2, 26, 14, juce::Justification::centred);

        // Title "S t a r d u s t" centered on the galaxy viewport — fully white
        g.setFont(juce::FontOptions(17.0f).withStyle("Bold"));
        g.setColour(StardustLookAndFeel::kAccent.withAlpha(0.05f));
        g.drawText("S t a r d u s t", gvf.getX(), gvf.getY() + 5, gvf.getWidth(), 18, juce::Justification::centred);
        g.setColour(StardustLookAndFeel::kAccent);
        g.drawText("S t a r d u s t", gvf.getX() - 1, gvf.getY() + 4, gvf.getWidth(), 18, juce::Justification::centred);
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

    // Sync preset selection with processor (for DAW session restore)
    const int procIdx = processorRef.getCurrentProgram();
    if (presetSelector.getSelectedId() != procIdx + 1)
        presetSelector.setSelectedId(procIdx + 1, juce::dontSendNotification);

    // Compute dirty by comparing current param values to loaded preset
    bool dirty = false;
    const auto& allPresets = processorRef.getAllPresets();
    if (procIdx >= 0 && procIdx < static_cast<int>(allPresets.size()))
    {
        const auto& preset = allPresets[static_cast<size_t>(procIdx)];
        for (const auto& [paramId, presetVal] : preset.values)
        {
            if (auto* param = processorRef.apvts.getParameter(paramId))
            {
                const float currentVal = param->convertFrom0to1(param->getValue());
                if (std::abs(currentVal - presetVal) > 0.01f)
                {
                    dirty = true;
                    break;
                }
            }
        }
    }

    // Update dirty indicator
    if (dirty && presetSelector.getName() != "dirty")
    {
        presetSelector.setName("dirty");
        presetSelector.repaint();
    }
    else if (!dirty && presetSelector.getName() == "dirty")
    {
        presetSelector.setName("");
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
    const int gap = 6;

    const int panelW = getWidth() - margin * 2;
    const int panelH = 430;

    // Bottom bar
    const int barH = 22;
    bottomBarBounds = { margin, getHeight() - margin - barH, panelW, barH };

    // Controls panel: above bottom bar
    controlsBounds = { margin, bottomBarBounds.getY() - gap - panelH, panelW, panelH };

    // Galaxy viewport: top, between top margin and controls
    const int galaxyY = margin;
    const int galaxyH = controlsBounds.getY() - gap - galaxyY;
    galaxyBounds = { margin, galaxyY, panelW, galaxyH };

    // Screen bounds: padded area inside viewport where visuals render
    const int pad = 28;
    screenBounds = galaxyBounds.reduced(pad);

    // Starfield renders only in the screen area
    starfield.setBounds(screenBounds);
    starfield.setExcludeRect({});

    // Preset bar: [icon] [Preset Name v][★] [<][>] [...] — selector centered on screen
    const int presetH = 22;
    const int presetY = screenBounds.getBottom() + (galaxyBounds.getBottom() - screenBounds.getBottom() - presetH) / 2;
    const int iconW = 22;
    const int starW = 20;
    const int arrowW = 26;
    const int menuBtnW = 28;
    const int selectorW = 180;
    const int pGap = 4;

    // Center the selector in the galaxy width
    const int selectorX = galaxyBounds.getX() + (galaxyBounds.getWidth() - selectorW) / 2;

    // Position elements relative to centered selector
    libraryBtn.setBounds(selectorX - pGap - iconW, presetY, iconW, presetH);
    presetSelector.setBounds(selectorX, presetY, selectorW, presetH);
    presetSelector.setJustificationType(juce::Justification::centred);

    int cx = selectorX + selectorW;
    favoriteBtn.setBounds(cx, presetY, starW, presetH);          cx += starW + pGap;
    prevPresetBtn.setBounds(cx, presetY, arrowW, presetH);      cx += arrowW;
    nextPresetBtn.setBounds(cx, presetY, arrowW, presetH);      cx += arrowW + pGap;
    savePresetBtn.setBounds(cx, presetY, menuBtnW, presetH);
    deletePresetBtn.setVisible(false);

    presetSelector.onChange = [this] {
        processorRef.loadPreset(presetSelector.getSelectedId() - 1);
        updateDoubleClickDefaults();
        updateFavoriteButton();
    };

    // Shared grid: 5 columns, equal spacing, both rows use same X positions
    const int knobW = 87;
    const int knobH = 68;
    const int numCols = 5;
    const int totalW = knobW * numCols;
    const int gridX = controlsBounds.getX() + (controlsBounds.getWidth() - totalW) / 2;

    // All sections use the same 5-column grid for alignment
    // Columns: gridX, gridX+87, gridX+174, gridX+261, gridX+348

    // DESTROY section — 4 knobs on columns 1-4 (centered)
    const int secGap = 24; // consistent gap between sections
    const int row1Y = controlsBounds.getY() + 40;
    layoutKnobInBounds(bitsKnob,   { gridX + knobW / 2,              row1Y, knobW, knobH });
    layoutKnobInBounds(rateKnob,   { gridX + knobW / 2 + knobW,      row1Y, knobW, knobH });
    layoutKnobInBounds(cutoffKnob, { gridX + knobW / 2 + knobW * 2,  row1Y, knobW, knobH });
    layoutKnobInBounds(mixKnob,    { gridX + knobW / 2 + knobW * 3,  row1Y, knobW, knobH });

    const int tunePad = 16;
    const int pitchY = controlsBounds.getY() + 108;
    tuneLabel.setBounds(gridX + tunePad, pitchY, 50, 18);
    tuneValueLabel.setBounds(gridX + totalW - tunePad - 65, pitchY, 65, 18);
    tuneFader.setBounds(gridX + tunePad, pitchY + 17, totalW - tunePad * 2, 24);

    // GRANULAR section — 5 knobs on all 5 columns
    const int grainRowY = controlsBounds.getY() + 180;
    layoutKnobInBounds(grainMixKnob,     { gridX,              grainRowY, knobW, knobH });
    layoutKnobInBounds(grainDensityKnob, { gridX + knobW,      grainRowY, knobW, knobH });
    layoutKnobInBounds(grainSizeKnob,    { gridX + knobW * 2,  grainRowY, knobW, knobH });
    layoutKnobInBounds(grainScatterKnob, { gridX + knobW * 3,  grainRowY, knobW, knobH });
    layoutKnobInBounds(widthKnob,        { gridX + knobW * 4,  grainRowY, knobW, knobH });

    // DISTORTION section — 2 knobs on columns 2-3 (aligned with DESTROY cols 2-3)
    const int distY = controlsBounds.getY() + 272;
    layoutKnobInBounds(driveKnob, { gridX + knobW / 2 + knobW,      distY, knobW, knobH });
    layoutKnobInBounds(toneKnob,  { gridX + knobW / 2 + knobW * 2,  distY, knobW, knobH });

    // MULTIPLY section — 3 knobs on columns 2-4
    const int multiplyY = controlsBounds.getY() + 358;
    layoutKnobInBounds(chorusMixKnob, { gridX + knobW,      multiplyY, knobW, knobH });
    layoutKnobInBounds(panOuterKnob,  { gridX + knobW * 2,  multiplyY, knobW, knobH });
    layoutKnobInBounds(panInnerKnob,  { gridX + knobW * 3,  multiplyY, knobW, knobH });

    // Section toggle buttons
    const int toggleH = 12;
    const int toggleW = 20;
    const int tInnerX = controlsBounds.getX() + 12;
    destroyToggle.setBounds(tInnerX, controlsBounds.getY() + 27, toggleW, toggleH);
    granularToggle.setBounds(tInnerX, controlsBounds.getY() + 167, toggleW, toggleH);
    distortionToggle.setBounds(tInnerX, controlsBounds.getY() + 259, toggleW, toggleH);
    multiplyToggle.setBounds(tInnerX, controlsBounds.getY() + 345, toggleW, toggleH);

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
