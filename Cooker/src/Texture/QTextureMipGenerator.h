#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <cstddef>

namespace Quiet::Assets
{
    struct TexturePixelData;
}

namespace Quiet::Cooker
{
    /**
     * @brief Stores one mip's dimensions and four-channel RGBA8 pixels.
     * @ingroup quiet_cooker
     */
    struct QTextureMipLevel
    {
        std::uint32_t Width = 0;
        std::uint32_t Height = 0;

        std::vector<std::uint8_t> Pixels;

        [[nodiscard]] bool IsValid() const
        {
            return Width > 0 && Height > 0 && Pixels.size() == static_cast<std::size_t>(Width) * static_cast<std::size_t>(Height) * 4u;
        }
    };

    /**
     * @brief Builds a complete RGBA8 sRGB mip chain ending at 1-by-1.
     * @ingroup quiet_cooker
     * @return False for invalid source data or a generated invalid level.
     * @note Requires four channels and copies the source as the base level.
     * @note Filters linear RGB with alpha weighting and supports odd dimensions.
     * @note Clears output before generation and on reported generation failure.
     */
    bool BuildRgba8SrgbMipChain(const Assets::TexturePixelData& source, std::vector<QTextureMipLevel>& outMipLevels, std::string& outError);
}