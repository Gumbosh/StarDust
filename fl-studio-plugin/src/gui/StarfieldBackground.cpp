#include "StarfieldBackground.h"

StarfieldBackground::StarfieldBackground(juce::AudioProcessorValueTreeState& apvtsRef)
    : apvts(apvtsRef)
{
    startTimerHz(18);
    cachedImage = juce::Image(juce::Image::ARGB, kRenderWidth, kRenderHeight, true);
}

void StarfieldBackground::paint(juce::Graphics& g)
{
    if (cachedImage.isValid())
    {
        g.setImageResamplingQuality(juce::Graphics::lowResamplingQuality);
        g.drawImage(cachedImage, getLocalBounds().toFloat());
    }
}

void StarfieldBackground::resized() {}

void StarfieldBackground::timerCallback()
{
    const auto params = readParams();
    timeCounter += 1.0f / 18.0f;
    cachedImage = renderStarfield(params, timeCounter);
    repaint();
}

StarfieldParams StarfieldBackground::readParams() const
{
    return StarfieldParams {
        *apvts.getRawParameterValue("bitDepth"),
        *apvts.getRawParameterValue("grainMix"),
        *apvts.getRawParameterValue("stereoWidth"),
        *apvts.getRawParameterValue("filterCutoff"),
        *apvts.getRawParameterValue("drive"),
        *apvts.getRawParameterValue("grainTune")
    };
}

float StarfieldBackground::hash(float x, float y)
{
    float h = x * 127.1f + y * 311.7f;
    h = std::sin(h) * 43758.5453f;
    return h - std::floor(h);
}

juce::Image StarfieldBackground::renderStarfield(const StarfieldParams& params,
                                                   float time) const
{
    juce::Image img(juce::Image::ARGB, kRenderWidth, kRenderHeight, false);

    // Map params to visuals
    const float crushAmount = (16.0f - params.bitDepth) / 12.0f; // 0=clean, 1=crushed
    const float detuneSpeed = std::abs(params.grainTune) / 24.0f * 2.0f;
    const float noiseFreq = 0.02f + params.stereoWidth * 0.05f;
    const float brightness = 0.12f + crushAmount * 0.4f + params.grain * 0.3f;
    const float glowRadius = 1.0f + params.drive * 4.0f;
    const int starCount = static_cast<int>(50.0f + params.grain * 400.0f);
    const int blockSize = std::max(1, static_cast<int>(9.0f - params.bitDepth / 2.0f));
    const float blurAmount = 1.0f - (params.filterCutoff / 99.0f);

    std::vector<float> pixelData(static_cast<size_t>(kRenderWidth * kRenderHeight), 0.0f);

    // Nebula
    for (int y = 0; y < kRenderHeight; ++y)
    {
        for (int x = 0; x < kRenderWidth; ++x)
        {
            const float fx = static_cast<float>(x);
            const float fy = static_cast<float>(y);
            const float swirl = time * detuneSpeed;

            const float n1 = std::sin(fx * noiseFreq + swirl * 0.7f)
                           * std::cos(fy * noiseFreq * 1.3f + swirl * 0.3f);
            const float n2 = std::sin((fx + fy) * noiseFreq * 0.7f + swirl * 0.5f) * 0.5f;
            const float n3 = std::cos(fx * noiseFreq * 1.5f - fy * noiseFreq * 0.8f
                                      + swirl * 0.9f) * 0.3f;

            float nebula = (n1 + n2 + n3) * 0.33f;
            nebula = (nebula + 1.0f) * 0.5f;
            nebula *= brightness * 0.35f;

            pixelData[static_cast<size_t>(y * kRenderWidth + x)] = nebula;
        }
    }

    // Stars
    const float gridSpacing = std::max(3.0f,
        static_cast<float>(kRenderWidth) / std::sqrt(static_cast<float>(starCount)));
    const int gridCols = static_cast<int>(static_cast<float>(kRenderWidth) / gridSpacing) + 1;
    const int gridRows = static_cast<int>(static_cast<float>(kRenderHeight) / gridSpacing) + 1;

    for (int gy = 0; gy < gridRows; ++gy)
    {
        for (int gx = 0; gx < gridCols; ++gx)
        {
            if (hash(static_cast<float>(gx) + 0.5f, static_cast<float>(gy) + 0.5f) > 0.55f)
                continue;

            const float jx = hash(static_cast<float>(gx) * 1.1f, static_cast<float>(gy) * 2.3f);
            const float jy = hash(static_cast<float>(gx) * 3.7f, static_cast<float>(gy) * 1.9f);
            const int sx = static_cast<int>((static_cast<float>(gx) + jx) * gridSpacing);
            const int sy = static_cast<int>((static_cast<float>(gy) + jy) * gridSpacing);

            if (sx < 0 || sx >= kRenderWidth || sy < 0 || sy >= kRenderHeight)
                continue;

            const float twinkle = hash(static_cast<float>(gx) * 7.3f, static_cast<float>(gy) * 5.1f);
            const float starBright = brightness * (0.4f + 0.6f * std::sin(time * (0.8f + twinkle * 2.5f)));

            const int glowInt = static_cast<int>(glowRadius);
            for (int dy = -glowInt; dy <= glowInt; ++dy)
            {
                for (int dx = -glowInt; dx <= glowInt; ++dx)
                {
                    const int px = sx + dx;
                    const int py = sy + dy;
                    if (px < 0 || px >= kRenderWidth || py < 0 || py >= kRenderHeight)
                        continue;

                    const float dist = std::sqrt(static_cast<float>(dx * dx + dy * dy));
                    const float falloff = std::max(0.0f, 1.0f - dist / (glowRadius + 0.5f));
                    auto& pixel = pixelData[static_cast<size_t>(py * kRenderWidth + px)];
                    pixel = std::min(1.0f, pixel + starBright * falloff * falloff);
                }
            }
        }
    }

    // Blur
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

    // Pixelation
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

    // Bayer dithering
    juce::Image::BitmapData bitmap(img, juce::Image::BitmapData::writeOnly);

    for (int y = 0; y < kRenderHeight; ++y)
    {
        for (int x = 0; x < kRenderWidth; ++x)
        {
            float value = juce::jlimit(0.0f, 1.0f,
                pixelData[static_cast<size_t>(y * kRenderWidth + x)]);

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
