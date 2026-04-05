#include "StarfieldBackground.h"
#include <cmath>

StarfieldBackground::StarfieldBackground(juce::AudioProcessorValueTreeState& apvtsRef,
                                           std::atomic<float>& outL,
                                           std::atomic<float>& outR)
    : apvts(apvtsRef), outputLevelL(outL), outputLevelR(outR)
{
    startTimerHz(30);
    cachedImage = juce::Image(juce::Image::ARGB, kRenderWidth, kRenderHeight, true);
}

void StarfieldBackground::paint(juce::Graphics& g)
{
    if (cachedImage.isValid())
    {
        if (!excludeRect.isEmpty())
        {
            juce::RectangleList<int> clipRegion(getLocalBounds());
            clipRegion.subtract(excludeRect);
            g.reduceClipRegion(clipRegion);
        }

        g.setImageResamplingQuality(juce::Graphics::lowResamplingQuality);
        g.drawImage(cachedImage, getLocalBounds().toFloat());
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

// ---------------------------------------------------------------------------
// Mix and enabled parameter lookup helpers
// ---------------------------------------------------------------------------

static const char* mixParamForEffect(int id)
{
    switch (id)
    {
        case 1:  return "destroyMix";
        case 3:  return "chorusMix";
        case 4:  return "tapeMix";
        case 5:  return "distortionMix";
        case 6:  return "reverbMix";
        case 7:  return "hazeMix";
        case 8:  return "unisonMix";
        case 9:  return "grainMix";
        case 10: return "stutterMix";
        case 11: return "shiftMix";
        case 12: return "reverserMix";
        default: return nullptr;
    }
}

static const char* enabledParamForEffect(int id)
{
    switch (id)
    {
        case 1:  return "destroyEnabled";
        case 3:  return "multiplyEnabled";
        case 4:  return "tapeEnabled";
        case 5:  return "distortionEnabled";
        case 6:  return "reverbEnabled";
        case 7:  return "hazeEnabled";
        case 8:  return "unisonEnabled";
        case 9:  return "grainEnabled";
        case 10: return "stutterEnabled";
        case 11: return "shiftEnabled";
        case 12: return "reverserEnabled";
        default: return nullptr;
    }
}

StarfieldParams StarfieldBackground::readParams() const
{
    StarfieldParams p;

    for (int i = 0; i < 4; ++i)
    {
        const int slotId = static_cast<int>(
            *apvts.getRawParameterValue("chainSlot" + juce::String(i)));

        p.slots[i].effectId = slotId;

        if (slotId == 0) continue;

        if (const auto* mixId = mixParamForEffect(slotId))
            p.slots[i].mix = *apvts.getRawParameterValue(mixId);

        if (const auto* enId = enabledParamForEffect(slotId))
            p.slots[i].enabled = *apvts.getRawParameterValue(enId) >= 0.5f;
    }

    p.inputGain  = *apvts.getRawParameterValue("inputGain");
    p.outputGain = *apvts.getRawParameterValue("outputGain");
    p.masterMix  = *apvts.getRawParameterValue("masterMix");

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

void StarfieldBackground::applyGrain(float mix, float time)
{
    // Scatter blocks: random rectangular regions displaced
    const int blockW = 8 + static_cast<int>((1.0f - mix) * 16.0f); // smaller blocks at high mix
    const int blockH = 4 + static_cast<int>((1.0f - mix) * 8.0f);
    const float displaceMax = mix * 20.0f;
    const float probability = mix * 0.3f;

    auto& temp = tempBuffer_;
    std::copy(pixelData_.begin(), pixelData_.end(), temp.begin());

    for (int by = 0; by < kRenderHeight; by += blockH)
        for (int bx = 0; bx < kRenderWidth; bx += blockW)
        {
            const float r = hash(static_cast<float>(bx) * 0.7f + time * 0.5f,
                                 static_cast<float>(by) * 1.3f + time * 0.3f);
            if (r > probability) continue;

            const int offX = static_cast<int>((hash(static_cast<float>(bx + 1), static_cast<float>(by) + time) - 0.5f) * 2.0f * displaceMax);
            const int offY = static_cast<int>((hash(static_cast<float>(bx), static_cast<float>(by + 1) + time) - 0.5f) * 2.0f * displaceMax);

            for (int dy = 0; dy < blockH && (by + dy) < kRenderHeight; ++dy)
                for (int dx = 0; dx < blockW && (bx + dx) < kRenderWidth; ++dx)
                {
                    const int srcX = bx + dx + offX;
                    const int srcY = by + dy + offY;
                    if (srcX >= 0 && srcX < kRenderWidth && srcY >= 0 && srcY < kRenderHeight)
                        pixelData_[static_cast<size_t>((by + dy) * kRenderWidth + (bx + dx))] =
                            temp[static_cast<size_t>(srcY * kRenderWidth + srcX)];
                }
        }
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

    // Galaxy parameters
    const float rotationSpeed = 0.3f;
    const float spiralTightness = 0.03f;
    const float coreRadius = 30.0f;
    const float coreBrightness = 0.8f;
    const float armBrightness = 0.20f;

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

            const float spiralAngle = angle - dist * spiralTightness + time * rotationSpeed;
            float armValue = std::cos(spiralAngle * 2.0f);
            armValue = (armValue + 1.0f) * 0.5f;

            const float armWidth = 0.3f + dist * 0.008f;
            armValue = std::pow(armValue, 1.0f / (armWidth + 0.01f));

            const float armFalloff = std::exp(-dist * 0.012f);
            const float armGlow = armValue * armBrightness * armFalloff;

            const float dustLane = 1.0f - (1.0f - armValue) * 0.3f * armFalloff;

            float pixel = coreGlow + std::max(0.0f, armGlow);
            pixel *= dustLane;
            pixel += 0.01f;

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
            case 3:  applyJu60(slot.mix, time);      break;
            case 4:  applyOxide(slot.mix, time);     break;
            case 5:  applyDist(slot.mix, time);      break;
            case 6:  applyVoid(slot.mix, time);      break;
            case 7:  applyHaze(slot.mix, time);      break;
            case 8:  applyMultiply(slot.mix, time);  break;
            case 9:  applyGrain(slot.mix, time);     break;
            case 10: applyStutter(slot.mix, time);   break;
            case 11: applyShift(slot.mix, time);     break;
            case 12: applyReverser(slot.mix, time);  break;
            default: break;
        }
    }

    // ---- Input Gain: overall scene brightness boost ----
    {
        const float inGainNorm = juce::jlimit(0.0f, 1.0f, (params.inputGain + 24.0f) / 36.0f);
        const float brightMul = 0.5f + inGainNorm * 1.0f;
        if (std::abs(brightMul - 1.0f) > 0.01f)
            for (size_t i = 0; i < pixelData.size(); ++i)
                pixelData[i] *= brightMul;
    }

    // ---- Output Gain: contrast/gamma adjustment ----
    {
        const float outGainNorm = juce::jlimit(0.0f, 1.0f, (params.outputGain + 24.0f) / 36.0f);
        const float gamma = 1.5f - outGainNorm * 1.0f;
        if (std::abs(gamma - 1.0f) > 0.02f)
            for (size_t i = 0; i < pixelData.size(); ++i)
                pixelData[i] = std::pow(juce::jlimit(0.0f, 1.0f, pixelData[i]), gamma);
    }

    // ---- Bayer dithering + scanlines + vignette ----
    juce::Image::BitmapData bitmap(img, juce::Image::BitmapData::writeOnly);

    const float mixAlpha = params.masterMix;
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
            value *= mixAlpha;

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
