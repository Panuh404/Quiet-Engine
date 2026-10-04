#include "Texture/QTextureWriter.h"

#include <bit>
#include <cstdlib>
#include <fstream>
#include <memory>
#include <type_traits>
#include <vector>
#include <algorithm>

#include <ktx.h>
#include <vulkan/vulkan_core.h>

#include "Quiet/Assets/QAssetFormat.h"
#include "Quiet/Assets/QTextureFormat.h"

namespace Quiet::Cooker
{
    namespace
    {
        constexpr std::uint64_t QAssetHeaderSize =
            4u +
            sizeof(std::uint16_t) +
            sizeof(std::uint16_t) +
            sizeof(std::uint32_t) +
            sizeof(std::uint32_t) +
            sizeof(std::uint64_t);

        struct KtxTexture2Deleter
        {
            void operator()(ktxTexture2* texture) const noexcept
            {
                if (texture) ktxTexture2_Destroy(texture);
            }
        };

        using KtxTexture2Ptr = std::unique_ptr<ktxTexture2, KtxTexture2Deleter>;

        std::string MakeKtxError(const char* operation, const KTX_error_code result)
        {
            const char* description = ktxErrorString(result);
            return std::string(operation) + ": " + (description ? description : "Unknown libktx error.");
        }

        template <typename T>
        void AppendValue(std::vector<std::uint8_t>& bytes, const T& value)
        {
            static_assert(std::is_trivially_copyable_v<T>);
            const auto* first = reinterpret_cast<const std::uint8_t*>(&value);

            bytes.insert(bytes.end(), first, first + sizeof(T));
        }

        void AppendBytes(std::vector<std::uint8_t>& bytes, const void* data, const std::size_t byteCount)
        {
            if (byteCount == 0) return;
            const auto* first = static_cast<const std::uint8_t*>(data);

            bytes.insert(bytes.end(), first, first + byteCount);
        }

        template <typename T>
        bool WriteValue(std::ofstream& output, const T& value)
        {
            static_assert(std::is_trivially_copyable_v<T>);
            output.write(reinterpret_cast<const char*>(&value), sizeof(T));
            return output.good();
        }

        bool WriteBytes(std::ofstream& output, const void* bytes, const std::size_t byteCount)
        {
            if (byteCount == 0) return true;
            output.write(static_cast<const char*>(bytes), static_cast<std::streamsize>(byteCount));
            return output.good();
        }
    }

    bool WriteQTexture(
        const std::filesystem::path& path,
        const std::vector<QTextureBC7MipLevel>& mipLevels,
        QTextureWriteStats& outStats,
        std::string& outError)
    {
        static_assert(std::endian::native == std::endian::little, "QAsset v1 currently supports little-endian targets only.");

        outStats = {};
        outError.clear();

        if (mipLevels.empty())
        {
            outError = "Cannot write QTexture without mip levels.";
            return false;
        }

        if (!mipLevels.front().IsValid())
        {
            outError = "QTexture base mip level is invalid.";
            return false;
        }

        for (std::size_t level = 0; level < mipLevels.size(); ++level)
        {
            const QTextureBC7MipLevel& mip = mipLevels[level];
            if (!mip.IsValid())
            {
                outError = "QTexture contains an invalid mip level.";
                return false;
            }

            const std::uint32_t expectedWidth = std::max(1u, mipLevels.front().Width >> static_cast<std::uint32_t>(level));
            const std::uint32_t expectedHeight = std::max(1u, mipLevels.front().Height >> static_cast<std::uint32_t>(level));

            if (mip.Width != expectedWidth || mip.Height != expectedHeight)
            {
                outError = "QTexture mip dimensions do not form a complete mip chain.";
                return false;
            }
        }

        // Build the internal KTX2 object.
        ktxTextureCreateInfo createInfo{};
        createInfo.vkFormat = VK_FORMAT_BC7_SRGB_BLOCK;
        createInfo.baseWidth = mipLevels.front().Width;
        createInfo.baseHeight = mipLevels.front().Height;
        createInfo.baseDepth = 1;
        createInfo.numDimensions = 2;
        createInfo.numLevels = static_cast<ktx_uint32_t>(mipLevels.size());
        createInfo.numLayers = 1;
        createInfo.numFaces = 1;
        createInfo.isArray = KTX_FALSE;
        createInfo.generateMipmaps = KTX_FALSE;

        ktxTexture2* rawTexture = nullptr;
        KTX_error_code result = ktxTexture2_Create(&createInfo, KTX_TEXTURE_CREATE_ALLOC_STORAGE, &rawTexture);
        if (result != KTX_SUCCESS)
        {
            outError = MakeKtxError("Failed to create KTX2 texture", result);
            return false;
        }

        KtxTexture2Ptr texture(rawTexture);
        for (std::size_t level = 0; level < mipLevels.size(); ++level)
        {
            const QTextureBC7MipLevel& mip = mipLevels[level];

            result = ktxTexture_SetImageFromMemory(ktxTexture(texture.get()), static_cast<ktx_uint32_t>(level), 0, 0, mip.Blocks.data(), static_cast<ktx_size_t>(mip.Blocks.size()));
            if (result != KTX_SUCCESS)
            {
                outError = MakeKtxError("Failed to set KTX2 BC7 mip image data", result);
                return false;
            }
        }

        // Serialize KTX2 into memory.
        //
        // We deliberately do NOT let libktx write the file
        // directly because the actual disk file is QTexture,
        // not a naked KTX2.
        ktx_uint8_t* rawKtxBytes = nullptr;
        ktx_size_t ktxByteCount = 0;

        result = ktxTexture2_WriteToMemory(texture.get(), &rawKtxBytes, &ktxByteCount);
        if (result != KTX_SUCCESS)
        {
            outError = MakeKtxError("Failed to serialize KTX2 payload", result);
            return false;
        }

        if (!rawKtxBytes || ktxByteCount == 0)
        {
            std::free(rawKtxBytes);
            outError = "libktx produced an empty KTX2 payload.";
            return false;
        }

        std::vector<std::uint8_t> ktxBytes(rawKtxBytes, rawKtxBytes + ktxByteCount);
        std::free(rawKtxBytes);

        // Build QTexture v1 payload.
        //
        // uint32 : QTexturePayloadFormat
        // uint64 : embedded KTX2 size
        // bytes  : KTX2
        std::vector<std::uint8_t> payload;
        payload.reserve(sizeof(std::uint32_t) + sizeof(std::uint64_t) + ktxBytes.size());

        const std::uint32_t payloadFormat = static_cast<std::uint32_t>(Assets::QTexturePayloadFormat::Ktx2);
        const std::uint64_t embeddedDataSize = static_cast<std::uint64_t>(ktxBytes.size());

        AppendValue(payload, payloadFormat);
        AppendValue(payload, embeddedDataSize);
        AppendBytes(payload, ktxBytes.data(), ktxBytes.size());

        // Prepare output path.
        std::error_code ec;
        if (!path.parent_path().empty())
        {
            std::filesystem::create_directories(path.parent_path(), ec);
            if (ec)
            {
                outError = "Failed to create QTexture output directory: " + ec.message();
                return false;
            }
        }

        // Write QAsset + QTexture.
        std::ofstream output(path, std::ios::binary | std::ios::trunc);
        if (!output.is_open())
        {
            outError = "Failed to open QTexture output file.";
            return false;
        }

        if (!WriteBytes(output, Assets::QAssetMagic.data(), Assets::QAssetMagic.size()))
        {
            outError = "Failed to write QAsset magic.";
            return false;
        }

        const std::uint16_t containerVersion = Assets::CurrentQAssetContainerVersion;
        const std::uint16_t assetType = static_cast<std::uint16_t>(Assets::QAssetPayloadType::Texture);
        const std::uint32_t payloadVersion = Assets::CurrentQTexturePayloadVersion;

        // No outer compression yet.
        const std::uint32_t flags = Assets::QAssetFlagNone;
        const std::uint64_t payloadSize = static_cast<std::uint64_t>(payload.size());

        if (!WriteValue(output, containerVersion) ||
            !WriteValue(output, assetType) ||
            !WriteValue(output, payloadVersion) ||
            !WriteValue(output, flags) ||
            !WriteValue(output, payloadSize))
        {
            outError = "Failed to write QAsset header.";
            return false;
        }

        if (!WriteBytes(output, payload.data(), payload.size()))
        {
            outError = "Failed to write QTexture payload.";
            return false;
        }

        outStats.Width = mipLevels.front().Width;
        outStats.Height = mipLevels.front().Height;
        outStats.MipLevels = static_cast<std::uint32_t>(mipLevels.size());

        for (const QTextureBC7MipLevel& mip : mipLevels)
        {
            outStats.BC7Bytes += static_cast<std::uint64_t>(mip.Blocks.size());
        }

        outStats.KtxBytes = static_cast<std::uint64_t>(ktxBytes.size());
        outStats.PayloadBytes = static_cast<std::uint64_t>(payload.size());
        outStats.FileBytes = QAssetHeaderSize + outStats.PayloadBytes;
        return true;
    }
}