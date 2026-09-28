#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "QTextureMipGenerator.h"

namespace Quiet::Cooker
{
    struct QTextureBC7MipLevel
    {
        std::uint32_t Width = 0;
        std::uint32_t Height = 0;

        std::vector<std::uint8_t> Blocks;

        [[nodiscard]] std::uint32_t GetBlockCountX() const
        {
            return std::max(1u, (Width + 3u) / 4u);
        }

        [[nodiscard]] std::uint32_t GetBlockCountY() const
        {
            return std::max(1u, (Height + 3u) / 4u);
        }

        [[nodiscard]] std::size_t GetExpectedByteSize() const
        {
            return static_cast<std::size_t>(GetBlockCountX()) * static_cast<std::size_t>(GetBlockCountY()) * 16u;
        }

        [[nodiscard]] bool IsValid() const
        {
            return Width > 0 && Height > 0 && Blocks.size() == GetExpectedByteSize();
        }
    };

    struct QTextureBC7CompressionStats
    {
        std::uint64_t SourceBytes = 0;
        std::uint64_t CompressedBytes = 0;
        std::uint64_t BlockCount = 0;
    };

    bool CompressRgba8MipChainToBC7(
        const std::vector<QTextureMipLevel>& sourceMipLevels,
        float quality,
        std::vector<QTextureBC7MipLevel>& outMipLevels,
        QTextureBC7CompressionStats& outStats,
        std::string& outError
    );
}