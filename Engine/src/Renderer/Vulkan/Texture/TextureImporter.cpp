#include "Renderer/Vulkan/Texture/TextureImporter.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
#include <stb_image.h>

namespace
{
    std::string ToLowerCopy(std::string value)
    {
        std::transform(
            value.begin(),
            value.end(),
            value.begin(),
            [](const unsigned char c)
            {
                return static_cast<char>(std::tolower(c));
            }
        );
        return value;
    }

    std::vector<std::uint8_t> ReadFileBytes(const std::filesystem::path& path)
    {
        std::ifstream file(path, std::ios::binary);
        if (!file) return {};

        file.seekg(0, std::ios::end);
        const std::streamoff size = file.tellg();
        if (size <= 0) return {};

        file.seekg(0, std::ios::beg);

        std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size));
        file.read(reinterpret_cast<char*>(bytes.data()), size);
        if (!file) return {};

        return bytes;
    }

    std::uint16_t ReadU16BE(const std::vector<std::uint8_t>& bytes, const std::size_t offset)
    {
        return static_cast<std::uint16_t>(
            static_cast<std::uint16_t>(bytes[offset]) << 8u |
            static_cast<std::uint16_t>(bytes[offset + 1])
        );
    }

    std::uint16_t ReadU16LE(const std::vector<std::uint8_t>& bytes, const std::size_t offset)
    {
        return static_cast<std::uint16_t>(
            static_cast<std::uint16_t>(bytes[offset]) |
            static_cast<std::uint16_t>(bytes[offset + 1]) << 8u
        );
    }

    std::uint32_t ReadU32BE(const std::vector<std::uint8_t>& bytes, const std::size_t offset)
    {
        return
            static_cast<std::uint32_t>(bytes[offset]) << 24u |
            static_cast<std::uint32_t>(bytes[offset + 1]) << 16u |
            static_cast<std::uint32_t>(bytes[offset + 2]) << 8u |
            static_cast<std::uint32_t>(bytes[offset + 3]);
    }

    std::uint32_t ReadU32LE(const std::vector<std::uint8_t>& bytes, const std::size_t offset)
    {
        return
            static_cast<std::uint32_t>(bytes[offset]) |
            (static_cast<std::uint32_t>(bytes[offset + 1]) << 8u) |
            (static_cast<std::uint32_t>(bytes[offset + 2]) << 16u) |
            (static_cast<std::uint32_t>(bytes[offset + 3]) << 24u);
    }

    bool ParsePngHeader(const std::vector<std::uint8_t>& bytes, std::uint32_t& width, std::uint32_t& height)
    {
        if (bytes.size() < 24) return false;
        for (std::size_t i = 0; i < 8; ++i)
        {
            constexpr std::uint8_t pngSignature[8] =
            {
                0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n'
            };
            if (bytes[i] != pngSignature[i]) return false;
        }

        width = ReadU32BE(bytes, 16);
        height = ReadU32BE(bytes, 20);

        return width > 0 && height > 0;
    }

    bool ParseBmpHeader(const std::vector<std::uint8_t>& bytes, std::uint32_t& width, std::uint32_t& height)
    {
        if (bytes.size() < 26 || bytes[0] != 'B' || bytes[1] != 'M') return false;

        const std::uint32_t rawWidth = ReadU32LE(bytes, 18);
        const auto rawHeight = static_cast<std::int32_t>(ReadU32LE(bytes, 22));

        width = rawWidth;
        height = static_cast<std::uint32_t>(std::abs(rawHeight));

        return width > 0 && height > 0;
    }

    bool ParseTgaHeader(const std::vector<std::uint8_t>& bytes, std::uint32_t& width, std::uint32_t& height)
    {
        if (bytes.size() < 18) return false;

        width = ReadU16LE(bytes, 12);
        height = ReadU16LE(bytes, 14);

        return width > 0 && height > 0;
    }

    bool IsJpegStartOfFrameMarker(const std::uint8_t marker)
    {
        return marker == 0xC0 ||
               marker == 0xC1 ||
               marker == 0xC2 ||
               marker == 0xC3 ||
               marker == 0xC5 ||
               marker == 0xC6 ||
               marker == 0xC7 ||
               marker == 0xC9 ||
               marker == 0xCA ||
               marker == 0xCB ||
               marker == 0xCD ||
               marker == 0xCE ||
               marker == 0xCF;
    }

    bool ParseJpegHeader(const std::vector<std::uint8_t>& bytes, std::uint32_t& width, std::uint32_t& height)
    {
        if (bytes.size() < 4 || bytes[0] != 0xFF || bytes[1] != 0xD8) return false;

        std::size_t offset = 2;
        while (offset + 4 < bytes.size())
        {
            while (offset < bytes.size() && bytes[offset] != 0xFF)
            {
                ++offset;
            }
            while (offset < bytes.size() && bytes[offset] == 0xFF)
            {
                ++offset;
            }

            if (offset >= bytes.size()) return false;
            const std::uint8_t marker = bytes[offset++];

            if (marker == 0xD9 || marker == 0xDA) return false;
            if (offset + 2 > bytes.size()) return false;

            const std::uint16_t segmentLength = ReadU16BE(bytes, offset);
            if (segmentLength < 2 || offset + segmentLength > bytes.size()) return false;
            if (IsJpegStartOfFrameMarker(marker))
            {
                if (segmentLength < 7) return false;

                height = ReadU16BE(bytes, offset + 3);
                width = ReadU16BE(bytes, offset + 5);

                return width > 0 && height > 0;
            }
            offset += segmentLength;
        }
        return false;
    }
}

namespace Quiet::Renderer::Vulkan
{
    TextureImportResult TextureImporter::LoadFromFile(const std::filesystem::path &path)
    {
        TextureImportResult result{};

        if (path.empty())
        {
            result.Error = "Texture path is empty.";
            return result;
        }

        if (!std::filesystem::exists(path))
        {
            result.Error = "Texture file does not exist.";
            return result;
        }

        if (!std::filesystem::is_regular_file(path))
        {
            result.Error = "Texture path does not point to a regular file.";
            return result;
        }

        result.FileSizeBytes = std::filesystem::file_size(path);
        const std::vector<std::uint8_t> bytes = ReadFileBytes(path);
        if (bytes.empty())
        {
            result.Error = "Failed to read texture file.";
            return result;
        }

        const std::string ext = ToLowerCopy(path.extension().string());

        if (ext == ".png")
        {
            result.Format = "PNG";
            result.DimensionsValid = ParsePngHeader(bytes, result.Width, result.Height);
        }
        else if (ext == ".jpg" || ext == ".jpeg")
        {
            result.Format = "JPEG";
            result.DimensionsValid = ParseJpegHeader(bytes, result.Width, result.Height);
        }
        else if (ext == ".tga")
        {
            result.Format = "TGA";
            result.DimensionsValid = ParseTgaHeader(bytes, result.Width, result.Height);
        }
        else if (ext == ".bmp")
        {
            result.Format = "BMP";
            result.DimensionsValid = ParseBmpHeader(bytes, result.Width, result.Height);
        }
        else
        {
            result.Error = "Unsupported texture extension.";
            return result;
        }

        result.Success = true;
        if (!result.DimensionsValid)
        {
            result.Warnings.emplace_back("Texture file was readable, but dimensions could not be parsed.");
        }

        return result;
    }

    TexturePixelData TextureImporter::LoadPixelsFromFile(const std::filesystem::path &path)
    {
        TexturePixelData result{};
        result.Metadata = LoadFromFile(path);
        if (!result.Metadata.Succeeded()) return result;

        int width = 0;
        int height = 0;
        int sourceChannels = 0;

        stbi_uc* pixels = stbi_load(
            path.string().c_str(),
            &width,
            &height,
            &sourceChannels,
            STBI_rgb_alpha
        );

        if (!pixels)
        {
            result.Metadata.Success = false;
            result.Metadata.Error = stbi_failure_reason()
                ? stbi_failure_reason()
                : "stb_image failed to decode texture.";
            return result;
        }

        if (width <= 0 || height <= 0)
        {
            stbi_image_free(pixels);

            result.Metadata.Success = false;
            result.Metadata.Error = "Decoded texture dimensions are invalid.";
            return result;
        }

        const std::size_t byteCount =
            static_cast<std::size_t>(width) *
            static_cast<std::size_t>(height) *
            4u;

        result.Width = static_cast<std::uint32_t>(width);
        result.Height = static_cast<std::uint32_t>(height);
        result.Channels = 4;
        result.Pixels.assign(pixels, pixels + byteCount);

        stbi_image_free(pixels);

        if (sourceChannels != 4)
            result.Metadata.Warnings.emplace_back("Texture was converted to RGBA8 during CPU import.");

        result.Metadata.Width = result.Width;
        result.Metadata.Height = result.Height;
        result.Metadata.DimensionsValid = true;

        return result;
    }
}
