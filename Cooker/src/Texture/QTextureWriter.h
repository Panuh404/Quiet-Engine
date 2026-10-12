#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include "Texture/QTextureCompressor.h"

namespace Quiet::Cooker
{
    /**
     * @brief Reports texture dimensions and BC7, KTX2, payload, and file sizes.
     * @ingroup quiet_cooker
     */
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

    /**
     * @brief Writes BC7 sRGB mip levels as embedded KTX2 in a QAsset container.
     * @ingroup quiet_cooker
     * @return False on reported validation, serialization, or write failure.
     * @note Validates supplied mip dimensions without requiring a final 1-by-1 level.
     * @note Creates the parent directory and truncates the destination.
     * @note Uses no KTX2 supercompression or outer QAsset compression.
     * @note Resets statistics and clears outError before processing.
     * @note Does not perform read-back validation or atomic file replacement.
     */
    bool WriteQTexture(
        const std::filesystem::path& path,
        const std::vector<QTextureBC7MipLevel>& mipLevels,
        QTextureWriteStats& outStats,
        std::string& outError);
}