#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include "Quiet/Assets/QTextureFormat.h"

namespace Quiet::Assets
{
    struct QTextureMipData
    {
        std::uint32_t Width = 0;
        std::uint32_t Height = 0;
        std::vector<std::uint8_t> Data;

        [[nodiscard]] bool IsValid() const
        {
            return Width > 0 && Height > 0 && !Data.empty();
        }
    };

    struct QTextureData
    {
        QTexturePayloadFormat Format = QTexturePayloadFormat::Unknown;

        // Raw VkFormat value stored by KTX2.
        // QTextureReader itself remains independent from Vulkan headers.
        std::uint32_t VulkanFormat = 0;
        std::uint32_t Width = 0;
        std::uint32_t Height = 0;
        std::vector<QTextureMipData> MipLevels;

        // Keep the original embedded KTX2 for now.
        // The Cooker still uses libktx as an independent validation path.
        std::vector<std::uint8_t> EncodedData;

        [[nodiscard]] bool IsValid() const
        {
            if (Format == QTexturePayloadFormat::Unknown ||
                VulkanFormat == 0 ||
                Width == 0 ||
                Height == 0 ||
                MipLevels.empty())
            {
                return false;
            }

            for (const QTextureMipData& mip : MipLevels)
            {
                if (!mip.IsValid()) return false;
            }
            return true;
        }
    };

    class QTextureReader
    {
    public:
        static bool LoadFromFile(const std::filesystem::path& path, QTextureData& outTexture, std::string& outError);
    };
}