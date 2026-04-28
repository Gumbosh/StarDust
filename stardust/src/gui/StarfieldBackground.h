#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

struct StarfieldParams
{
    struct Slot
    {
        int effectId = 0;    // 0=empty, 1=GRIT, 2=EXCITER, 3=JU-60, etc.
        float mix    = 0.0f; // 0..1 wet/dry
        bool enabled = false;
    };
    Slot slots[4];

    float characterAmount = 0.0f;
    int characterMode = 0;

    float gritDriveDb = 0.0f;
    float gritRateHz = 22050.0f;
    float gritBits = 12.0f;
    float gritJitter = 0.0f;
    float exciterDrive = 0.0f;
    float exciterToneHz = 6000.0f;
};

class StarfieldBackground : public juce::Component, public juce::Timer
{
public:
    StarfieldBackground(juce::AudioProcessorValueTreeState& apvts);

    void paint(juce::Graphics& g) override;
    void resized() override;
    void timerCallback() override;

    void setExcludeRect(juce::Rectangle<int> rect) { excludeRect = rect; }

private:
    StarfieldParams readParams() const;
    void renderStarfield(const StarfieldParams& params, float time);

    // Per-effect visual layers (each takes pixelData, mix intensity, and time)
    void applyGrit       (float mix, float time);
    void applyExciter    (float mix, float time);
    void applyJu60       (float mix, float time);
    void applyOxide      (float mix, float time);
    void applyDist       (float mix, float time);
    void applyVoid       (float mix, float time);
    void applyHaze       (float mix, float time);
    void applyMultiply   (float mix, float time);
    void applyStutter    (float mix, float time);
    void applyShift      (float mix, float time);
    void applyReverser   (float mix, float time);

    static float hash(float x, float y);

    static constexpr int kBayerSize = 4;
    static constexpr float kBayerMatrix[kBayerSize][kBayerSize] = {
        {  0.0f / 16.0f,  8.0f / 16.0f,  2.0f / 16.0f, 10.0f / 16.0f },
        { 12.0f / 16.0f,  4.0f / 16.0f, 14.0f / 16.0f,  6.0f / 16.0f },
        {  3.0f / 16.0f, 11.0f / 16.0f,  1.0f / 16.0f,  9.0f / 16.0f },
        { 15.0f / 16.0f,  7.0f / 16.0f, 13.0f / 16.0f,  5.0f / 16.0f }
    };

    juce::AudioProcessorValueTreeState& apvts;
    juce::Image cachedImage;
    juce::Rectangle<int> excludeRect;
    float timeCounter = 0.0f;

    static constexpr int kRenderWidth = 440;
    static constexpr int kRenderHeight = 200;
    static constexpr int kNumShades = 16;
    static constexpr size_t kPixelCount = kRenderWidth * kRenderHeight;

    // Pre-allocated buffers to avoid heap allocation every frame
    std::vector<float> pixelData_ = std::vector<float>(kPixelCount, 0.0f);
    std::vector<float> tempBuffer_ = std::vector<float>(kPixelCount, 0.0f);
};
