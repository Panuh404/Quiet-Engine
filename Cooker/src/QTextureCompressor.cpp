#include "QTextureCompressor.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <memory>

#include <cmp_core.h>

namespace Quiet::Cooker
{
    namespace
    {
        struct BC7OptionsDeleter
        {
            void operator()(void* options) const noexcept
            {
                if (options) DestroyOptionsBC7(options);
            }
        };

        using BC7OptionsPtr = std::unique_ptr<void, BC7OptionsDeleter>;

        bool CreateBC7Options(const float quality, BC7OptionsPtr& outOptions, std::string& outError)
        {
            void* rawOptions = nullptr;
            if (CreateOptionsBC7(&rawOptions) != 0 || !rawOptions)
            {
                outError = "Failed to create Compressonator BC7 options.";
                return false;
            }

            BC7OptionsPtr options(rawOptions);
            if (SetQualityBC7(options.get(), quality) != 0)
            {
                outError = "Failed to configure Compressonator BC7 quality.";
                return false;
            }

            outOptions = std::move(options);
            return true;
        }

        void CopyEdgePaddedBlock(const QTextureMipLevel& mip, const std::uint32_t blockX, const std::uint32_t blockY, std::array<std::uint8_t, 64>& outBlock)
        {
            const std::uint32_t baseX = blockX * 4u;
            const std::uint32_t baseY = blockY * 4u;

            for (std::uint32_t localY = 0; localY < 4u; ++localY)
            {
                const std::uint32_t sourceY = std::min(baseY + localY, mip.Height - 1u);

                for (std::uint32_t localX = 0; localX < 4u; ++localX)
                {
                    const std::uint32_t sourceX = std::min(baseX + localX, mip.Width - 1u);
                    const std::size_t sourceOffset = (static_cast<std::size_t>(sourceY) * mip.Width + sourceX) * 4u;
                    const std::size_t destinationOffset = (static_cast<std::size_t>(localY) * 4u + localX) * 4u;

                    outBlock[destinationOffset + 0u] = mip.Pixels[sourceOffset + 0u];
                    outBlock[destinationOffset + 1u] = mip.Pixels[sourceOffset + 1u];
                    outBlock[destinationOffset + 2u] = mip.Pixels[sourceOffset + 2u];
                    outBlock[destinationOffset + 3u] = mip.Pixels[sourceOffset + 3u];
                }
            }
        }
    }

    bool CompressRgba8MipChainToBC7(
        const std::vector<QTextureMipLevel>& sourceMipLevels,
        const float quality,
        std::vector<QTextureBC7MipLevel>& outMipLevels,
        QTextureBC7CompressionStats& outStats,
        std::string& outError)
    {
        outMipLevels.clear();
        outStats = {};
        outError.clear();

        if (sourceMipLevels.empty())
        {
            outError = "Cannot BC7-compress an empty mip chain.";
            return false;
        }

        if (quality < 0.0f || quality > 1.0f)
        {
            outError = "BC7 quality must be in the range [0, 1].";
            return false;
        }

        for (const QTextureMipLevel& mip : sourceMipLevels)
        {
            if (!mip.IsValid())
            {
                outError = "Cannot BC7-compress an invalid RGBA8 mip level.";
                return false;
            }
        }

        BC7OptionsPtr options;
        if (!CreateBC7Options(quality, options, outError)) return false;

        outMipLevels.reserve(sourceMipLevels.size());

        for (const QTextureMipLevel& source : sourceMipLevels)
        {
            QTextureBC7MipLevel compressed{};
            compressed.Width = source.Width;
            compressed.Height = source.Height;

            const std::uint32_t blockCountX = compressed.GetBlockCountX();
            const std::uint32_t blockCountY = compressed.GetBlockCountY();
            compressed.Blocks.resize(compressed.GetExpectedByteSize());

            std::array<std::uint8_t, 64> paddedBlock{};
            for (std::uint32_t blockY = 0; blockY < blockCountY; ++blockY)
            {
                for (std::uint32_t blockX = 0; blockX < blockCountX; ++blockX)
                {
                    const std::uint32_t baseX = blockX * 4u;
                    const std::uint32_t baseY = blockY * 4u;

                    const bool completeBlock = baseX + 4u <= source.Width && baseY + 4u <= source.Height;
                    const unsigned char* sourceBlock = nullptr;
                    unsigned int sourceStride = 0;

                    if (completeBlock)
                    {
                        const std::size_t sourceOffset = (static_cast<std::size_t>(baseY) * source.Width + baseX) * 4u;
                        sourceBlock = source.Pixels.data() + sourceOffset;
                        sourceStride = source.Width * 4u;
                    }
                    else
                    {
                        CopyEdgePaddedBlock(source, blockX, blockY, paddedBlock);
                        sourceBlock = paddedBlock.data();
                        sourceStride = 16u;
                    }

                    const std::size_t destinationOffset = (static_cast<std::size_t>(blockY) * blockCountX + blockX) * 16u;
                    unsigned char* destinationBlock = compressed.Blocks.data() + destinationOffset;

                    if (CompressBlockBC7(sourceBlock, sourceStride, destinationBlock, options.get()) != 0)
                    {
                        outMipLevels.clear();
                        outStats = {};
                        outError = "Compressonator failed to encode a BC7 block.";
                        return false;
                    }
                }
            }

            if (!compressed.IsValid())
            {
                outMipLevels.clear();
                outStats = {};
                outError = "BC7 compression produced an invalid mip level.";
                return false;
            }

            outStats.SourceBytes += static_cast<std::uint64_t>(source.Pixels.size());
            outStats.CompressedBytes += static_cast<std::uint64_t>(compressed.Blocks.size());
            outStats.BlockCount += static_cast<std::uint64_t>(blockCountX) * static_cast<std::uint64_t>(blockCountY);
            outMipLevels.push_back(std::move(compressed));
        }

        return true;
    }
}