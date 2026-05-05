#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "Renderer/Vulkan/Texture/TextureImportResult.h"

namespace Quiet::Renderer::Vulkan
{
    struct TexturePixelData
    {
        TextureImportResult Metadata;

        std::uint32_t Width = 0;
        std::uint32_t Height = 0;
        std::uint32_t Channels = 4;

        std::vector<std::uint8_t> Pixels;

        [[nodiscard]] bool IsValid() const
        {
            return Width > 0 &&
                   Height > 0 &&
                   Channels == 4 &&
                   !Pixels.empty();
        }

        [[nodiscard]] std::size_t GetByteSize() const
        {
            return Pixels.size();
        }
    };
}