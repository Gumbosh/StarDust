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

    p.slots[0].effectId = 1;
    p.slots[0].mix = *apvts.getRawParameterValue("destroyMix");
    p.slots[0].enabled = p.slots[0].mix > 0.001f;

    p.slots[1].effectId = 2;
    p.slots[1].mix = *apvts.getRawParameterValue("exciterMix");
    p.slots[1].enabled = p.slots[1].mix > 0.001f;

    if (characterActive)
    {
        p.slots[0].mix = juce::jmax(p.slots[0].mix, characterAmount * 0.75f);
        p.slots[0].enabled = true;

        p.slots[1].mix = juce::jmax(p.slots[1].mix, characterAmount * 0.65f);
        p.slots[1].enabled = true;
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
// Per-effect visual layers
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

void StarfieldBackground::applyJu60(float mix, float time)
{
    // Horizontal shimmer: slow sinusoidal row shifts (chorus wobble)
    const float depth = mix * 6.0f; // 0-6 pixel shift
    if (depth < 0.5f) return;

    auto& temp = tempBuffer_;
    std::copy(pixelData_.begin(), pixelData_.end(), temp.begin());

    for (int y = 0; y < kRenderHeight; ++y)
    {
        const float rowPhase = static_cast<float>(y) * 0.05f;
        const float shift = std::sin(time * 1.2f + rowPhase) * depth
                          + std::sin(time * 0.7f + rowPhase * 1.7f) * depth * 0.3f;
        const int iShift = static_cast<int>(shift);
        const float frac = shift - static_cast<float>(iShift);

        for (int x = 0; x < kRenderWidth; ++x)
        {
            const int src0 = x - iShift;
            const int src1 = src0 - 1;
            const float v0 = (src0 >= 0 && src0 < kRenderWidth)
                ? temp[static_cast<size_t>(y * kRenderWidth + src0)] : 0.0f;
            const float v1 = (src1 >= 0 && src1 < kRenderWidth)
                ? temp[static_cast<size_t>(y * kRenderWidth + src1)] : 0.0f;
            pixelData_[static_cast<size_t>(y * kRenderWidth + x)] = v0 * (1.0f - frac) + v1 * frac;
        }
    }
}

void StarfieldBackground::applyOxide(float mix, float time)
{
    // Wow: slow sinusoidal horizontal drift
    const float wowDepth = mix * 8.0f;
    if (wowDepth > 0.5f)
    {
        auto& temp = tempBuffer_;
        std::copy(pixelData_.begin(), pixelData_.end(), temp.begin());

        for (int y = 0; y < kRenderHeight; ++y)
        {
            const float rowPhase = static_cast<float>(y) * 0.03f;
            const float shift = std::sin(time * 0.8f + rowPhase) * wowDepth;
            const int iShift = static_cast<int>(shift);
            const float frac = shift - static_cast<float>(iShift);

            for (int x = 0; x < kRenderWidth; ++x)
            {
                const int src0 = x - iShift;
                const int src1 = src0 - 1;
                const float v0 = (src0 >= 0 && src0 < kRenderWidth)
                    ? temp[static_cast<size_t>(y * kRenderWidth + src0)] : 0.0f;
                const float v1 = (src1 >= 0 && src1 < kRenderWidth)
                    ? temp[static_cast<size_t>(y * kRenderWidth + src1)] : 0.0f;
                pixelData_[static_cast<size_t>(y * kRenderWidth + x)] = v0 * (1.0f - frac) + v1 * frac;
            }
        }
    }

    // Hiss: animated noise particles
    const float hissIntensity = mix * 0.20f;
    const float hissDensity = mix * 0.12f;
    if (hissDensity > 0.01f)
    {
        for (int y = 0; y < kRenderHeight; ++y)
            for (int x = 0; x < kRenderWidth; ++x)
            {
                const float n = hash(static_cast<float>(x) * 1.7f + time * 1.3f,
                                     static_cast<float>(y) * 2.3f + time * 0.9f);
                if (n < hissDensity)
                {
                    auto& pixel = pixelData_[static_cast<size_t>(y * kRenderWidth + x)];
                    pixel = std::min(1.0f, pixel + n * hissIntensity / hissDensity);
                }
            }
    }

    // Soft-clip compression
    if (mix > 0.1f)
    {
        const float driveGain = 1.0f + mix * 2.5f;
        const float driveScale = std::tanh(driveGain);
        for (auto& px : pixelData_)
            px = std::tanh(px * driveGain) / driveScale;
    }
}

void StarfieldBackground::applyDist(float mix, float time)
{
    (void)time;
    // Contrast boost: steep S-curve
    const float contrast = 1.0f + mix * 3.0f;
    for (auto& px : pixelData_)
    {
        const float centered = px - 0.5f;
        px = juce::jlimit(0.0f, 1.0f, 0.5f + centered * contrast);
    }

    // Bloom: bright pixels spread to neighbors
    if (mix > 0.2f)
    {
        const float bloomStr = mix * 0.3f;
        auto& src = tempBuffer_;
        std::copy(pixelData_.begin(), pixelData_.end(), src.begin());
        constexpr float kThreshold = 0.5f;
        for (int y = 1; y < kRenderHeight - 1; ++y)
            for (int x = 1; x < kRenderWidth - 1; ++x)
            {
                const float excess = std::max(0.0f,
                    src[static_cast<size_t>(y * kRenderWidth + x)] - kThreshold);
                if (excess < 0.005f) continue;
                const float spread = excess * bloomStr;
                for (int dy = -1; dy <= 1; ++dy)
                    for (int dx = -1; dx <= 1; ++dx)
                        if (dx != 0 || dy != 0)
                        {
                            auto& p = pixelData_[static_cast<size_t>((y + dy) * kRenderWidth + (x + dx))];
                            p = std::min(1.0f, p + spread * (dx == 0 || dy == 0 ? 0.5f : 0.35f));
                        }
            }
    }
}

void StarfieldBackground::applyVoid(float mix, float time)
{
    (void)time;
    // Multi-pass box blur (foggy/spacious)
    const int passes = 1 + static_cast<int>(mix * 2.0f);  // 1-3 passes
    const int radius = 2 + static_cast<int>(mix * 4.0f);  // 2-6 px
    auto& temp = tempBuffer_;

    for (int pass = 0; pass < passes; ++pass)
    {
        // Horizontal
        for (int y = 0; y < kRenderHeight; ++y)
            for (int x = 0; x < kRenderWidth; ++x)
            {
                float sum = 0.0f; int count = 0;
                for (int dx = -radius; dx <= radius; ++dx)
                {
                    const int nx = x + dx;
                    if (nx >= 0 && nx < kRenderWidth)
                    {
                        sum += pixelData_[static_cast<size_t>(y * kRenderWidth + nx)];
                        ++count;
                    }
                }
                temp[static_cast<size_t>(y * kRenderWidth + x)] = sum / static_cast<float>(count);
            }

        // Vertical
        for (int y = 0; y < kRenderHeight; ++y)
            for (int x = 0; x < kRenderWidth; ++x)
            {
                float sum = 0.0f; int count = 0;
                for (int dy = -radius; dy <= radius; ++dy)
                {
                    const int ny = y + dy;
                    if (ny >= 0 && ny < kRenderHeight)
                    {
                        sum += temp[static_cast<size_t>(ny * kRenderWidth + x)];
                        ++count;
                    }
                }
                pixelData_[static_cast<size_t>(y * kRenderWidth + x)] = sum / static_cast<float>(count);
            }
    }

    // Radial fade-to-center: edges darken more with higher mix
    const float cx = static_cast<float>(kRenderWidth) * 0.5f;
    const float cy = static_cast<float>(kRenderHeight) * 0.5f;
    const float maxDist = std::sqrt(cx * cx + cy * cy);
    const float fadeStr = mix * 0.4f;
    for (int y = 0; y < kRenderHeight; ++y)
        for (int x = 0; x < kRenderWidth; ++x)
        {
            const float dx = static_cast<float>(x) - cx;
            const float dy = static_cast<float>(y) - cy;
            const float dist = std::sqrt(dx * dx + dy * dy) / maxDist;
            auto& px = pixelData_[static_cast<size_t>(y * kRenderWidth + x)];
            px *= 1.0f - fadeStr * dist * dist;
        }
}

void StarfieldBackground::applyHaze(float mix, float time)
{
    // Animated noise overlay (additive)
    const float intensity = mix * 0.25f;
    const float density = mix * 0.18f;
    for (int y = 0; y < kRenderHeight; ++y)
        for (int x = 0; x < kRenderWidth; ++x)
        {
            const float n = hash(static_cast<float>(x) * 2.1f + time * 0.7f,
                                 static_cast<float>(y) * 1.9f + time * 1.1f);
            if (n < density)
            {
                auto& pixel = pixelData_[static_cast<size_t>(y * kRenderWidth + x)];
                pixel = std::min(1.0f, pixel + n * intensity / density);
            }
        }
}

void StarfieldBackground::applyMultiply(float mix, float time)
{
    // Kaleidoscope: wedge-fold mirror
    const int segments = 2 + static_cast<int>(mix * 6.0f);
    const float wedgeAngle = juce::MathConstants<float>::twoPi / static_cast<float>(segments);
    const float rotation = time * 0.3f;

    const float cx = static_cast<float>(kRenderWidth) * 0.5f;
    const float cy = static_cast<float>(kRenderHeight) * 0.5f;

    auto& source = tempBuffer_;
    std::copy(pixelData_.begin(), pixelData_.end(), source.begin());
    std::fill(pixelData_.begin(), pixelData_.end(), 0.0f);

    for (int y = 0; y < kRenderHeight; ++y)
        for (int x = 0; x < kRenderWidth; ++x)
        {
            const float dx = static_cast<float>(x) - cx;
            const float dy = static_cast<float>(y) - cy;
            const float dist = std::sqrt(dx * dx + dy * dy);
            float angle = std::atan2(dy, dx) + rotation;

            angle = std::fmod(angle + juce::MathConstants<float>::twoPi, wedgeAngle);
            if (angle > wedgeAngle * 0.5f)
                angle = wedgeAngle - angle;

            const float srcXf = cx + std::cos(angle) * dist;
            const float srcYf = cy + std::sin(angle) * dist;

            const float clampedX = juce::jlimit(0.0f, static_cast<float>(kRenderWidth - 2), srcXf);
            const float clampedY = juce::jlimit(0.0f, static_cast<float>(kRenderHeight - 2), srcYf);
            const int x0 = static_cast<int>(clampedX);
            const int y0 = static_cast<int>(clampedY);
            const float fx = clampedX - static_cast<float>(x0);
            const float fy = clampedY - static_cast<float>(y0);

            const float val =
                source[static_cast<size_t>(y0 * kRenderWidth + x0)] * (1.0f - fx) * (1.0f - fy)
              + source[static_cast<size_t>(y0 * kRenderWidth + (x0 + 1))] * fx * (1.0f - fy)
              + source[static_cast<size_t>((y0 + 1) * kRenderWidth + x0)] * (1.0f - fx) * fy
              + source[static_cast<size_t>((y0 + 1) * kRenderWidth + (x0 + 1))] * fx * fy;

            pixelData_[static_cast<size_t>(y * kRenderWidth + x)] = val;
        }

    // Crossfade original ↔ kaleidoscope by mix
    for (size_t i = 0; i < pixelData_.size(); ++i)
        pixelData_[i] = source[i] * (1.0f - mix) + pixelData_[i] * mix;
}

void StarfieldBackground::applyStutter(float mix, float time)
{
    // Horizontal slice repeat: random rows get duplicated (glitch stripes)
    const float stripeProb = mix * 0.25f;
    const int maxRepeatH = 2 + static_cast<int>(mix * 6.0f);

    auto& temp = tempBuffer_;
    std::copy(pixelData_.begin(), pixelData_.end(), temp.begin());

    int y = 0;
    while (y < kRenderHeight)
    {
        const float r = hash(static_cast<float>(y) * 3.7f + time * 0.4f, time * 1.1f);
        if (r < stripeProb && y > 0)
        {
            const int repeatH = 1 + static_cast<int>(r / stripeProb * static_cast<float>(maxRepeatH));
            const int srcRow = y - 1;
            for (int dy = 0; dy < repeatH && (y + dy) < kRenderHeight; ++dy)
                for (int x = 0; x < kRenderWidth; ++x)
                    pixelData_[static_cast<size_t>((y + dy) * kRenderWidth + x)] =
                        temp[static_cast<size_t>(srcRow * kRenderWidth + x)];
            y += repeatH;
        }
        else
        {
            ++y;
        }
    }
}

void StarfieldBackground::applyShift(float mix, float time)
{
    // Vertical offset bands: columns shifted up/down in bands
    const int bandWidth = 8 + static_cast<int>((1.0f - mix) * 24.0f);
    const float maxShift = mix * 15.0f;

    auto& temp = tempBuffer_;
    std::copy(pixelData_.begin(), pixelData_.end(), temp.begin());

    for (int bx = 0; bx < kRenderWidth; bx += bandWidth)
    {
        const float r = hash(static_cast<float>(bx) * 2.3f + time * 0.2f, time * 0.8f);
        const int shift = static_cast<int>((r - 0.5f) * 2.0f * maxShift);

        for (int x = bx; x < bx + bandWidth && x < kRenderWidth; ++x)
            for (int y = 0; y < kRenderHeight; ++y)
            {
                const int srcY = y - shift;
                pixelData_[static_cast<size_t>(y * kRenderWidth + x)] =
                    (srcY >= 0 && srcY < kRenderHeight)
                        ? temp[static_cast<size_t>(srcY * kRenderWidth + x)] : 0.0f;
            }
    }
}

void StarfieldBackground::applyReverser(float mix, float time)
{
    (void)time;
    // Horizontal flip blend: crossfade between normal and mirrored
    auto& temp = tempBuffer_;
    std::copy(pixelData_.begin(), pixelData_.end(), temp.begin());

    for (int y = 0; y < kRenderHeight; ++y)
        for (int x = 0; x < kRenderWidth; ++x)
        {
            const int mirrorX = kRenderWidth - 1 - x;
            const float original = temp[static_cast<size_t>(y * kRenderWidth + x)];
            const float mirrored = temp[static_cast<size_t>(y * kRenderWidth + mirrorX)];
            pixelData_[static_cast<size_t>(y * kRenderWidth + x)] =
                original * (1.0f - mix) + mirrored * mix;
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

    // ---- Apply per-effect visual layers in slot order ----
    for (int i = 0; i < 4; ++i)
    {
        const auto& slot = params.slots[i];
        if (slot.effectId == 0 || !slot.enabled || slot.mix < 0.01f)
            continue;

        switch (slot.effectId)
        {
            case 1:  applyGrit(slot.mix, time);     break;
            case 2:  applyExciter(slot.mix, time);  break;
            case 3:  applyJu60(slot.mix, time);      break;
            case 4:  applyOxide(slot.mix, time);     break;
            case 5:  applyDist(slot.mix, time);      break;
            case 6:  applyVoid(slot.mix, time);      break;
            case 7:  applyHaze(slot.mix, time);      break;
            case 8:  applyMultiply(slot.mix, time);  break;
            case 10: applyStutter(slot.mix, time);   break;
            case 11: applyShift(slot.mix, time);     break;
            case 12: applyReverser(slot.mix, time);  break;
            default: break;
        }
    }

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
