#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include "QTextureCompressor.h"

namespace Quiet::Cooker
{
    struct QTextureWriteStats
    {
        std::uint32_t Width = 0;
        std::uint32_t Height = 0;
        std::uint32_t MipLevels = 0;

        std::uint64_t BC7Bytes = 0;

        std::uint64_t KtxBytes = 0;
        std::uint64_t PayloadBytes = 0;
        std::uint64_t FileBytes = 0;
    };

    bool WriteQTexture(
        const std::filesystem::path& path,
        const std::vector<QTextureBC7MipLevel>& mipLevels,
        QTextureWriteStats& outStats,
        std::string& outError);
}