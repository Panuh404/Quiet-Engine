#include "Quiet/Assets/QTextureReader.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <limits>
#include <span>
#include <type_traits>
#include <vector>

#include "Quiet/Assets/QAssetFormat.h"

namespace Quiet::Assets
{
    namespace
    {
        constexpr std::array<std::uint8_t, 12> Ktx2Identifier =
        {
            0xAB, 0x4B, 0x54, 0x58,
            0x20, 0x32, 0x30, 0xBB,
            0x0D, 0x0A, 0x1A, 0x0A
        };
        constexpr std::size_t Ktx2HeaderSize = 80;
        constexpr std::size_t Ktx2LevelIndexEntrySize = 24;

        template <typename T>
        bool ReadValue(const std::span<const std::uint8_t> bytes, std::size_t& offset, T& outValue)
        {
            static_assert(std::is_trivially_copyable_v<T>);
            if (offset > bytes.size() || sizeof(T) > bytes.size() - offset) return false;

            std::memcpy(&outValue, bytes.data() + offset, sizeof(T));
            offset += sizeof(T);
            return true;
        }

        bool ReadBytes(const std::span<const std::uint8_t> bytes, std::size_t& offset, void* destination, const std::size_t byteCount)
        {
            if (offset > bytes.size() || byteCount > bytes.size() - offset) return false;
            if (byteCount > 0) std::memcpy(destination, bytes.data() + offset, byteCount);

            offset += byteCount;
            return true;
        }

        bool IsRangeValid(
    const std::size_t totalSize,
    const std::uint64_t offset,
    const std::uint64_t length)
{
    if (offset > totalSize)
        return false;

    return length <=
        static_cast<std::uint64_t>(totalSize) - offset;
}

bool ParseKtx2(
    const std::span<const std::uint8_t> bytes,
    QTextureData& outTexture,
    std::string& outError)
{
    if (bytes.size() < Ktx2HeaderSize)
    {
        outError = "QTexture embedded KTX2 is smaller than its header.";
        return false;
    }

    std::size_t offset = 0;
    std::array<std::uint8_t, 12> identifier{};

    if (!ReadBytes(bytes, offset, identifier.data(), identifier.size()) || identifier != Ktx2Identifier)
    {
        outError = "QTexture contains an invalid KTX2 identifier.";
        return false;
    }

    std::uint32_t vkFormat = 0;
    std::uint32_t typeSize = 0;

    std::uint32_t pixelWidth = 0;
    std::uint32_t pixelHeight = 0;
    std::uint32_t pixelDepth = 0;

    std::uint32_t layerCount = 0;
    std::uint32_t faceCount = 0;
    std::uint32_t levelCount = 0;

    std::uint32_t supercompressionScheme = 0;

    std::uint32_t dfdByteOffset = 0;
    std::uint32_t dfdByteLength = 0;

    std::uint32_t kvdByteOffset = 0;
    std::uint32_t kvdByteLength = 0;

    std::uint64_t sgdByteOffset = 0;
    std::uint64_t sgdByteLength = 0;

    if (!ReadValue(bytes, offset, vkFormat) ||
        !ReadValue(bytes, offset, typeSize) ||
        !ReadValue(bytes, offset, pixelWidth) ||
        !ReadValue(bytes, offset, pixelHeight) ||
        !ReadValue(bytes, offset, pixelDepth) ||
        !ReadValue(bytes, offset, layerCount) ||
        !ReadValue(bytes, offset, faceCount) ||
        !ReadValue(bytes, offset, levelCount) ||
        !ReadValue(bytes, offset, supercompressionScheme) ||
        !ReadValue(bytes, offset, dfdByteOffset) ||
        !ReadValue(bytes, offset, dfdByteLength) ||
        !ReadValue(bytes, offset, kvdByteOffset) ||
        !ReadValue(bytes, offset, kvdByteLength) ||
        !ReadValue(bytes, offset, sgdByteOffset) ||
        !ReadValue(bytes, offset, sgdByteLength))
    {
        outError = "QTexture contains an invalid KTX2 header.";
        return false;
    }

    if (offset != Ktx2HeaderSize)
    {
        outError = "QTexture KTX2 header layout is invalid.";
        return false;
    }

    if (vkFormat == 0)
    {
        outError = "QTexture KTX2 uses an undefined Vulkan format.";
        return false;
    }

    if (pixelWidth == 0 || pixelHeight == 0)
    {
        outError = "QTexture KTX2 does not describe a valid 2D texture.";
        return false;
    }

    if (pixelDepth != 0)
    {
        outError = "QTexture KTX2 3D textures are not currently supported.";
        return false;
    }

    if (layerCount != 0)
    {
        outError = "QTexture KTX2 array textures are not currently supported.";
        return false;
    }

    if (faceCount != 1)
    {
        outError = "QTexture KTX2 cubemaps are not currently supported.";
        return false;
    }

    if (levelCount == 0)
    {
        outError = "QTexture KTX2 contains no explicit mip levels.";
        return false;
    }

    if (supercompressionScheme != 0)
    {
        outError = "QTexture KTX2 supercompression is not currently supported.";
        return false;
    }

    if (dfdByteLength == 0 || !IsRangeValid(bytes.size(), dfdByteOffset, dfdByteLength))
    {
        outError = "QTexture KTX2 contains an invalid Data Format Descriptor.";
        return false;
    }

    if (kvdByteLength > 0 && !IsRangeValid(bytes.size(), kvdByteOffset, kvdByteLength))
    {
        outError = "QTexture KTX2 contains invalid key/value data.";
        return false;
    }

    if (sgdByteOffset != 0 || sgdByteLength != 0)
    {
        outError = "QTexture KTX2 unexpectedly contains supercompression global data.";
        return false;
    }

    if (levelCount > (bytes.size() - offset) / Ktx2LevelIndexEntrySize)
    {
        outError = "QTexture KTX2 level index exceeds the embedded data.";
        return false;
    }

    struct LevelIndexEntry
    {
        std::uint64_t ByteOffset = 0;
        std::uint64_t ByteLength = 0;
        std::uint64_t UncompressedByteLength = 0;
    };

    std::vector<LevelIndexEntry> levelIndex(levelCount);

    for (LevelIndexEntry& entry : levelIndex)
    {
        if (!ReadValue(bytes, offset, entry.ByteOffset) ||
            !ReadValue(bytes, offset, entry.ByteLength) ||
            !ReadValue(bytes, offset, entry.UncompressedByteLength))
        {
            outError = "Failed to read QTexture KTX2 level index.";
            return false;
        }

        if (entry.ByteLength == 0)
        {
            outError = "QTexture KTX2 contains an empty mip level.";
            return false;
        }

        if (entry.ByteLength != entry.UncompressedByteLength)
        {
            outError = "QTexture KTX2 mip sizes are inconsistent for a non-supercompressed texture.";
            return false;
        }

        if (!IsRangeValid(bytes.size(), entry.ByteOffset, entry.ByteLength))
        {
            outError = "QTexture KTX2 mip data exceeds the embedded data.";
            return false;
        }

        if (entry.ByteOffset > std::numeric_limits<std::size_t>::max() ||
            entry.ByteLength > std::numeric_limits<std::size_t>::max())
        {
            outError = "QTexture KTX2 mip data exceeds addressable memory size.";
            return false;
        }
    }

    outTexture.VulkanFormat = vkFormat;
    outTexture.Width = pixelWidth;
    outTexture.Height = pixelHeight;

    outTexture.MipLevels.clear();
    outTexture.MipLevels.reserve(levelCount);

    for (std::uint32_t level = 0; level < levelCount; ++level)
    {
        const LevelIndexEntry& entry = levelIndex[level];
        const auto mipOffset = static_cast<std::size_t>(entry.ByteOffset);
        const auto mipLength = static_cast<std::size_t>(entry.ByteLength);

        QTextureMipData mip{};
        mip.Width = std::max(1u, pixelWidth >> level);
        mip.Height = std::max(1u, pixelHeight >> level);
        mip.Data.assign(bytes.begin() + static_cast<std::ptrdiff_t>(mipOffset), bytes.begin() + static_cast<std::ptrdiff_t>(mipOffset + mipLength));
        outTexture.MipLevels.push_back(std::move(mip));
    }

    if (!outTexture.IsValid())
    {
        outError = "QTexture KTX2 decoded into invalid runtime texture data.";
        return false;
    }

    return true;
}
    }

    bool QTextureReader::LoadFromFile(const std::filesystem::path& path, QTextureData& outTexture, std::string& outError)
    {
        static_assert(std::endian::native == std::endian::little, "QAsset v1 currently supports little-endian targets only.");

        outTexture = {};
        outError.clear();

        std::ifstream input(path, std::ios::binary | std::ios::ate);
        if (!input.is_open())
        {
            outError = "Failed to open QTexture file.";
            return false;
        }

        const std::streampos endPosition = input.tellg();
        if (endPosition <= 0)
        {
            outError = "QTexture file is empty.";
            return false;
        }

        const auto fileSize = static_cast<std::uint64_t>(endPosition);
        if (fileSize > std::numeric_limits<std::size_t>::max())
        {
            outError = "QTexture file exceeds addressable memory size.";
            return false;
        }

        std::vector<std::uint8_t> bytes(static_cast<std::size_t>(fileSize));
        input.seekg(0, std::ios::beg);
        input.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));

        if (!input)
        {
            outError = "Failed to read QTexture file.";
            return false;
        }

        std::size_t fileOffset = 0;

        // QAsset header
        std::array<char, 4> magic{};

        if (!ReadBytes(bytes, fileOffset, magic.data(), magic.size()))
        {
            outError = "QTexture is missing its QAsset header.";
            return false;
        }

        if (!std::equal(magic.begin(), magic.end(), QAssetMagic.begin()))
        {
            outError = "Invalid QAsset magic.";
            return false;
        }

        std::uint16_t containerVersion = 0;
        std::uint16_t assetType = 0;

        std::uint32_t payloadVersion = 0;
        std::uint32_t flags = 0;

        std::uint64_t payloadSize = 0;

        if (!ReadValue(bytes, fileOffset, containerVersion) ||
            !ReadValue(bytes, fileOffset, assetType) ||
            !ReadValue(bytes, fileOffset, payloadVersion) ||
            !ReadValue(bytes, fileOffset, flags) ||
            !ReadValue(bytes, fileOffset, payloadSize))
        {
            outError = "Invalid QAsset header.";
            return false;
        }

        if (containerVersion != CurrentQAssetContainerVersion)
        {
            outError = "Unsupported QAsset container version.";
            return false;
        }

        if (assetType != static_cast<std::uint16_t>(QAssetPayloadType::Texture))
        {
            outError = "QAsset is not a texture.";
            return false;
        }

        if (payloadVersion != CurrentQTexturePayloadVersion)
        {
            outError = "Unsupported QTexture payload version.";
            return false;
        }

        // QTexture v1 currently has no outer
        // QAsset compression.
        if (flags != QAssetFlagNone)
        {
            outError = "QTexture v1 contains unsupported QAsset flags.";
            return false;
        }

        if (payloadSize != bytes.size() - fileOffset)
        {
            outError = "QTexture payload size does not match file size.";
            return false;
        }

        if (payloadSize == 0)
        {
            outError = "QTexture contains an empty payload.";
            return false;
        }

        const std::span<const std::uint8_t> payload
        {
            bytes.data() + fileOffset, static_cast<std::size_t>(payloadSize)
        };

        // QTexture v1
        std::size_t payloadOffset = 0;

        std::uint32_t payloadFormatValue = 0;
        std::uint64_t encodedDataSize = 0;

        if (!ReadValue(payload, payloadOffset, payloadFormatValue) ||
            !ReadValue(payload, payloadOffset, encodedDataSize))
        {
            outError = "Invalid QTexture payload header.";
            return false;
        }

        const auto payloadFormat = static_cast<QTexturePayloadFormat>(payloadFormatValue);
        if (payloadFormat != QTexturePayloadFormat::Ktx2)
        {
            outError = "Unsupported QTexture payload format.";
            return false;
        }

        if (encodedDataSize == 0)
        {
            outError = "QTexture contains an empty encoded texture.";
            return false;
        }

        const std::size_t remainingBytes = payload.size() - payloadOffset;
        if (encodedDataSize != remainingBytes)
        {
            outError = "QTexture embedded data size does not match payload size.";
            return false;
        }

        if (encodedDataSize > std::numeric_limits<std::size_t>::max())
        {
            outError = "QTexture embedded data exceeds addressable memory size.";
            return false;
        }

        QTextureData texture{};
        texture.Format = payloadFormat;
        texture.EncodedData.assign(payload.begin() + static_cast<std::ptrdiff_t>(payloadOffset), payload.end());

        if (!ParseKtx2(texture.EncodedData, texture, outError)) return false;
        outTexture = std::move(texture);
        return true;
    }
}