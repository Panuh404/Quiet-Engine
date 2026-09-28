#pragma once

#include <cstdint>
#include <filesystem>
#include <string>

namespace Quiet::Assets
{
    struct TexturePixelData;
}

namespace Quiet::Cooker
{
    struct QTextureWriteStats
    {
        std::uint32_t Width = 0;
        std::uint32_t Height = 0;
        std::uint32_t MipLevels = 0;

        std::uint64_t SourceBytes = 0;
        std::uint64_t KtxBytes = 0;
        std::uint64_t PayloadBytes = 0;
        std::uint64_t FileBytes = 0;
    };

    bool WriteQTexture(const std::filesystem::path& path, const Assets::TexturePixelData& pixels, QTextureWriteStats& outStats, std::string& outError);
}