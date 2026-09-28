#include "QTextureMipGenerator.h"

#include <algorithm>
#include <cmath>
#include <cstddef>

#include "Quiet/Assets/TexturePixelData.h"

namespace Quiet::Cooker
{
    namespace
    {
        float SrgbToLinear(const float value)
        {
            if (value <= 0.04045f) return value / 12.92f;
            return std::pow((value + 0.055f) / 1.055f, 2.4f);
        }

        float LinearToSrgb(const float value)
        {
            const float clamped = std::clamp(value, 0.0f, 1.0f);
            if (clamped <= 0.0031308f) return clamped * 12.92f;
            return 1.055f * std::pow(clamped, 1.0f / 2.4f) - 0.055f;
        }

        std::uint8_t EncodeUnorm8(const float value)
        {
            const float clamped = std::clamp(value, 0.0f, 1.0f);
            return static_cast<std::uint8_t>(std::lround(clamped * 255.0f));
        }

        QTextureMipLevel DownsampleSrgb(const QTextureMipLevel& source)
        {
            QTextureMipLevel result{};
            result.Width = std::max(1u, source.Width / 2u);
            result.Height = std::max(1u, source.Height / 2u);
            result.Pixels.resize(static_cast<std::size_t>(result.Width) * static_cast<std::size_t>(result.Height) * 4u);

            // Use an area-weighted filter rather than assuming
            // all textures are powers of two.
            //
            // This correctly handles dimensions such as:
            // 1920x1080
            //    ↓
            // 960x540
            //    ↓
            // ...
            // and eventually odd-sized levels as well.
            for (std::uint32_t dstY = 0; dstY < result.Height; ++dstY)
            {
                const float sourceY0 = static_cast<float>(dstY) * static_cast<float>(source.Height) / static_cast<float>(result.Height);
                const float sourceY1 = static_cast<float>(dstY + 1u) * static_cast<float>(source.Height) / static_cast<float>(result.Height);

                const std::uint32_t firstY = static_cast<std::uint32_t>(std::floor(sourceY0));
                const std::uint32_t lastY = std::min(source.Height - 1u, static_cast<std::uint32_t>(std::ceil(sourceY1)) - 1u);

                for (std::uint32_t dstX = 0; dstX < result.Width; ++dstX)
                {
                    const float sourceX0 = static_cast<float>(dstX) * static_cast<float>(source.Width) / static_cast<float>(result.Width);
                    const float sourceX1 = static_cast<float>(dstX + 1u) * static_cast<float>(source.Width) / static_cast<float>(result.Width);

                    const std::uint32_t firstX = static_cast<std::uint32_t>(std::floor(sourceX0));
                    const std::uint32_t lastX = std::min(source.Width - 1u, static_cast<std::uint32_t>(std::ceil(sourceX1)) - 1u);

                    float totalWeight = 0.0f;
                    float accumulatedAlpha = 0.0f;
                    float accumulatedRed = 0.0f;
                    float accumulatedGreen = 0.0f;
                    float accumulatedBlue = 0.0f;

                    for (std::uint32_t srcY = firstY; srcY <= lastY; ++srcY)
                    {
                        const float overlapY = std::max(0.0f, std::min(sourceY1, static_cast<float>(srcY + 1u)) - std::max(sourceY0, static_cast<float>(srcY)));

                        for (std::uint32_t srcX = firstX; srcX <= lastX; ++srcX)
                        {
                            const float overlapX = std::max(0.0f, std::min(sourceX1, static_cast<float>(srcX + 1u)) - std::max(sourceX0, static_cast<float>(srcX)));
                            const float weight = overlapX * overlapY;
                            if (weight <= 0.0f) continue;

                            const std::size_t sourceIndex = (static_cast<std::size_t>(srcY) * source.Width + srcX) * 4u;
                            const float red = SrgbToLinear(source.Pixels[sourceIndex + 0] / 255.0f);
                            const float green = SrgbToLinear(source.Pixels[sourceIndex + 1] / 255.0f);
                            const float blue = SrgbToLinear(source.Pixels[sourceIndex + 2] / 255.0f);
                            const float alpha = source.Pixels[sourceIndex + 3] / 255.0f;

                            // Accumulate premultiplied linear RGB.
                            //
                            // This prevents transparent texture edges
                            // from bleeding arbitrary RGB into lower
                            // mip levels.
                            accumulatedRed += red * alpha * weight;
                            accumulatedGreen += green * alpha * weight;
                            accumulatedBlue += blue * alpha * weight;
                            accumulatedAlpha += alpha * weight;
                            totalWeight += weight;
                        }
                    }

                    const float outputAlpha = totalWeight > 0.0f ? accumulatedAlpha / totalWeight : 0.0f;
                    float outputRed = 0.0f;
                    float outputGreen = 0.0f;
                    float outputBlue = 0.0f;

                    if (accumulatedAlpha > 1.0e-8f)
                    {
                        outputRed = accumulatedRed / accumulatedAlpha;
                        outputGreen = accumulatedGreen / accumulatedAlpha;
                        outputBlue = accumulatedBlue / accumulatedAlpha;
                    }

                    const std::size_t destinationIndex = (static_cast<std::size_t>(dstY) * result.Width + dstX) * 4u;

                    result.Pixels[destinationIndex + 0] = EncodeUnorm8(LinearToSrgb(outputRed));
                    result.Pixels[destinationIndex + 1] = EncodeUnorm8(LinearToSrgb(outputGreen));
                    result.Pixels[destinationIndex + 2] = EncodeUnorm8(LinearToSrgb(outputBlue));
                    result.Pixels[destinationIndex + 3] = EncodeUnorm8(outputAlpha);
                }
            }

            return result;
        }
    }

    bool BuildRgba8SrgbMipChain(const Assets::TexturePixelData& source, std::vector<QTextureMipLevel>& outMipLevels, std::string& outError)
    {
        outMipLevels.clear();
        outError.clear();

        if (!source.IsValid())
        {
            outError = "Cannot build mip chain from invalid texture data.";
            return false;
        }

        if (source.Channels != 4)
        {
            outError = "RGBA8 sRGB mip generation requires four channels.";
            return false;
        }

        QTextureMipLevel baseLevel{};

        baseLevel.Width = source.Width;
        baseLevel.Height = source.Height;
        baseLevel.Pixels = source.Pixels;

        outMipLevels.push_back(std::move(baseLevel));

        while (outMipLevels.back().Width > 1u || outMipLevels.back().Height > 1u)
        {
            QTextureMipLevel next = DownsampleSrgb(outMipLevels.back());

            if (!next.IsValid())
            {
                outMipLevels.clear();
                outError = "Mip generation produced an invalid level.";
                return false;
            }

            outMipLevels.push_back(std::move(next));
        }

        return true;
    }
}