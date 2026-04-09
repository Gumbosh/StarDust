#include "PresetLibraryPanel.h"
#include "../PluginEditor.h"

// Colour palette — sourced from StardustLookAndFeel for consistency
static const juce::Colour kBg       = StardustLookAndFeel::kBg;
static const juce::Colour kPanelBg  { 0xFF111111 };
static const juce::Colour kFg       = StardustLookAndFeel::kFg;
static const juce::Colour kFgDim    = StardustLookAndFeel::kFgDim;
static const juce::Colour kFgGhost  = StardustLookAndFeel::kFgGhost;
static const juce::Colour kAccent   = StardustLookAndFeel::kAccent;

// ============================================================================
// PresetLibraryPanel
// ============================================================================

PresetLibraryPanel::PresetLibraryPanel()
{
    setOpaque(true);
    setWantsKeyboardFocus(true);

    closeBtn.setButtonText("X");
    closeBtn.setColour(juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    closeBtn.setColour(juce::TextButton::textColourOffId, kFgDim);
    closeBtn.onClick = [this] { if (onClose) onClose(); };
    addAndMakeVisible(closeBtn);

    // Search field
    searchField.setTextToShowWhenEmpty("Search...", kFgDim);
    searchField.setColour(juce::TextEditor::backgroundColourId, kBg);
    searchField.setColour(juce::TextEditor::textColourId, kFg);
    searchField.setColour(juce::TextEditor::outlineColourId, kFgGhost.withAlpha(0.5f));
    searchField.setColour(juce::TextEditor::focusedOutlineColourId, kFgDim);
    searchField.setColour(juce::CaretComponent::caretColourId, kFg);
    searchField.setFont(juce::FontOptions(11.0f));
    searchField.setJustification(juce::Justification::centredLeft);
    searchField.setIndents(4, 0);
    searchField.onTextChange = [this] {
        searchQuery = searchField.getText().trim();
        applyFilter();
    };
    addAndMakeVisible(searchField);

    addAndMakeVisible(bankList);

    presetViewport.setViewedComponent(&presetList, false);
    presetViewport.setScrollBarsShown(true, false);
    presetViewport.getVerticalScrollBar().setColour(
        juce::ScrollBar::thumbColourId, kFgGhost);
    addAndMakeVisible(presetViewport);

    // Load Bank button
    loadBankBtn.setButtonText("+ Load Bank");
    loadBankBtn.setColour(juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    loadBankBtn.setColour(juce::TextButton::textColourOffId, kFgDim);
    loadBankBtn.onClick = [this] {
        if (fileChooser != nullptr) return; // guard against re-entry
        fileChooser = std::make_unique<juce::FileChooser>(
            "Select a preset bank folder...",
            juce::File::getSpecialLocation(juce::File::userHomeDirectory));
        auto safeThis = juce::Component::SafePointer<PresetLibraryPanel>(this);
        fileChooser->launchAsync(juce::FileBrowserComponent::openMode
                                 | juce::FileBrowserComponent::canSelectDirectories,
            [safeThis](const juce::FileChooser& fc) {
                auto result = fc.getResult();
                if (safeThis == nullptr) return;
                safeThis->fileChooser.reset();
                if (result.isDirectory() && safeThis->onImportBank)
                    safeThis->onImportBank(result);
            });
    };
    addAndMakeVisible(loadBankBtn);
}

void PresetLibraryPanel::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    g.fillAll(juce::Colour(0xFF0A0A0A));
    g.setColour(kPanelBg);
    g.fillRoundedRectangle(bounds, 6.0f);

    // Border
    g.setColour(kFgGhost.withAlpha(0.5f));
    g.drawRoundedRectangle(bounds.reduced(0.5f), 6.0f, 1.0f);

    // Header background
    auto headerBounds = getLocalBounds().removeFromTop(kHeaderH).toFloat();
    g.setColour(kBg);
    g.fillRoundedRectangle(headerBounds.reduced(1.0f, 1.0f).withBottom(headerBounds.getBottom()), 5.0f);

    // Header title
    g.setColour(kAccent);
    g.setFont(juce::FontOptions(12.0f).withStyle("Bold"));
    g.drawText("PRESETS", headerBounds.withLeft(12.0f), juce::Justification::centredLeft);

    // Header divider
    g.setColour(kFgGhost.withAlpha(0.4f));
    g.drawHorizontalLine(kHeaderH, 1.0f, static_cast<float>(getWidth()) - 1.0f);

    // Vertical divider between bank list and preset list
    const float divX = static_cast<float>(kBankW);
    g.setColour(kFgGhost.withAlpha(0.3f));
    g.drawVerticalLine(static_cast<int>(divX), static_cast<float>(kHeaderH), static_cast<float>(getHeight()));
}

void PresetLibraryPanel::resized()
{
    auto area = getLocalBounds();

    // Close button
    closeBtn.setBounds(area.getWidth() - 30, 4, 26, 24);

    // Below header
    auto body = area.withTrimmedTop(kHeaderH + 1);

    // Left column — bank list + load bank button
    auto leftCol = body.removeFromLeft(kBankW);
    loadBankBtn.setBounds(leftCol.removeFromBottom(kLoadBankBtnH).reduced(4, 2));
    bankList.setBounds(leftCol);

    // Search field at top of right column
    auto searchArea = body.removeFromTop(kSearchH);
    searchField.setBounds(searchArea.reduced(6, 3));

    // Right column (viewport)
    presetViewport.setBounds(body);
    presetList.updateHeight();
}

bool PresetLibraryPanel::keyPressed(const juce::KeyPress& key)
{
    if (key == juce::KeyPress::escapeKey)
    {
        if (onClose) onClose();
        return true;
    }
    return false;
}

void PresetLibraryPanel::updatePresets(std::vector<PresetListItem> factory,
                                        std::map<juce::String, std::vector<PresetListItem>> factoryBanks,
                                        std::vector<PresetListItem> user,
                                        std::map<juce::String, std::vector<PresetListItem>> bankItems)
{
    factoryItems = std::move(factory);
    factoryBankItemsMap = std::move(factoryBanks);
    userItems = std::move(user);
    bankItemsMap = std::move(bankItems);

    bankNames.clear();
    for (const auto& pair : bankItemsMap)
        bankNames.push_back(pair.first);
    std::sort(bankNames.begin(), bankNames.end());

    // Clamp selectedBank if banks were removed
    if (bankList.selectedBank >= bankList.totalBankCount())
        bankList.selectedBank = 1; // fall back to first factory bank

    rebuildFavorites();
    applyFilter();
    bankList.repaint();
}

void PresetLibraryPanel::setCurrentPresetIndex(int index)
{
    currentPresetIndex = index;
    presetList.repaint();
}

void PresetLibraryPanel::setFavorites(const std::set<juce::String>& favs)
{
    for (auto& item : factoryItems)
        item.isFavorite = favs.count(item.name) > 0;
    for (auto& [bk, items] : factoryBankItemsMap)
        for (auto& item : items)
            item.isFavorite = favs.count(item.name) > 0;
    for (auto& item : userItems)
        item.isFavorite = favs.count(item.name) > 0;
    for (auto& [bk, items] : bankItemsMap)
        for (auto& item : items)
            item.isFavorite = favs.count(item.name) > 0;
    rebuildFavorites();
    applyFilter();
}

const std::vector<PresetListItem>& PresetLibraryPanel::activeItems() const
{
    const int sel = bankList.selectedBank;

    // 0 = FAVORITES
    if (sel == 0) return favoriteItems;

    // 1..kNumFactoryBanks = factory sub-banks
    if (sel >= 1 && sel <= kNumFactoryBanks)
    {
        const auto& bankName = kFactoryBankOrder[sel - 1];
        auto it = factoryBankItemsMap.find(bankName);
        if (it != factoryBankItemsMap.end())
            return it->second;
        return factoryItems; // fallback to uncategorized
    }

    // kNumFactoryBanks+1 = USER
    const int userIdx = kNumFactoryBanks + 1;
    if (sel == userIdx) return userItems;

    // kNumFactoryBanks+2+ = dynamic user banks
    const int bankIdx = sel - BankList::kFixedBanks;
    if (bankIdx >= 0 && bankIdx < static_cast<int>(bankNames.size()))
    {
        auto it = bankItemsMap.find(bankNames[static_cast<size_t>(bankIdx)]);
        if (it != bankItemsMap.end())
            return it->second;
    }
    return userItems; // fallback
}

void PresetLibraryPanel::applyFilter()
{
    const auto& items = activeItems();
    filteredIndices.clear();

    if (searchQuery.isEmpty())
    {
        for (int i = 0; i < static_cast<int>(items.size()); ++i)
            filteredIndices.push_back(i);
    }
    else
    {
        for (int i = 0; i < static_cast<int>(items.size()); ++i)
        {
            if (items[static_cast<size_t>(i)].name.containsIgnoreCase(searchQuery))
                filteredIndices.push_back(i);
        }
    }

    presetList.updateHeight();
    presetList.repaint();
}

void PresetLibraryPanel::rebuildFavorites()
{
    favoriteItems.clear();
    for (const auto& item : factoryItems)
        if (item.isFavorite) favoriteItems.push_back(item);
    for (const auto& [bk, items] : factoryBankItemsMap)
        for (const auto& item : items)
            if (item.isFavorite) favoriteItems.push_back(item);
    for (const auto& item : userItems)
        if (item.isFavorite) favoriteItems.push_back(item);
    for (const auto& [bk, items] : bankItemsMap)
        for (const auto& item : items)
            if (item.isFavorite) favoriteItems.push_back(item);
}

// ============================================================================
// BankList
// ============================================================================

PresetLibraryPanel::BankList::BankList(PresetLibraryPanel& o) : owner(o)
{
    setRepaintsOnMouseActivity(true);
}

int PresetLibraryPanel::BankList::totalBankCount() const
{
    return kFixedBanks + static_cast<int>(owner.bankNames.size());
}

int PresetLibraryPanel::BankList::bankAtY(int y) const
{
    const int startY = 12;
    const int rowH = 30;
    if (y < startY) return -1;

    // Fixed banks: FAVORITES(0), factory banks(1..4), USER(5)
    const int fixedEndY = startY + kFixedBanks * rowH;
    if (y < fixedEndY)
        return (y - startY) / rowH;

    // Separator gap before dynamic user banks
    if (owner.bankNames.empty()) return -1;
    const int dynamicStartY = fixedEndY + kSeparatorH;
    if (y < dynamicStartY) return -1;

    // Dynamic user banks
    const int dynIdx = (y - dynamicStartY) / rowH;
    const int bankIdx = kFixedBanks + dynIdx;
    return (bankIdx < totalBankCount()) ? bankIdx : -1;
}

void PresetLibraryPanel::BankList::paint(juce::Graphics& g)
{
    // Bank layout: FAVORITES, Lo-Fi, Glitch, Tape, Atmosphere, USER, [dynamic...]
    const juce::String fixedNames[] = {
        "FAVORITES",
        "LO-FI", "GLITCH", "TAPE", "ATMOSPHERE",
        "USER"
    };
    const int startY = 12;
    const int rowH = 30;

    auto drawBankRow = [&](int bankIdx, const juce::String& label, float rowY, bool showStar) {
        auto rowBounds = juce::Rectangle<float>(0.0f, rowY,
                                                 static_cast<float>(getWidth()), static_cast<float>(rowH));

        const bool isSelected = (bankIdx == selectedBank);
        const bool isHovered = (bankIdx == hoveredBank);

        if (isSelected)
        {
            g.setColour(kAccent.withAlpha(0.08f));
            g.fillRoundedRectangle(rowBounds.reduced(4.0f, 2.0f), 3.0f);
            g.setColour(kAccent);
            g.fillRect(0.0f, rowBounds.getY() + 4.0f, 2.0f, rowBounds.getHeight() - 8.0f);
        }
        else if (isHovered)
        {
            g.setColour(kAccent.withAlpha(0.04f));
            g.fillRoundedRectangle(rowBounds.reduced(4.0f, 2.0f), 3.0f);
        }

        g.setColour(isSelected ? kAccent : (isHovered ? kFg : kFgDim));
        g.setFont(juce::FontOptions(10.0f).withStyle("Bold"));

        if (showStar)
            g.drawText(juce::String::charToString(0x2605) + " " + label,
                        rowBounds.withLeft(10.0f), juce::Justification::centredLeft);
        else
            g.drawText(label, rowBounds.withLeft(14.0f), juce::Justification::centredLeft);
    };

    // Draw all fixed banks (FAVORITES + 4 factory banks + USER)
    for (int i = 0; i < kFixedBanks; ++i)
        drawBankRow(i, fixedNames[i], static_cast<float>(startY + i * rowH), i == 0);

    // Draw separator and dynamic user banks
    if (!owner.bankNames.empty())
    {
        const int sepY = startY + kFixedBanks * rowH + kSeparatorH / 2;
        g.setColour(kFgGhost.withAlpha(0.3f));
        g.drawHorizontalLine(sepY, 8.0f, static_cast<float>(getWidth()) - 8.0f);

        const int dynamicStartY = startY + kFixedBanks * rowH + kSeparatorH;

        for (int i = 0; i < static_cast<int>(owner.bankNames.size()); ++i)
        {
            const int bankIdx = kFixedBanks + i;
            drawBankRow(bankIdx,
                        owner.bankNames[static_cast<size_t>(i)].toUpperCase(),
                        static_cast<float>(dynamicStartY + i * rowH), false);
        }
    }
}

void PresetLibraryPanel::BankList::mouseMove(const juce::MouseEvent& e)
{
    const int newHover = bankAtY(e.y);
    if (newHover != hoveredBank)
    {
        hoveredBank = newHover;
        repaint();
    }
}

void PresetLibraryPanel::BankList::mouseExit(const juce::MouseEvent&)
{
    hoveredBank = -1;
    repaint();
}

void PresetLibraryPanel::BankList::mouseDown(const juce::MouseEvent& e)
{
    const int bank = bankAtY(e.y);
    if (bank < 0) return;

    // Right-click on dynamic banks → context menu
    if (e.mods.isPopupMenu() && bank >= kFixedBanks)
    {
        const int dynIdx = bank - kFixedBanks;
        if (dynIdx < 0 || dynIdx >= static_cast<int>(owner.bankNames.size()))
            return;

        const juce::String bankName = owner.bankNames[static_cast<size_t>(dynIdx)];
        juce::PopupMenu menu;
        menu.addItem(1, "Rename Bank...");
        menu.addItem(2, "Delete Bank");
        auto safeBankList = juce::Component::SafePointer<BankList>(this);
        menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this),
            [safeBankList, bankName](int result) {
                if (safeBankList == nullptr) return;
                auto& ownerRef = safeBankList->owner;
                if (result == 1 && ownerRef.onRenameBank)
                {
                    // Show rename dialog via AlertWindow
                    auto* aw = new juce::AlertWindow("Rename Bank", "Enter new name:",
                                                      juce::MessageBoxIconType::NoIcon);
                    aw->addTextEditor("name", bankName);
                    aw->addButton("Rename", 1);
                    aw->addButton("Cancel", 0);
                    auto safeInner = juce::Component::SafePointer<BankList>(safeBankList);
                    aw->enterModalState(true, juce::ModalCallbackFunction::create(
                        [safeInner, bankName, aw](int result2) {
                            std::unique_ptr<juce::AlertWindow> owned(aw);
                            if (safeInner == nullptr || result2 != 1) return;
                            auto newName = owned->getTextEditorContents("name").trim();
                            newName = newName.removeCharacters("/\\:*?\"<>|").substring(0, 64);
                            if (newName.isNotEmpty() && newName != bankName)
                                safeInner->owner.onRenameBank(bankName, newName);
                        }), false);
                }
                else if (result == 2 && ownerRef.onDeleteBank)
                {
                    ownerRef.onDeleteBank(bankName);
                }
            });
        return;
    }

    // Left click — select bank
    if (bank != selectedBank)
    {
        selectedBank = bank;
        repaint();
        owner.searchField.clear();
        owner.searchQuery = {};
        owner.applyFilter();
    }
}

// ============================================================================
// PresetList
// ============================================================================

PresetLibraryPanel::PresetList::PresetList(PresetLibraryPanel& o) : owner(o)
{
    setRepaintsOnMouseActivity(true);
}

int PresetLibraryPanel::PresetList::rowAtY(int y) const
{
    if (y < 0) return -1;
    int idx = y / PresetLibraryPanel::kRowH;
    return (idx >= 0 && idx < static_cast<int>(owner.filteredIndices.size())) ? idx : -1;
}

void PresetLibraryPanel::PresetList::paint(juce::Graphics& g)
{
    const auto& items = owner.activeItems();
    const auto& filtered = owner.filteredIndices;

    if (filtered.empty())
    {
        g.setColour(kFgGhost);
        g.setFont(juce::FontOptions(11.0f).withStyle("Italic"));
        const auto msg = owner.searchQuery.isNotEmpty() ? "No matches" : "No presets";
        g.drawText(msg, getLocalBounds().withTrimmedTop(8).withTrimmedLeft(12),
                   juce::Justification::topLeft);
        return;
    }

    for (int row = 0; row < static_cast<int>(filtered.size()); ++row)
    {
        const int itemIdx = filtered[static_cast<size_t>(row)];
        const auto& item = items[static_cast<size_t>(itemIdx)];

        auto rowBounds = juce::Rectangle<float>(
            0.0f, static_cast<float>(row * kRowH),
            static_cast<float>(getWidth()), static_cast<float>(kRowH));

        const bool isActive = (item.presetIndex == owner.currentPresetIndex);
        const bool isHovered = (row == hoveredRow);

        // Background
        if (isActive)
        {
            g.setColour(kAccent.withAlpha(0.10f));
            g.fillRoundedRectangle(rowBounds.reduced(4.0f, 1.0f), 3.0f);
            g.setColour(kAccent);
            g.fillRect(4.0f, rowBounds.getY() + 4.0f, 2.0f, rowBounds.getHeight() - 8.0f);
        }
        else if (isHovered)
        {
            g.setColour(kAccent.withAlpha(0.06f));
            g.fillRoundedRectangle(rowBounds.reduced(4.0f, 1.0f), 3.0f);
        }

        // Preset name
        g.setColour(isActive ? kAccent : (isHovered ? kFg : kFgDim));
        g.setFont(isActive ? juce::FontOptions(11.0f).withStyle("Bold") : juce::FontOptions(11.0f));
        g.drawText(item.name,
                   rowBounds.withLeft(14.0f).withRight(rowBounds.getRight() - kStarW - 4.0f),
                   juce::Justification::centredLeft);

        // Star icon (right side)
        auto starBounds = rowBounds.withLeft(rowBounds.getRight() - kStarW);
        g.setColour(item.isFavorite ? kAccent : kFgDim);
        g.setFont(juce::FontOptions(13.0f));
        g.drawText(item.isFavorite ? juce::String::charToString(0x2605)    // filled
                                   : juce::String::charToString(0x2606),   // outline
                   starBounds, juce::Justification::centred);
    }
}

void PresetLibraryPanel::PresetList::mouseMove(const juce::MouseEvent& e)
{
    const int newHover = rowAtY(e.y);
    if (newHover != hoveredRow)
    {
        hoveredRow = newHover;
        repaint();
    }
}

void PresetLibraryPanel::PresetList::mouseExit(const juce::MouseEvent&)
{
    hoveredRow = -1;
    repaint();
}

void PresetLibraryPanel::PresetList::mouseDown(const juce::MouseEvent& e)
{
    const int row = rowAtY(e.y);
    if (row < 0) return;

    const auto& items = owner.activeItems();
    const auto& filtered = owner.filteredIndices;
    if (row >= static_cast<int>(filtered.size())) return;

    const int itemIdx = filtered[static_cast<size_t>(row)];
    if (itemIdx >= static_cast<int>(items.size())) return;

    const auto& item = items[static_cast<size_t>(itemIdx)];

    // Check if click is on the star area
    const float starX = static_cast<float>(getWidth()) - kStarW;
    if (static_cast<float>(e.x) >= starX)
    {
        if (owner.onToggleFavorite)
            owner.onToggleFavorite(item.name, !item.isFavorite);
        return;
    }

    // Right-click context menu for user presets
    if (e.mods.isPopupMenu() && !item.isFactory)
    {
        juce::PopupMenu menu;
        menu.addItem(1, "Rename...");
        menu.addItem(2, "Delete");
        const int presetIdx = item.presetIndex;
        const juce::String presetName = item.name;
        auto safePresetList = juce::Component::SafePointer<PresetList>(this);
        menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this),
            [safePresetList, presetIdx, presetName](int result) {
                if (safePresetList == nullptr) return;
                auto& ownerRef = safePresetList->owner;
                if (result == 1 && ownerRef.onRenamePreset)
                    ownerRef.onRenamePreset(presetIdx, presetName);
                else if (result == 2 && ownerRef.onDeletePreset)
                    ownerRef.onDeletePreset(presetIdx);
            });
        return;
    }

    // Left click — load preset
    if (owner.onPresetSelected)
        owner.onPresetSelected(item.presetIndex);
}

void PresetLibraryPanel::PresetList::updateHeight()
{
    const int rows = static_cast<int>(owner.filteredIndices.size());
    const int h = juce::jmax(rows * kRowH, owner.presetViewport.getHeight());
    setSize(owner.presetViewport.getWidth() - 8, h);
}
