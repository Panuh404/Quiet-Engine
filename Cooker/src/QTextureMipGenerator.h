#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace Quiet::Assets
{
    struct TexturePixelData;
}

namespace Quiet::Cooker
{
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

    bool BuildRgba8SrgbMipChain(const Assets::TexturePixelData& source, std::vector<QTextureMipLevel>& outMipLevels, std::string& outError);
}