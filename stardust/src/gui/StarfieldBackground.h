#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <array>

struct StarfieldParams
{
    float bitDepth = 12.0f;
    float sampleRate = 26040.0f;
    float grain = 0.0f;
    float grainDensity = 4.0f;
    float grainSize = 30.0f;
    float grainScatter = 0.2f;
    float stereoWidth = 0.0f;
    float filterCutoff = 99.0f;
    float drive = 0.0f;
    float grainTune = 0.0f;
    float mix = 1.0f;
    float outputLevel = 0.0f;
};

struct Particle
{
    float x = 0.0f;
    float y = 0.0f;
    float vx = 0.0f;
    float vy = 0.0f;
    float life = 0.0f;
    float brightness = 0.0f;
    static constexpr int kTrailLen = 6;
    float trailX[kTrailLen] = {};
    float trailY[kTrailLen] = {};
    int trailCount = 0;
};

class StarfieldBackground : public juce::Component, public juce::Timer
{
public:
    StarfieldBackground(juce::AudioProcessorValueTreeState& apvts,
                        std::atomic<float>& outputLevelL,
                        std::atomic<float>& outputLevelR);

    void paint(juce::Graphics& g) override;
    void resized() override;
    void timerCallback() override;

    void setExcludeRect(juce::Rectangle<int> rect) { excludeRect = rect; }

private:
    StarfieldParams readParams() const;
    juce::Image renderStarfield(const StarfieldParams& params, float time);

    void spawnParticles(int count);
    void updateParticles(float dt);

    static float hash(float x, float y);

    static constexpr int kBayerSize = 4;
    static constexpr float kBayerMatrix[kBayerSize][kBayerSize] = {
        {  0.0f / 16.0f,  8.0f / 16.0f,  2.0f / 16.0f, 10.0f / 16.0f },
        { 12.0f / 16.0f,  4.0f / 16.0f, 14.0f / 16.0f,  6.0f / 16.0f },
        {  3.0f / 16.0f, 11.0f / 16.0f,  1.0f / 16.0f,  9.0f / 16.0f },
        { 15.0f / 16.0f,  7.0f / 16.0f, 13.0f / 16.0f,  5.0f / 16.0f }
    };

    juce::AudioProcessorValueTreeState& apvts;
    std::atomic<float>& outputLevelL;
    std::atomic<float>& outputLevelR;
    juce::Image cachedImage;
    juce::Rectangle<int> excludeRect;
    float timeCounter = 0.0f;
    float smoothedLevel = 0.0f;
    float previousLevel = 0.0f;

    // Particle system
    static constexpr int kMaxParticles = 30;
    std::array<Particle, kMaxParticles> particles {};
    juce::Random rng;

    static constexpr int kRenderWidth = 260;
    static constexpr int kRenderHeight = 190;
    static constexpr int kNumShades = 16;
};
