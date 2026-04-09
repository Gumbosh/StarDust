#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include <vector>
#include <map>
#include <set>

struct PresetListItem
{
    juce::String name;
    int presetIndex;
    bool isFactory;
    bool isFavorite = false;
    juce::String bank;  // subfolder name, empty = root USER
};

class PresetLibraryPanel : public juce::Component
{
public:
    PresetLibraryPanel();

    void paint(juce::Graphics& g) override;
    void resized() override;
    bool keyPressed(const juce::KeyPress& key) override;

    void updatePresets(std::vector<PresetListItem> factory,
                       std::map<juce::String, std::vector<PresetListItem>> factoryBanks,
                       std::vector<PresetListItem> user,
                       std::map<juce::String, std::vector<PresetListItem>> bankItems);
    void setCurrentPresetIndex(int index);
    void setFavorites(const std::set<juce::String>& favs);

    std::function<void(int)> onPresetSelected;
    std::function<void()> onClose;
    std::function<void(int, const juce::String&)> onRenamePreset;
    std::function<void(int)> onDeletePreset;
    std::function<void(const juce::String&, bool)> onToggleFavorite;
    std::function<void(const juce::File&)> onImportBank;
    std::function<void(const juce::String&)> onDeleteBank;
    std::function<void(const juce::String&, const juce::String&)> onRenameBank;

    // Factory bank display order (curated, not alphabetical)
    static constexpr int kNumFactoryBanks = 4;
    static inline const juce::String kFactoryBankOrder[kNumFactoryBanks] = {
        "Lo-Fi", "Glitch", "Tape", "Atmosphere"
    };

private:
    // ─── Bank list (left column) ───
    class BankList : public juce::Component
    {
    public:
        BankList(PresetLibraryPanel& owner);
        void paint(juce::Graphics& g) override;
        void mouseMove(const juce::MouseEvent& e) override;
        void mouseExit(const juce::MouseEvent& e) override;
        void mouseDown(const juce::MouseEvent& e) override;

        // Bank index layout:
        // 0 = FAVORITES
        // 1..kNumFactoryBanks = factory sub-banks (Lo-Fi, Glitch, Tape, Atmosphere)
        // kNumFactoryBanks+1 = USER
        // kNumFactoryBanks+2+ = dynamic user banks
        int selectedBank = 1; // default to first factory bank (Lo-Fi)
        int totalBankCount() const;
        static constexpr int kFixedBanks = 1 + kNumFactoryBanks + 1; // FAV + 4 factory + USER = 6

    private:
        PresetLibraryPanel& owner;
        int hoveredBank = -1;
        int bankAtY(int y) const;
        static constexpr int kSeparatorH = 8;
    };

    // ─── Preset list content (right column, inside Viewport) ───
    class PresetList : public juce::Component
    {
    public:
        PresetList(PresetLibraryPanel& owner);
        void paint(juce::Graphics& g) override;
        void mouseMove(const juce::MouseEvent& e) override;
        void mouseExit(const juce::MouseEvent& e) override;
        void mouseDown(const juce::MouseEvent& e) override;
        void updateHeight();

    private:
        PresetLibraryPanel& owner;
        int hoveredRow = -1;
        int rowAtY(int y) const;
        static constexpr int kStarW = 24;
    };

    static constexpr int kHeaderH = 32;
    static constexpr int kSearchH = 28;
    static constexpr int kRowH = 26;
    static constexpr int kBankW = 110;  // slightly wider to fit "ATMOSPHERE"
    static constexpr int kLoadBankBtnH = 26;

    juce::TextButton closeBtn;
    juce::TextEditor searchField;
    BankList bankList { *this };
    juce::Viewport presetViewport;
    PresetList presetList { *this };
    juce::TextButton loadBankBtn;
    std::unique_ptr<juce::FileChooser> fileChooser;

    std::vector<PresetListItem> factoryItems;  // uncategorized factory (Init)
    std::map<juce::String, std::vector<PresetListItem>> factoryBankItemsMap;
    std::vector<PresetListItem> userItems;
    std::vector<PresetListItem> favoriteItems;
    std::map<juce::String, std::vector<PresetListItem>> bankItemsMap;
    std::vector<juce::String> bankNames;
    std::vector<int> filteredIndices;
    juce::String searchQuery;
    int currentPresetIndex = -1;

    const std::vector<PresetListItem>& activeItems() const;
    void applyFilter();
    void rebuildFavorites();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PresetLibraryPanel)
};
