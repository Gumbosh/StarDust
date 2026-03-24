#include "StarfieldBackground.h"
#include <cmath>

StarfieldBackground::StarfieldBackground(juce::AudioProcessorValueTreeState& apvtsRef,
                                           std::atomic<float>& outL,
                                           std::atomic<float>& outR)
    : apvts(apvtsRef), outputLevelL(outL), outputLevelR(outR)
{
    startTimerHz(18);
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
    const float dt = 1.0f / 18.0f;
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
        *apvts.getRawParameterValue("grainTune"),
        *apvts.getRawParameterValue("mix")
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
    const float scatter = params.grainScatter;
    const float visualIntensity = 0.3f + params.mix * 0.7f;
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
    const float spiralTightness = 0.03f + params.stereoWidth * 0.04f;
    const float coreRadius = 25.0f;
    const float coreBrightness = 0.7f * visualIntensity;
    const float armBrightness = (0.15f + crushAmount * 0.2f + params.grain * 0.15f) * visualIntensity;

    const float densityNorm = (params.grainDensity - 1.0f) / 19.0f;
    const float sizeNorm = (params.grainSize - 5.0f) / 95.0f;

    std::vector<float> pixelData(static_cast<size_t>(kRenderWidth * kRenderHeight), 0.0f);

    // ---- Galaxy: Core + Spiral Arms ----
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

            const float armWidth = 0.3f + dist * 0.008f + scatter * 0.25f;
            armValue = std::pow(armValue, 1.0f / (armWidth + 0.01f));

            const float armFalloff = std::exp(-dist * 0.012f);
            float armGlow = armValue * armBrightness * armFalloff;

            if (scatter > 0.05f)
            {
                const float turbX = static_cast<float>(x) * 0.08f + time * rotationSpeed * 0.5f;
                const float turbY = static_cast<float>(y) * 0.08f - time * rotationSpeed * 0.3f;
                const float turb = std::sin(turbX) * std::cos(turbY * 1.3f) * scatter * 0.12f;
                armGlow += turb * armFalloff;
            }

            const float dustLane = 1.0f - (1.0f - armValue) * 0.3f * armFalloff;

            float pixel = coreGlow + std::max(0.0f, armGlow);
            pixel *= dustLane;
            pixel += 0.01f * visualIntensity;

            const float fog = densityNorm * 0.25f * std::exp(-dist * 0.008f) * visualIntensity;
            pixel += fog;

            pixelData[static_cast<size_t>(y * kRenderWidth + x)] = pixel;
        }
    }

    // ---- Blur ----
    if (blurAmount > 0.05f)
    {
        const int blurPasses = static_cast<int>(blurAmount * 3.0f);
        std::vector<float> temp(pixelData.size());

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

    // ---- Stars: 3 depth layers ----
    struct StarLayer { int count; float brightMul; int spikeLen; float twinkleSpeed; };
    const StarLayer layers[3] = {
        { static_cast<int>(80.0f + params.grain * 120.0f), 0.25f, 0, 0.6f },
        { static_cast<int>(30.0f + params.grain * 100.0f),
          0.5f, static_cast<int>(1.0f + sizeNorm * 1.5f), 1.2f },
        { static_cast<int>(10.0f + params.grain * 40.0f),
          1.0f, static_cast<int>(2.0f + sizeNorm * 3.0f + params.drive * 2.0f), 2.5f },
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
                const int sx = static_cast<int>((static_cast<float>(gx) + jx) * gridSpacing);
                const int sy = static_cast<int>((static_cast<float>(gy) + jy) * gridSpacing);

                if (sx < 0 || sx >= kRenderWidth || sy < 0 || sy >= kRenderHeight)
                    continue;

                const float twinkle = hash(static_cast<float>(gx) * 7.3f + layerSeed, static_cast<float>(gy) * 5.1f);
                const float bright = sl.brightMul * visualIntensity
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

    // ---- Warp / Glitch (scatter only, no audio) ----
    const float warpAmount = scatter * 6.0f;
    if (warpAmount > 0.5f)
    {
        std::vector<float> warped(pixelData.size(), 0.0f);
        const float warpFreq = 0.04f + scatter * 0.03f;
        const float warpSpeed = 3.0f;

        for (int y = 0; y < kRenderHeight; ++y)
        {
            float offset = std::sin(static_cast<float>(y) * warpFreq + time * warpSpeed) * warpAmount;
            const float tearChance = scatter * 0.05f;
            if (hash(static_cast<float>(y) * 0.1f, time * 10.0f) < tearChance)
                offset += (hash(static_cast<float>(y), time) - 0.5f) * (20.0f * scatter);

            const int intOffset = static_cast<int>(offset);
            for (int x = 0; x < kRenderWidth; ++x)
            {
                int srcX = ((x + intOffset) % kRenderWidth + kRenderWidth) % kRenderWidth;
                warped[static_cast<size_t>(y * kRenderWidth + x)] =
                    pixelData[static_cast<size_t>(y * kRenderWidth + srcX)];
            }
        }
        pixelData = std::move(warped);
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

            const float vdx = static_cast<float>(x) - cx;
            const float vdy = static_cast<float>(y) - cy;
            const float vdist = std::sqrt(vdx * vdx + vdy * vdy);
            const float vignette = 1.0f - 0.55f * (vdist / maxDist) * (vdist / maxDist);
            value *= vignette;

            value *= scanlineMul;
            value *= visualIntensity;

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
