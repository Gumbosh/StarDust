#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

struct StarfieldParams
{
    float destroyFader = 1.0f;  // 0=33RPM, 1=45RPM, 2=x2, 3=78RPM (continuous)
    float grainMix = 0.0f;
    float grainCloud = 0.3f;
    float grainDrift = 0.2f;
    float grainSpace = 0.3f;
    float grainMorph = 0.5f;
    float filterCutoff = 99.0f;
    float destroyIn = 0.0f;
    float destroyOut = 0.0f;
    float destroyMix = 1.0f;
    float chorusMix = 0.0f;
    float chorusSpeed = 1.0f;
    float panOuter = 1.0f;
    float panInner = 0.8f;
    float filterLfo = 0.0f;
    float tapeWow = 0.0f;
    float tapeFlutter = 0.0f;
    float tapeHiss = 0.0f;
    float inputGain = 0.0f;
    float outputGain = 0.0f;
    float masterMix = 1.0f;
    bool tapeEnabled = false;
    bool destroyEnabled = true;
    bool granularEnabled = true;
    bool multiplyEnabled = true;
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

    // Grain visualization — call from editor timer at 30Hz
    struct GrainParticle { float normPos = 0.0f; float phase = 0.0f; bool active = false; };
    static constexpr int kMaxGrainParticles = 32;
    void setGrainData(const std::array<GrainParticle, kMaxGrainParticles>& particles, bool isFrozen);


private:
    StarfieldParams readParams() const;
    void renderStarfield(const StarfieldParams& params, float time);

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
    std::array<GrainParticle, kMaxGrainParticles> grainParticles {};
    bool grainFrozen = false;

    static constexpr int kRenderWidth = 440;
    static constexpr int kRenderHeight = 200;
    static constexpr int kNumShades = 16;
    static constexpr size_t kPixelCount = kRenderWidth * kRenderHeight;

    // Pre-allocated buffers to avoid heap allocation every frame
    std::vector<float> pixelData_ = std::vector<float>(kPixelCount, 0.0f);
    std::vector<float> tempBuffer_ = std::vector<float>(kPixelCount, 0.0f);
};
