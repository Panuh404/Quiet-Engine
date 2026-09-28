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

        outTexture.Format = payloadFormat;
        outTexture.EncodedData.assign(payload.begin() + static_cast<std::ptrdiff_t>(payloadOffset), payload.end());
        return true;
    }
}