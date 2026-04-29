#include "StarfieldBackground.h"
#include <cmath>

StarfieldBackground::StarfieldBackground(juce::AudioProcessorValueTreeState& apvtsRef)
    : apvts(apvtsRef)
{
    startTimerHz(30);
    cachedImage = juce::Image(juce::Image::ARGB, kRenderWidth, kRenderHeight, true);
}

void StarfieldBackground::paint(juce::Graphics& g)
{
    if (cachedImage.isValid())
    {
        auto bounds = getLocalBounds().toFloat();
        const auto lens = bounds.reduced(2.0f);

        juce::Path clip;
        clip.addEllipse(lens);

        juce::Graphics::ScopedSaveState saveState(g);
        g.reduceClipRegion(clip);

        if (!excludeRect.isEmpty())
        {
            juce::RectangleList<int> clipRegion(getLocalBounds());
            clipRegion.subtract(excludeRect);
            g.reduceClipRegion(clipRegion);
        }

        g.setImageResamplingQuality(juce::Graphics::lowResamplingQuality);
        g.drawImage(cachedImage, bounds);

        juce::ColourGradient vignette(juce::Colours::transparentBlack,
                                      bounds.getCentreX(), bounds.getCentreY(),
                                      juce::Colours::black.withAlpha(0.62f),
                                      bounds.getRight(), bounds.getBottom(),
                                      true);
        g.setGradientFill(vignette);
        g.fillEllipse(lens);
    }
}

void StarfieldBackground::resized() {}

void StarfieldBackground::timerCallback()
{
    const float dt = 1.0f / 30.0f;
    const auto params = readParams();
    timeCounter += dt;

    renderStarfield(params, timeCounter);
    repaint();
}

StarfieldParams StarfieldBackground::readParams() const
{
    StarfieldParams p;
    const float characterAmount = juce::jlimit(0.0f, 1.0f,
        apvts.getRawParameterValue("characterAmount")->load(std::memory_order_relaxed));
    const bool characterActive = characterAmount > 0.001f;
    p.characterAmount = characterAmount;
    p.characterMode = juce::jlimit(0, 5,
        juce::roundToInt(apvts.getRawParameterValue("characterMode")->load(std::memory_order_relaxed)));

    p.gritMix = *apvts.getRawParameterValue("destroyMix");
    p.exciterMix = *apvts.getRawParameterValue("exciterMix");

    if (characterActive)
    {
        p.gritMix = juce::jmax(p.gritMix, characterAmount * 0.75f);
        p.exciterMix = juce::jmax(p.exciterMix, characterAmount * 0.65f);
    }

    p.gritDriveDb = *apvts.getRawParameterValue("destroyIn");
    p.gritRateHz = *apvts.getRawParameterValue("destroyFader");
    p.gritBits = *apvts.getRawParameterValue("destroyBits");
    p.gritJitter = *apvts.getRawParameterValue("destroyJitter");
    p.exciterDrive = *apvts.getRawParameterValue("exciterDrive");
    p.exciterToneHz = *apvts.getRawParameterValue("exciterTone");

    return p;
}

float StarfieldBackground::hash(float x, float y)
{
    float h = x * 127.1f + y * 311.7f;
    h = std::sin(h) * 43758.5453f;
    return h - std::floor(h);
}

// ---------------------------------------------------------------------------
// Visual layers
// ---------------------------------------------------------------------------

void StarfieldBackground::applyGrit(float mix, float time)
{
    (void)time;
    // Pixelation: block size grows with mix (1 at 0%, 5 at 100%)
    const int blockSize = std::max(1, static_cast<int>(1.0f + mix * 4.0f));

    if (blockSize > 1)
    {
        for (int by = 0; by < kRenderHeight; by += blockSize)
            for (int bx = 0; bx < kRenderWidth; bx += blockSize)
            {
                float sum = 0.0f; int count = 0;
                for (int dy = 0; dy < blockSize && (by + dy) < kRenderHeight; ++dy)
                    for (int dx = 0; dx < blockSize && (bx + dx) < kRenderWidth; ++dx)
                    {
                        sum += pixelData_[static_cast<size_t>((by + dy) * kRenderWidth + (bx + dx))];
                        ++count;
                    }
                const float avg = sum / static_cast<float>(count);
                for (int dy = 0; dy < blockSize && (by + dy) < kRenderHeight; ++dy)
                    for (int dx = 0; dx < blockSize && (bx + dx) < kRenderWidth; ++dx)
                        pixelData_[static_cast<size_t>((by + dy) * kRenderWidth + (bx + dx))] = avg;
            }
    }

    // Posterization: fewer levels at higher mix
    const float levels = 16.0f - mix * 10.0f; // 16 levels (0%) → 6 levels (100%)
    const float strength = mix * 0.45f;
    const float invLevels = 1.0f / levels;
    for (auto& px : pixelData_)
    {
        const float posterized = std::floor(px * levels) * invLevels;
        px = px + (posterized - px) * strength;
    }
}

void StarfieldBackground::applyExciter(float mix, float time)
{
    const float sparkle = juce::jlimit(0.0f, 1.0f, mix);
    if (sparkle < 0.01f)
        return;

    for (int y = 0; y < kRenderHeight; ++y)
    {
        for (int x = 0; x < kRenderWidth; ++x)
        {
            const float nx = (static_cast<float>(x) / static_cast<float>(kRenderWidth) - 0.5f) * 2.0f;
            const float ny = (static_cast<float>(y) / static_cast<float>(kRenderHeight) - 0.5f) * 2.0f;
            const float coreMask = std::exp(-(nx * nx * 2.8f + ny * ny * 1.6f));
            const float shimmer = 0.65f + 0.35f * std::sin(time * 2.5f + nx * 12.0f + ny * 5.0f);
            auto& pixel = pixelData_[static_cast<size_t>(y * kRenderWidth + x)];
            pixel = std::min(1.0f, pixel + sparkle * coreMask * shimmer * 0.20f);
        }
    }

    for (int y = 0; y < kRenderHeight; ++y)
    {
        for (int x = 0; x < kRenderWidth; ++x)
        {
            const float nx = (static_cast<float>(x) / static_cast<float>(kRenderWidth) - 0.5f) * 2.0f;
            const float ny = (static_cast<float>(y) / static_cast<float>(kRenderHeight) - 0.5f) * 2.0f;
            const float coreMask = std::exp(-(nx * nx * 2.4f + ny * ny * 1.4f));
            auto& px = pixelData_[static_cast<size_t>(y * kRenderWidth + x)];
            px = px + (std::min(1.0f, std::pow(px, 1.0f - sparkle * 0.10f)) - px) * coreMask;
        }
    }
}

// ---------------------------------------------------------------------------
// Main render
// ---------------------------------------------------------------------------

void StarfieldBackground::renderStarfield(const StarfieldParams& params, float time)
{
    auto& img = cachedImage;

    const float fw = static_cast<float>(kRenderWidth);
    const float fh = static_cast<float>(kRenderHeight);
    const float cx = fw * 0.5f;
    const float cy = fh * 0.5f;
    const float maxDist = std::sqrt(cx * cx + cy * cy);
    const float gritDriveNorm = juce::jlimit(0.0f, 1.0f, params.gritDriveDb / 24.0f);
    const float gritRateNorm = juce::jlimit(0.0f, 1.0f,
        (std::log(std::max(250.0f, params.gritRateHz)) - std::log(250.0f))
        / (std::log(96000.0f) - std::log(250.0f)));
    const float gritBitNorm = juce::jlimit(0.0f, 1.0f, (24.0f - params.gritBits) / 20.0f);
    const float gritJitterNorm = juce::jlimit(0.0f, 1.0f, params.gritJitter);
    const float exciterDriveNorm = juce::jlimit(0.0f, 1.0f, params.exciterDrive);
    const float exciterToneNorm = juce::jlimit(0.0f, 1.0f,
        (std::log(std::max(800.0f, params.exciterToneHz)) - std::log(800.0f))
        / (std::log(16000.0f) - std::log(800.0f)));

    // Galaxy parameters
    const float rotationSpeed = 0.18f + 0.42f * params.characterAmount
                              + 0.05f * static_cast<float>(params.characterMode);
    const float spiralTightness = 0.022f + 0.020f * (1.0f - gritRateNorm);
    const float coreRadius = 24.0f + 20.0f * exciterToneNorm;
    const float coreBrightness = 0.62f + 0.30f * params.characterAmount + 0.22f * exciterDriveNorm;
    const float armBrightness = 0.14f + 0.22f * gritDriveNorm + 0.10f * gritBitNorm;
    const float modePhase = static_cast<float>(params.characterMode) * 0.73f;

    auto& pixelData = pixelData_;
    std::fill(pixelData.begin(), pixelData.end(), 0.0f);

    // ---- Base Galaxy: Core + Spiral Arms (always visible) ----
    for (int y = 0; y < kRenderHeight; ++y)
    {
        for (int x = 0; x < kRenderWidth; ++x)
        {
            const float fx = static_cast<float>(x) - cx;
            const float fy = static_cast<float>(y) - cy;

            const float dist = std::sqrt(fx * fx + fy * fy);
            const float angle = std::atan2(fy, fx);

            const float coreGlow = coreBrightness * std::exp(-(dist * dist) / (2.0f * coreRadius * coreRadius));

            const float spiralAngle = angle - dist * spiralTightness + time * rotationSpeed + modePhase;
            float armValue = std::cos(spiralAngle * (2.0f + static_cast<float>(params.characterMode % 3)));
            armValue = (armValue + 1.0f) * 0.5f;

            const float armWidth = 0.24f + dist * (0.006f + gritJitterNorm * 0.010f);
            armValue = std::pow(armValue, 1.0f / (armWidth + 0.01f));

            const float armFalloff = std::exp(-dist * 0.012f);
            const float armGlow = armValue * armBrightness * armFalloff;

            const float dustLane = 1.0f - (1.0f - armValue) * (0.22f + gritBitNorm * 0.36f) * armFalloff;

            float pixel = coreGlow + std::max(0.0f, armGlow);
            pixel *= dustLane;
            pixel += 0.006f + params.characterAmount * 0.008f;

            if (gritJitterNorm > 0.001f)
            {
                const float n = hash(static_cast<float>(x) * 0.31f + time * 3.1f,
                                     static_cast<float>(y) * 0.47f + modePhase);
                pixel += (n - 0.5f) * gritJitterNorm * 0.08f;
            }

            pixelData[static_cast<size_t>(y * kRenderWidth + x)] = pixel;
        }
    }

    if (params.gritMix >= 0.01f)
        applyGrit(params.gritMix, time);
    if (params.exciterMix >= 0.01f)
        applyExciter(params.exciterMix, time);

    // ---- Bayer dithering + scanlines + vignette ----
    juce::Image::BitmapData bitmap(img, juce::Image::BitmapData::writeOnly);

    constexpr int scanlineSpacing = 4;
    constexpr float scanlineDarken = 0.75f;

    for (int y = 0; y < kRenderHeight; ++y)
    {
        const float scanlineMul = (y % scanlineSpacing == 0) ? scanlineDarken : 1.0f;

        for (int x = 0; x < kRenderWidth; ++x)
        {
            float value = juce::jlimit(0.0f, 1.0f,
                pixelData[static_cast<size_t>(y * kRenderWidth + x)]);

            // Vignette
            const float vdx = static_cast<float>(x) - cx;
            const float vdy = static_cast<float>(y) - cy;
            const float vdist = std::sqrt(vdx * vdx + vdy * vdy);
            const float edgeNorm = vdist / maxDist;
            const float vignette = 1.0f - 0.55f * edgeNorm * edgeNorm;
            value *= vignette;

            value *= scanlineMul;

            const float threshold = kBayerMatrix[y % kBayerSize][x % kBayerSize];
            const float scaled = value * static_cast<float>(kNumShades - 1);
            const float dithered = (scaled - std::floor(scaled)) > threshold
                ? std::ceil(scaled) : std::floor(scaled);

            const auto gray = static_cast<uint8_t>(
                juce::jlimit(0.0f, 255.0f, dithered * (255.0f / static_cast<float>(kNumShades - 1))));

            bitmap.setPixelColour(x, y, juce::Colour(gray, gray, gray));
        }
    }
}
