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

StarfieldParams StarfieldBackground::readParams() const
{
    return StarfieldParams {
        *apvts.getRawParameterValue("destroyFader"),
        *apvts.getRawParameterValue("grainMix"),
        *apvts.getRawParameterValue("grainCloud"),
        *apvts.getRawParameterValue("grainDrift"),
        *apvts.getRawParameterValue("grainSpace"),
        *apvts.getRawParameterValue("grainMorph"),
        *apvts.getRawParameterValue("filterCutoff"),
        *apvts.getRawParameterValue("destroyIn"),
        *apvts.getRawParameterValue("destroyOut"),
        *apvts.getRawParameterValue("destroyMix"),
        *apvts.getRawParameterValue("chorusMix"),
        *apvts.getRawParameterValue("chorusSpeed"),
        *apvts.getRawParameterValue("multiplyPanOuter"),
        *apvts.getRawParameterValue("multiplyPanInner"),
        *apvts.getRawParameterValue("filterLfo"),
        *apvts.getRawParameterValue("tapeWow"),
        *apvts.getRawParameterValue("tapeFlutter"),
        *apvts.getRawParameterValue("tapeHiss"),
        *apvts.getRawParameterValue("inputGain"),
        *apvts.getRawParameterValue("outputGain"),
        *apvts.getRawParameterValue("masterMix"),
        *apvts.getRawParameterValue("tapeEnabled") >= 0.5f,
        *apvts.getRawParameterValue("destroyEnabled") >= 0.5f,
        *apvts.getRawParameterValue("granularEnabled") >= 0.5f,
        *apvts.getRawParameterValue("multiplyEnabled") >= 0.5f
    };
}

float StarfieldBackground::hash(float x, float y)
{
    float h = x * 127.1f + y * 311.7f;
    h = std::sin(h) * 43758.5453f;
    return h - std::floor(h);
}

void StarfieldBackground::renderStarfield(const StarfieldParams& params, float time)
{
    auto& img = cachedImage;

    const float crushAmount = (16.0f - 12.0f) / 12.0f;
    // DESTROY Mix → galaxy opacity (0% mix = dim, 100% = full)
    const float mixFactor = params.destroyEnabled ? params.destroyMix : 0.0f;
    const float galaxyIntensity = 0.3f + mixFactor * 0.7f;
    const float starIntensity = params.grainMix;
    const float scatter = params.grainDrift;
    // Derive visual params from macros (same mappings as DSP)
    const float derivedDensity = 1.0f + params.grainCloud * 15.0f;
    const float derivedSize = 80.0f - params.grainCloud * 65.0f;
    const float derivedWidth = params.grainSpace;
    const float derivedPitch = (params.grainMorph - 0.5f) * 24.0f;
    // Map fader position to normalized rate (0=slowest, 1=fastest)
    const float rateNorm = juce::jlimit(0.0f, 1.0f, 1.0f - params.destroyFader / 3.0f);
    const int scanlineSpacing = std::max(2, static_cast<int>(2.0f + rateNorm * 6.0f));
    const float scanlineDarken = 0.55f + rateNorm * 0.4f;
    const float blurAmount = 1.0f - params.filterCutoff; // filterCutoff is 0.0-1.0

    const float fw = static_cast<float>(kRenderWidth);
    const float fh = static_cast<float>(kRenderHeight);
    const float cx = fw * 0.5f;
    const float cy = fh * 0.5f;
    const float maxDist = std::sqrt(cx * cx + cy * cy);

    // Galaxy parameters — static, no audio reactivity
    const float rotationSpeed = 0.3f;
    // Tilt based on fader offset from center (45 RPM = 1.0)
    const float tiltAmount = params.destroyEnabled ? ((params.destroyFader - 1.0f) / 2.0f) : 0.0f;
    const float spiralTightness = 0.03f;
    // IN gain → brighter, wider core (0dB=normal, +12dB=hot glow)
    const float inNorm = juce::jlimit(0.0f, 1.0f, (params.destroyIn + 12.0f) / 24.0f); // 0..1
    // Pixelation driven by input drive (more gain = coarser pixels)
    const int blockSize = std::max(1, static_cast<int>(1.0f + inNorm * 4.0f));
    const float coreRadius = 25.0f + inNorm * 20.0f;
    const float coreBrightness = (0.7f + inNorm * 0.5f) * galaxyIntensity;
    // OUT gain → overall brightness boost
    const float outNorm = juce::jlimit(0.0f, 1.0f, (params.destroyOut + 12.0f) / 24.0f);
    const float outBoost = 1.0f + outNorm * 0.6f;
    const float armBrightness = (0.15f + crushAmount * 0.2f) * galaxyIntensity;

    const float densityNorm = std::min(1.0f, (derivedDensity - 1.0f) / 15.0f);
    const float sizeNorm = std::min(1.0f, (derivedSize - 5.0f) / 75.0f);

    auto& pixelData = pixelData_;
    std::fill(pixelData.begin(), pixelData.end(), 0.0f);

    // Ambient glow for MULTIPLY-only mode (when DESTROY + GRANULAR are off)
    if (params.multiplyEnabled && !params.destroyEnabled && !params.granularEnabled && params.chorusMix > 0.01f)
    {
        for (int y = 0; y < kRenderHeight; ++y)
            for (int x = 0; x < kRenderWidth; ++x)
            {
                const float dx = static_cast<float>(x) - cx;
                const float dy = static_cast<float>(y) - cy;
                const float dist = std::sqrt(dx * dx + dy * dy);
                const float glow = 0.15f * std::exp(-dist * 0.02f) * params.chorusMix;
                pixelData[static_cast<size_t>(y * kRenderWidth + x)] += glow;
            }
    }

    // ---- Galaxy: Core + Spiral Arms (always renders, intensity follows destroy mix) ----
  {
    for (int y = 0; y < kRenderHeight; ++y)
    {
        for (int x = 0; x < kRenderWidth; ++x)
        {
            const float rawFx = static_cast<float>(x) - cx;
            const float rawFy = static_cast<float>(y) - cy;

            const float fx = rawFx * (1.0f + tiltAmount * 0.5f);
            const float fy = rawFy * (1.0f - tiltAmount * 0.5f);

            const float dist = std::sqrt(fx * fx + fy * fy);
            const float angle = std::atan2(fy, fx);

            const float coreGlow = coreBrightness * std::exp(-(dist * dist) / (2.0f * coreRadius * coreRadius));

            const float spiralAngle = angle - dist * spiralTightness + time * rotationSpeed;
            float armValue = std::cos(spiralAngle * 2.0f);
            armValue = (armValue + 1.0f) * 0.5f;

            const float armWidth = 0.3f + dist * 0.008f;
            armValue = std::pow(armValue, 1.0f / (armWidth + 0.01f));

            const float armFalloff = std::exp(-dist * 0.012f);
            float armGlow = armValue * armBrightness * armFalloff;

            const float dustLane = 1.0f - (1.0f - armValue) * 0.3f * armFalloff;

            float pixel = coreGlow + std::max(0.0f, armGlow);
            pixel *= dustLane;
            pixel += 0.01f * galaxyIntensity;

            pixelData[static_cast<size_t>(y * kRenderWidth + x)] = pixel * outBoost;
        }
    }

    // ---- Blur (only when destroy is active) — stronger effect for visible filter response ----
    if (blurAmount > 0.05f && params.destroyEnabled)
    {
        const int blurPasses = 1 + static_cast<int>(blurAmount * 3.0f); // 1-4 passes (was 16)
        const int blurRadius = 2 + static_cast<int>(blurAmount * 4.0f); // 2-6px radius (wider to compensate)
        auto& temp = tempBuffer_;

        for (int pass = 0; pass < blurPasses; ++pass)
        {
            for (int y = 0; y < kRenderHeight; ++y)
                for (int x = 0; x < kRenderWidth; ++x)
                {
                    float sum = 0.0f; int count = 0;
                    for (int dx = -blurRadius; dx <= blurRadius; ++dx)
                    {
                        const int nx = x + dx;
                        if (nx >= 0 && nx < kRenderWidth)
                            { sum += pixelData[static_cast<size_t>(y * kRenderWidth + nx)]; ++count; }
                    }
                    temp[static_cast<size_t>(y * kRenderWidth + x)] = sum / static_cast<float>(count);
                }

            for (int y = 0; y < kRenderHeight; ++y)
                for (int x = 0; x < kRenderWidth; ++x)
                {
                    float sum = 0.0f; int count = 0;
                    for (int dy = -blurRadius; dy <= blurRadius; ++dy)
                    {
                        const int ny = y + dy;
                        if (ny >= 0 && ny < kRenderHeight)
                            { sum += temp[static_cast<size_t>(ny * kRenderWidth + x)]; ++count; }
                    }
                    pixelData[static_cast<size_t>(y * kRenderWidth + x)] = sum / static_cast<float>(count);
                }
        }
    }

    // ---- Pixelation (only when destroy is active) ----
    if (blockSize > 1 && params.destroyEnabled)
    {
        for (int by = 0; by < kRenderHeight; by += blockSize)
            for (int bx = 0; bx < kRenderWidth; bx += blockSize)
            {
                float sum = 0.0f; int count = 0;
                for (int dy = 0; dy < blockSize && (by + dy) < kRenderHeight; ++dy)
                    for (int dx = 0; dx < blockSize && (bx + dx) < kRenderWidth; ++dx)
                        { sum += pixelData[static_cast<size_t>((by + dy) * kRenderWidth + (bx + dx))]; ++count; }
                const float avg = sum / static_cast<float>(count);
                for (int dy = 0; dy < blockSize && (by + dy) < kRenderHeight; ++dy)
                    for (int dx = 0; dx < blockSize && (bx + dx) < kRenderWidth; ++dx)
                        pixelData[static_cast<size_t>((by + dy) * kRenderWidth + (bx + dx))] = avg;
            }
    }
  } // end DESTROY

  if (params.granularEnabled && starIntensity > 0.005f)
  {
    // ---- Stars: 3 depth layers (GRAIN knobs only) ----
    // Save galaxy-only pixels so we can blend stars with uniform opacity
    auto& galaxyOnly = tempBuffer_;
    std::copy(pixelData.begin(), pixelData.end(), galaxyOnly.begin());

    const float widthSpread = derivedWidth; // WIDTH: brightness spread from center
    const float densityMul = 1.0f + densityNorm * 2.0f; // DENSITY: just more stars

    struct StarLayer { int count; float brightMul; int spikeLen; float twinkleSpeed; };
    const StarLayer layers[3] = {
        { static_cast<int>(150.0f * densityMul), 0.25f, 0, 0.6f },
        { static_cast<int>(80.0f * densityMul),
          0.5f, static_cast<int>(1.0f + sizeNorm * 1.5f), 1.2f },
        { static_cast<int>(30.0f * densityMul),
          1.0f, static_cast<int>(2.0f + sizeNorm * 3.0f), 2.5f },
    };

    for (int layer = 0; layer < 3; ++layer)
    {
        const auto& sl = layers[layer];
        const float gridSpacing = std::max(3.0f, fw / std::sqrt(static_cast<float>(sl.count)));
        const int gridCols = static_cast<int>(fw / gridSpacing) + 1;
        const int gridRows = static_cast<int>(fh / gridSpacing) + 1;
        const float layerSeed = static_cast<float>(layer) * 100.0f;

        for (int gy = 0; gy < gridRows; ++gy)
        {
            for (int gx = 0; gx < gridCols; ++gx)
            {
                if (hash(static_cast<float>(gx) + layerSeed, static_cast<float>(gy) + 0.5f) > 0.5f)
                    continue;

                const float jx = hash(static_cast<float>(gx) * 1.1f + layerSeed, static_cast<float>(gy) * 2.3f);
                const float jy = hash(static_cast<float>(gx) * 3.7f + layerSeed, static_cast<float>(gy) * 1.9f);
                float starX = (static_cast<float>(gx) + jx) * gridSpacing;
                float starY = (static_cast<float>(gy) + jy) * gridSpacing;

                // PITCH: vertical shift of star layer (-12..+12 → scroll up/down)
                if (std::abs(derivedPitch) > 0.05f)
                {
                    const float pitchShift = derivedPitch * (fh / 24.0f);
                    starY -= pitchShift;
                    // Wrap around vertically
                    starY = std::fmod(starY + fh * 2.0f, fh);
                }

                // SCATTER: animated wiggle/distortion — stars wobble more as scatter increases
                if (scatter > 0.01f)
                {
                    const float starSeed = hash(static_cast<float>(gx) * 5.3f + layerSeed, static_cast<float>(gy) * 3.1f);
                    const float wiggleSpeed = 1.5f + starSeed * 3.0f;
                    const float wiggleAmount = scatter * 8.0f;
                    starX += std::sin(time * wiggleSpeed + starSeed * 20.0f) * wiggleAmount;
                    starY += std::cos(time * wiggleSpeed * 0.7f + starSeed * 15.0f) * wiggleAmount;
                }

                const int sx = static_cast<int>(starX);
                const int sy = static_cast<int>(starY);

                if (sx < 0 || sx >= kRenderWidth || sy < 0 || sy >= kRenderHeight)
                    continue;

                // WIDTH: controls brightness spread (0 = bright near center, 1 = bright everywhere)
                const float starDx = static_cast<float>(sx) - cx;
                const float starDy = static_cast<float>(sy) - cy;
                const float starDist = std::sqrt(starDx * starDx + starDy * starDy) / maxDist;
                const float distFade = widthSpread + (1.0f - widthSpread) * (1.0f - starDist);

                const float twinkle = hash(static_cast<float>(gx) * 7.3f + layerSeed, static_cast<float>(gy) * 5.1f);
                const float bright = sl.brightMul * distFade
                    * (0.4f + 0.6f * std::sin(time * (sl.twinkleSpeed + twinkle * 2.0f)));

                // Draw star shape mirroring the grain envelope curve
                const int shape = static_cast<int>(params.grainMorph * 3.99f);
                const int radius = sl.spikeLen;

                if (shape == 1) // Gaussian — wide soft bloom, smooth radial falloff
                {
                    constexpr int glowR = 1;
                    for (int dy = -glowR; dy <= glowR; ++dy)
                    {
                        for (int dx = -glowR; dx <= glowR; ++dx)
                        {
                            const int px = sx + dx;
                            const int py = sy + dy;
                            if (px >= 0 && px < kRenderWidth && py >= 0 && py < kRenderHeight)
                            {
                                const float d2 = static_cast<float>(dx * dx + dy * dy);
                                constexpr float sigma = 0.75f;
                                const float glow = bright * std::exp(-d2 / (2.0f * sigma * sigma));
                                auto& pixel = pixelData[static_cast<size_t>(py * kRenderWidth + px)];
                                pixel = std::min(1.0f, pixel + glow);
                            }
                        }
                    }
                }
                else if (shape == 2) // Triangle — random scatter burst
                {
                    // Bright peaked center
                    auto& centerPx = pixelData[static_cast<size_t>(sy * kRenderWidth + sx)];
                    centerPx = std::min(1.0f, centerPx + bright);

                    // 2-3 scattered pixels within radius, seeded by position
                    const int scatterR = std::max(1, radius + 1);
                    const int numFragments = 2 + (radius > 0 ? 1 : 0);
                    for (int f = 0; f < numFragments; ++f)
                    {
                        const float seed1 = hash(static_cast<float>(sx) * (2.3f + static_cast<float>(f) * 5.7f),
                                                  static_cast<float>(sy) * (4.1f + static_cast<float>(f) * 3.3f));
                        const float seed2 = hash(static_cast<float>(sx) * (7.9f + static_cast<float>(f) * 1.1f),
                                                  static_cast<float>(sy) * (2.7f + static_cast<float>(f) * 8.3f));
                        const int dx = static_cast<int>((seed1 * 2.0f - 1.0f) * static_cast<float>(scatterR));
                        const int dy = static_cast<int>((seed2 * 2.0f - 1.0f) * static_cast<float>(scatterR));
                        const int px = sx + dx;
                        const int py = sy + dy;
                        if (px >= 0 && px < kRenderWidth && py >= 0 && py < kRenderHeight)
                        {
                            const float dist = std::sqrt(static_cast<float>(dx * dx + dy * dy));
                            const float fade = bright * (1.0f - dist / (static_cast<float>(scatterR) * 1.5f));
                            if (fade > 0.0f)
                            {
                                auto& pixel = pixelData[static_cast<size_t>(py * kRenderWidth + px)];
                                pixel = std::min(1.0f, pixel + fade);
                            }
                        }
                    }
                }
                else if (shape == 3) // Trapezoid — random tetris block
                {
                    // 7 tetromino shapes: I, O, T, S, Z, L, J
                    // Each defined as 4 pixel offsets from origin
                    constexpr int kNumShapes = 7;
                    constexpr int tetrominoes[kNumShapes][4][2] = {
                        { {0,0}, {1,0}, {2,0}, {3,0} },   // I
                        { {0,0}, {1,0}, {0,1}, {1,1} },   // O
                        { {0,0}, {1,0}, {2,0}, {1,1} },   // T
                        { {0,0}, {1,0}, {1,1}, {2,1} },   // S
                        { {1,0}, {2,0}, {0,1}, {1,1} },   // Z
                        { {0,0}, {0,1}, {0,2}, {1,2} },   // L
                        { {1,0}, {1,1}, {1,2}, {0,2} },   // J
                    };

                    // Pick shape and rotation by hashing star position
                    const float shapeSeed = hash(static_cast<float>(sx) * 5.3f, static_cast<float>(sy) * 9.1f);
                    const int shapeIdx = static_cast<int>(shapeSeed * static_cast<float>(kNumShapes)) % kNumShapes;
                    const float rotSeed = hash(static_cast<float>(sx) * 2.7f, static_cast<float>(sy) * 6.3f);
                    const int rotation = static_cast<int>(rotSeed * 4.0f) % 4;

                    for (int p = 0; p < 4; ++p)
                    {
                        int dx = tetrominoes[shapeIdx][p][0];
                        int dy = tetrominoes[shapeIdx][p][1];

                        // Apply rotation (0=none, 1=90°CW, 2=180°, 3=270°CW)
                        for (int r = 0; r < rotation; ++r)
                        {
                            const int tmp = dx;
                            dx = -dy;
                            dy = tmp;
                        }

                        const int px = sx + dx;
                        const int py = sy + dy;
                        if (px >= 0 && px < kRenderWidth && py >= 0 && py < kRenderHeight)
                        {
                            auto& pixel = pixelData[static_cast<size_t>(py * kRenderWidth + px)];
                            pixel = std::min(1.0f, pixel + bright * 0.9f);
                        }
                    }
                }
                else // Hanning — tiny ring: bright center + cardinal neighbors
                {
                    // Bright center pixel
                    auto& centerPx = pixelData[static_cast<size_t>(sy * kRenderWidth + sx)];
                    centerPx = std::min(1.0f, centerPx + bright);

                    // Soft ring on 4 cardinal neighbors only (no diagonals = round, not square)
                    const float ringStr = bright * 0.4f;
                    const int cardinals[4][2] = { {1,0}, {-1,0}, {0,1}, {0,-1} };
                    for (const auto& c : cardinals)
                    {
                        const int px = sx + c[0];
                        const int py = sy + c[1];
                        if (px >= 0 && px < kRenderWidth && py >= 0 && py < kRenderHeight)
                        {
                            auto& pixel = pixelData[static_cast<size_t>(py * kRenderWidth + px)];
                            pixel = std::min(1.0f, pixel + ringStr);
                        }
                    }

                    // Subtle axis spikes for larger layers
                    if (radius > 0)
                    {
                        for (int i = 2; i <= radius + 1; ++i)
                        {
                            const float fade = bright * 0.25f * (1.0f - static_cast<float>(i) / static_cast<float>(radius + 2));
                            const int arms[4][2] = { {i, 0}, {-i, 0}, {0, i}, {0, -i} };
                            for (const auto& a : arms)
                            {
                                const int px = sx + a[0];
                                const int py = sy + a[1];
                                if (px >= 0 && px < kRenderWidth && py >= 0 && py < kRenderHeight)
                                {
                                    auto& pixel = pixelData[static_cast<size_t>(py * kRenderWidth + px)];
                                    pixel = std::min(1.0f, pixel + fade);
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    // Apply GRAIN as uniform star opacity
    for (size_t i = 0; i < pixelData.size(); ++i)
    {
        const float starOnly = pixelData[i] - galaxyOnly[i];
        pixelData[i] = galaxyOnly[i] + starOnly * starIntensity;
    }
  } // end GRANULAR

    // ---- Tape effects (wow = horizontal drift, flutter = jitter, hiss = noise) ----
    if (params.tapeEnabled)
    {
        // Wow: slow sinusoidal horizontal shift of each row
        if (params.tapeWow > 0.01f)
        {
            auto& temp = tempBuffer_;
            std::copy(pixelData.begin(), pixelData.end(), temp.begin());

            const float wowDepth = params.tapeWow * 12.0f;
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
                    pixelData[static_cast<size_t>(y * kRenderWidth + x)] = v0 * (1.0f - frac) + v1 * frac;
                }
            }
        }

        // Flutter: continuous jitter per row (slow animation)
        if (params.tapeFlutter > 0.01f)
        {
            auto& temp = tempBuffer_;
            std::copy(pixelData.begin(), pixelData.end(), temp.begin());

            const float flutterDepth = params.tapeFlutter * 6.0f;
            for (int y = 0; y < kRenderHeight; ++y)
            {
                const float rowSeed = hash(static_cast<float>(y) * 3.7f, static_cast<float>(y) * 1.3f);
                const float jitterF = std::sin(time * (0.8f + rowSeed * 1.5f) + rowSeed * 20.0f) * flutterDepth;
                const int jitter = static_cast<int>(jitterF);

                for (int x = 0; x < kRenderWidth; ++x)
                {
                    const int srcX = x - jitter;
                    pixelData[static_cast<size_t>(y * kRenderWidth + x)] =
                        (srcX >= 0 && srcX < kRenderWidth)
                            ? temp[static_cast<size_t>(y * kRenderWidth + srcX)] : 0.0f;
                }
            }
        }

        // Hiss: continuous animated noise pixels (slow animation)
        if (params.tapeHiss > 0.01f)
        {
            const float hissIntensity = params.tapeHiss * 0.25f;
            const float hissDensity = params.tapeHiss * 0.15f;
            for (int y = 0; y < kRenderHeight; ++y)
            {
                for (int x = 0; x < kRenderWidth; ++x)
                {
                    const float n = hash(static_cast<float>(x) * 1.7f + time * 1.3f,
                                         static_cast<float>(y) * 2.3f + time * 0.9f);
                    if (n < hissDensity)
                    {
                        auto& pixel = pixelData[static_cast<size_t>(y * kRenderWidth + x)];
                        pixel = std::min(1.0f, pixel + n * hissIntensity / hissDensity);
                    }
                }
            }
        }
    }

    // ---- Kaleidoscope (MULTIPLY) ----
    if (params.multiplyEnabled && params.chorusMix > 0.01f)
    {
        const float chorusMix = params.chorusMix;
        const int segments = 2 + static_cast<int>(chorusMix * 6.0f);
        const float wedgeAngle = juce::MathConstants<float>::twoPi / static_cast<float>(segments);
        // Chorus speed drives kaleidoscope rotation
        const float kRotation = time * params.chorusSpeed * 0.3f;

        auto& source = tempBuffer_;
        std::copy(pixelData.begin(), pixelData.end(), source.begin());
        // Reuse pixelData as kaleidoscope output (we read from source, write to pixelData)
        std::fill(pixelData.begin(), pixelData.end(), 0.0f);
        auto& kaleidoscope = pixelData;

        for (int y = 0; y < kRenderHeight; ++y)
        {
            for (int x = 0; x < kRenderWidth; ++x)
            {
                const float dx = static_cast<float>(x) - cx;
                const float dy = static_cast<float>(y) - cy;
                const float dist = std::sqrt(dx * dx + dy * dy);
                float angle = std::atan2(dy, dx) + kRotation;

                // Fold angle into first wedge
                angle = std::fmod(angle + juce::MathConstants<float>::twoPi, wedgeAngle);

                // Mirror for seamless reflection
                if (angle > wedgeAngle * 0.5f)
                    angle = wedgeAngle - angle;

                // Map back to source coordinates
                const float srcXf = cx + std::cos(angle) * dist;
                const float srcYf = cy + std::sin(angle) * dist;

                // Bilinear interpolation with clamping (no black edges)
                const float clampedX = juce::jlimit(0.0f, static_cast<float>(kRenderWidth - 2), srcXf);
                const float clampedY = juce::jlimit(0.0f, static_cast<float>(kRenderHeight - 2), srcYf);
                const int x0 = static_cast<int>(clampedX);
                const int y0 = static_cast<int>(clampedY);
                const int x1 = x0 + 1;
                const int y1 = y0 + 1;
                const float fx = clampedX - static_cast<float>(x0);
                const float fy = clampedY - static_cast<float>(y0);

                float val = source[static_cast<size_t>(y0 * kRenderWidth + x0)] * (1.0f - fx) * (1.0f - fy)
                    + source[static_cast<size_t>(y0 * kRenderWidth + x1)] * fx * (1.0f - fy)
                    + source[static_cast<size_t>(y1 * kRenderWidth + x0)] * (1.0f - fx) * fy
                    + source[static_cast<size_t>(y1 * kRenderWidth + x1)] * fx * fy;

                kaleidoscope[static_cast<size_t>(y * kRenderWidth + x)] = val;
            }
        }

        // Crossfade between original and kaleidoscope
        for (size_t i = 0; i < pixelData.size(); ++i)
            pixelData[i] = source[i] * (1.0f - chorusMix) + kaleidoscope[i] * chorusMix;
    }

    // ---- Atmospheric haze (1+2 and 3+4 pan spread) ----
    if (params.multiplyEnabled && params.chorusMix > 0.01f)
    {
        const float spread = (params.panOuter + params.panInner) * 0.5f;
        const float hazeAmount = spread * 0.35f * params.chorusMix;

        if (hazeAmount > 0.01f)
        {
            // Blur pass — simple box blur to create soft haze layer
            const int blurRadius = 2 + static_cast<int>(spread * 4.0f);
            auto& haze = tempBuffer_;
            std::fill(haze.begin(), haze.end(), 0.0f);

            for (int y = 0; y < kRenderHeight; ++y)
            {
                for (int x = 0; x < kRenderWidth; ++x)
                {
                    float sum = 0.0f;
                    int count = 0;
                    for (int dy = -blurRadius; dy <= blurRadius; ++dy)
                    {
                        for (int dx = -blurRadius; dx <= blurRadius; ++dx)
                        {
                            const int sx = x + dx;
                            const int sy = y + dy;
                            if (sx >= 0 && sx < kRenderWidth && sy >= 0 && sy < kRenderHeight)
                            {
                                sum += pixelData[static_cast<size_t>(sy * kRenderWidth + sx)];
                                ++count;
                            }
                        }
                    }
                    haze[static_cast<size_t>(y * kRenderWidth + x)] = sum / static_cast<float>(count);
                }
            }

            // Blend haze on top: adds soft atmospheric glow
            for (size_t i = 0; i < pixelData.size(); ++i)
                pixelData[i] = pixelData[i] * (1.0f - hazeAmount * 0.5f) + haze[i] * hazeAmount;
        }
    }

    // ---- Filter LFO: pulsing brightness modulation ----
    if (params.filterLfo > 0.01f)
    {
        const float pulse = 0.5f + 0.5f * std::sin(time * 2.0f);
        const float lfoMod = 1.0f - params.filterLfo * 0.4f * (1.0f - pulse);
        for (size_t i = 0; i < pixelData.size(); ++i)
            pixelData[i] *= lfoMod;
    }

    // ---- Input Gain: overall scene brightness boost (-24..+12 dB) ----
    {
        const float inGainNorm = juce::jlimit(0.0f, 1.0f, (params.inputGain + 24.0f) / 36.0f);
        const float brightMul = 0.5f + inGainNorm * 1.0f;
        if (std::abs(brightMul - 1.0f) > 0.01f)
            for (size_t i = 0; i < pixelData.size(); ++i)
                pixelData[i] *= brightMul;
    }

    // ---- Output Gain: contrast/gamma adjustment (-24..+12 dB) ----
    {
        const float outGainNorm = juce::jlimit(0.0f, 1.0f, (params.outputGain + 24.0f) / 36.0f);
        const float gamma = 1.5f - outGainNorm * 1.0f;
        if (std::abs(gamma - 1.0f) > 0.02f)
            for (size_t i = 0; i < pixelData.size(); ++i)
                pixelData[i] = std::pow(juce::jlimit(0.0f, 1.0f, pixelData[i]), gamma);
    }

    // ---- Bayer dithering + scanlines + vignette ----
    juce::Image::BitmapData bitmap(img, juce::Image::BitmapData::writeOnly);

    // Master Mix: fade between processed starfield and dark background
    const float mixAlpha = params.masterMix;

    for (int y = 0; y < kRenderHeight; ++y)
    {
        const float scanlineMul = (y % scanlineSpacing == 0) ? scanlineDarken : 1.0f;

        for (int x = 0; x < kRenderWidth; ++x)
        {
            float value = juce::jlimit(0.0f, 1.0f,
                pixelData[static_cast<size_t>(y * kRenderWidth + x)]);

            // Edge distance (0 at center, 1 at corners)
            const float vdx = static_cast<float>(x) - cx;
            const float vdy = static_cast<float>(y) - cy;
            const float vdist = std::sqrt(vdx * vdx + vdy * vdy);
            const float edgeNorm = vdist / maxDist;

            // Dark vignette (always applied)
            const float vignette = 1.0f - 0.55f * edgeNorm * edgeNorm;
            value *= vignette;

            value *= scanlineMul;

            // Master Mix: crossfade to black
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
