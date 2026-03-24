#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

struct StarfieldParams
{
    float bitDepth = 12.0f;
    float grain = 0.0f;
    float stereoWidth = 0.0f;
    float filterCutoff = 99.0f;
    float drive = 0.0f;
    float grainTune = 0.0f;
};

class StarfieldBackground : public juce::Component, public juce::Timer
{
public:
    explicit StarfieldBackground(juce::AudioProcessorValueTreeState& apvts);

    void paint(juce::Graphics& g) override;
    void resized() override;
    void timerCallback() override;

private:
    StarfieldParams readParams() const;
    juce::Image renderStarfield(const StarfieldParams& params, float time) const;

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
    float timeCounter = 0.0f;

    static constexpr int kRenderWidth = 260;
    static constexpr int kRenderHeight = 190;
    static constexpr int kNumShades = 16;
};
