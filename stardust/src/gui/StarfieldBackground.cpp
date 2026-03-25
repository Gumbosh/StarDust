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

    cachedImage = renderStarfield(params, timeCounter);
    repaint();
}

StarfieldParams StarfieldBackground::readParams() const
{
    return StarfieldParams {
        *apvts.getRawParameterValue("bitDepth"),
        *apvts.getRawParameterValue("sampleRate"),
        *apvts.getRawParameterValue("grainMix"),
        *apvts.getRawParameterValue("grainDensity"),
        *apvts.getRawParameterValue("grainSize"),
        *apvts.getRawParameterValue("grainScatter"),
        *apvts.getRawParameterValue("stereoWidth"),
        *apvts.getRawParameterValue("filterCutoff"),
        *apvts.getRawParameterValue("drive"),
        *apvts.getRawParameterValue("tone"),
        *apvts.getRawParameterValue("grainTune"),
        *apvts.getRawParameterValue("mix"),
        *apvts.getRawParameterValue("chorusMix"),
        *apvts.getRawParameterValue("multiplyPanOuter"),
        *apvts.getRawParameterValue("multiplyPanInner"),
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

juce::Image StarfieldBackground::renderStarfield(const StarfieldParams& params, float time)
{
    juce::Image img(juce::Image::ARGB, kRenderWidth, kRenderHeight, false);

    const float crushAmount = (16.0f - params.bitDepth) / 12.0f;
    const float galaxyIntensity = 0.3f + params.mix * 0.7f;   // DESTROY mix → galaxy opacity
    const float starIntensity = params.grain;   // GRAIN → star opacity (0 = invisible, 1 = full)
    const float scatter = params.grainScatter;
    const float rateNorm = (params.sampleRate - 4000.0f) / 44000.0f;
    const int scanlineSpacing = std::max(2, static_cast<int>(2.0f + rateNorm * 6.0f));
    const float scanlineDarken = 0.55f + rateNorm * 0.4f;
    const float blurAmount = 1.0f - (params.filterCutoff / 99.0f);
    const int blockSize = std::max(1, static_cast<int>(9.0f - params.bitDepth / 2.0f));

    const float fw = static_cast<float>(kRenderWidth);
    const float fh = static_cast<float>(kRenderHeight);
    const float cx = fw * 0.5f;
    const float cy = fh * 0.5f;
    const float maxDist = std::sqrt(cx * cx + cy * cy);

    // Galaxy parameters — static, no audio reactivity
    const float rotationSpeed = 0.3f;
    const float tiltAmount = params.grainTune / 24.0f;
    const float spiralTightness = 0.03f;
    const float coreRadius = 25.0f;
    const float coreBrightness = 0.7f * galaxyIntensity;
    const float armBrightness = (0.15f + crushAmount * 0.2f) * galaxyIntensity;

    const float densityNorm = (params.grainDensity - 1.0f) / 19.0f;
    const float sizeNorm = (params.grainSize - 5.0f) / 95.0f;

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

    // ---- Galaxy: Core + Spiral Arms (DESTROY) ----
  if (params.destroyEnabled)
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

            pixelData[static_cast<size_t>(y * kRenderWidth + x)] = pixel;
        }
    }

    // ---- Blur ----
    if (blurAmount > 0.05f)
    {
        const int blurPasses = static_cast<int>(blurAmount * 3.0f);
        auto& temp = tempBuffer_;

        for (int pass = 0; pass < blurPasses; ++pass)
        {
            for (int y = 0; y < kRenderHeight; ++y)
                for (int x = 0; x < kRenderWidth; ++x)
                {
                    float sum = 0.0f; int count = 0;
                    for (int dx = -1; dx <= 1; ++dx)
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
                    for (int dy = -1; dy <= 1; ++dy)
                    {
                        const int ny = y + dy;
                        if (ny >= 0 && ny < kRenderHeight)
                            { sum += temp[static_cast<size_t>(ny * kRenderWidth + x)]; ++count; }
                    }
                    pixelData[static_cast<size_t>(y * kRenderWidth + x)] = sum / static_cast<float>(count);
                }
        }
    }

    // ---- Pixelation ----
    if (blockSize > 1)
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

    const float widthSpread = params.stereoWidth; // WIDTH: brightness spread from center
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

                auto& centerPx = pixelData[static_cast<size_t>(sy * kRenderWidth + sx)];
                centerPx = std::min(1.0f, centerPx + bright);

                if (sl.spikeLen > 0)
                {
                    for (int i = 1; i <= sl.spikeLen; ++i)
                    {
                        const float fade = bright * (1.0f - static_cast<float>(i) / static_cast<float>(sl.spikeLen + 1));
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
    // Apply GRAIN as uniform star opacity
    for (size_t i = 0; i < pixelData.size(); ++i)
    {
        const float starOnly = pixelData[i] - galaxyOnly[i];
        pixelData[i] = galaxyOnly[i] + starOnly * starIntensity;
    }
  } // end GRANULAR

    // ---- Kaleidoscope (MULTIPLY) ----
    if (params.multiplyEnabled && params.chorusMix > 0.01f)
    {
        const float chorusMix = params.chorusMix;
        const int segments = 2 + static_cast<int>(chorusMix * 6.0f);
        const float wedgeAngle = juce::MathConstants<float>::twoPi / static_cast<float>(segments);

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
                float angle = std::atan2(dy, dx);

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

    // ---- Bayer dithering + scanlines + vignette ----
    juce::Image::BitmapData bitmap(img, juce::Image::BitmapData::writeOnly);

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

            // DRIVE + TONE: white vignette on borders
            // DRIVE controls intensity, TONE controls edge sharpness
            if (params.drive > 0.01f)
            {
                // TONE: 0=soft fuzzy glow, 0.5=moderate, 1=crisp hard edge
                const float sharpness = 1.0f + params.tone * 4.0f; // exponent range 1.0 to 5.0
                const float edgeFade = std::pow(edgeNorm, sharpness);
                value += edgeFade * params.drive * 0.8f;
            }

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

    return img;
}
