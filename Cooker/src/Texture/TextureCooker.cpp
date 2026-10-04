#include "Texture/TextureCooker.h"

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <cmp_core.h>
#include <ktx.h>
#include <vulkan/vulkan_core.h>

#include "Texture/QTextureCompressor.h"
#include "Texture/QTextureMipGenerator.h"
#include "Texture/QTextureWriter.h"

#include "Quiet/Assets/QTextureReader.h"
#include "Quiet/Assets/TextureImporter.h"
#include "Quiet/Assets/TexturePixelData.h"

static_assert(KTX_SUCCESS == 0, "Quiet_Cooker expects the libktx API.");

namespace Quiet::Cooker
{
    namespace
    {
        constexpr std::string_view TextureCookSignature = "qtexture-v1|ktx2-bc7-srgb|mipgen-srgb-premul-v1|compressonator-bc7-q0.05";

        std::filesystem::path ResolveAssetPath(const std::filesystem::path& projectRoot, const Project::AssetRecord& record)
        {
            if (record.Path.empty()) return {};
            const std::filesystem::path rawPath = record.Path;

            return rawPath.is_absolute() ? rawPath.lexically_normal() : (projectRoot / rawPath).lexically_normal();
        }

        struct KtxTexture2Deleter
        {
            void operator()(ktxTexture2* texture) const noexcept
            {
                if (texture) ktxTexture2_Destroy(texture);
            }
        };
        using KtxTexture2Ptr = std::unique_ptr<ktxTexture2, KtxTexture2Deleter>;

        bool ValidateQTextureRoundTrip(const std::filesystem::path& path, const std::vector<QTextureBC7MipLevel>& expectedMipLevels, std::string& outError)
        {
            outError.clear();

            if (expectedMipLevels.empty())
            {
                outError = "Cannot validate an empty QTexture mip chain.";
                return false;
            }

            for (const QTextureBC7MipLevel& mip : expectedMipLevels)
            {
                if (!mip.IsValid())
                {
                    outError = "Cannot validate an invalid BC7 mip chain.";
                    return false;
                }
            }

            Assets::QTextureData cookedTexture{};
            if (!Assets::QTextureReader::LoadFromFile(path, cookedTexture, outError))
            {
                return false;
            }

            if (cookedTexture.Format != Assets::QTexturePayloadFormat::Ktx2)
            {
                outError = "QTexture reader returned an unexpected payload format.";
                return false;
            }

            if (cookedTexture.VulkanFormat != static_cast<std::uint32_t>(VK_FORMAT_BC7_SRGB_BLOCK))
            {
                outError = "QTexture reader returned an unexpected Vulkan format.";
                return false;
            }

            if (cookedTexture.Width != expectedMipLevels.front().Width ||
                cookedTexture.Height != expectedMipLevels.front().Height)
            {
                outError = "QTexture reader changed the texture dimensions.";
                return false;
            }

            if (cookedTexture.MipLevels.size() != expectedMipLevels.size())
            {
                outError = "QTexture reader returned an unexpected mip count.";
                return false;
            }

            for (std::size_t level = 0; level < expectedMipLevels.size(); ++level)
            {
                const QTextureBC7MipLevel& expected = expectedMipLevels[level];
                const Assets::QTextureMipData& decoded = cookedTexture.MipLevels[level];

                if (decoded.Width != expected.Width || decoded.Height != expected.Height)
                {
                    outError = "QTexture reader changed BC7 mip dimensions.";
                    return false;
                }

                if (decoded.Data != expected.Blocks)
                {
                    outError = "QTexture reader changed BC7 mip data.";
                    return false;
                }
            }

            ktxTexture2* rawTexture = nullptr;

            const KTX_error_code result = ktxTexture2_CreateFromMemory(
                cookedTexture.EncodedData.data(),
                static_cast<ktx_size_t>(cookedTexture.EncodedData.size()),
                KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT,
                &rawTexture);

            if (result != KTX_SUCCESS)
            {
                outError = std::string("Failed to parse embedded KTX2: ") + ktxErrorString(result);
                return false;
            }

            KtxTexture2Ptr texture(rawTexture);

            if (texture->vkFormat != VK_FORMAT_BC7_SRGB_BLOCK)
            {
                outError = "QTexture KTX2 has an unexpected Vulkan format.";
                return false;
            }

            if (texture->baseWidth != expectedMipLevels.front().Width ||
                texture->baseHeight != expectedMipLevels.front().Height)
            {
                outError = "QTexture KTX2 dimensions changed during round-trip.";
                return false;
            }

            if (texture->numDimensions != 2 ||
                texture->numLevels != expectedMipLevels.size() ||
                texture->numLayers != 1 ||
                texture->numFaces != 1)
            {
                outError = "QTexture KTX2 has an unexpected texture layout.";
                return false;
            }

            if (texture->supercompressionScheme != KTX_SS_NONE)
            {
                outError = "QTexture unexpectedly uses KTX2 supercompression.";
                return false;
            }

            ktxTexture* baseTexture = ktxTexture(texture.get());

            const ktx_uint8_t* imageData = ktxTexture_GetData(baseTexture);
            const ktx_size_t dataSize = ktxTexture_GetDataSize(baseTexture);

            if (!imageData)
            {
                outError = "QTexture KTX2 contains no loaded image data.";
                return false;
            }

            for (std::size_t level = 0; level < expectedMipLevels.size(); ++level)
            {
                const QTextureBC7MipLevel& expected = expectedMipLevels[level];
                const ktx_size_t imageSize = ktxTexture_GetImageSize(baseTexture, static_cast<ktx_uint32_t>(level));

                if (imageSize != expected.Blocks.size())
                {
                    outError = "QTexture BC7 mip byte size changed during round-trip.";
                    return false;
                }

                ktx_size_t imageOffset = 0;

                const KTX_error_code offsetResult = ktxTexture_GetImageOffset(baseTexture, static_cast<ktx_uint32_t>(level), 0, 0, &imageOffset);
                if (offsetResult != KTX_SUCCESS)
                {
                    outError = std::string("Failed to resolve KTX2 mip offset: ") + ktxErrorString(offsetResult);
                    return false;
                }

                if (imageOffset > dataSize || expected.Blocks.size() > dataSize - imageOffset)
                {
                    outError = "QTexture BC7 mip range exceeds loaded KTX2 data.";
                    return false;
                }

                if (std::memcmp(imageData + imageOffset, expected.Blocks.data(), expected.Blocks.size()) != 0)
                {
                    outError = "QTexture BC7 mip data changed during round-trip.";
                    return false;
                }
            }

            return true;
        }

        struct CompressonatorBC7OptionsDeleter
        {
            void operator()(void* options) const noexcept
            {
                if (options) DestroyOptionsBC7(options);
            }
        };
        using CompressonatorBC7OptionsPtr = std::unique_ptr<void, CompressonatorBC7OptionsDeleter>;
    }

    bool ValidateCompressonatorBC7(std::string& outError)
    {
        outError.clear();

        void* rawOptions = nullptr;
        if (CreateOptionsBC7(&rawOptions) != 0 || !rawOptions)
        {
            outError = "Failed to create Compressonator BC7 options.";
            return false;
        }

        CompressonatorBC7OptionsPtr options(rawOptions);
        if (SetQualityBC7(options.get(), 0.5f) != 0)
        {
            outError = "Failed to configure Compressonator BC7 quality.";
            return false;
        }

        // One 4x4 RGBA8 test block.
        //
        // BC7 always consumes 4x4 texels and produces
        // exactly 16 bytes.
        std::array<unsigned char, 64> source{};
        for (std::size_t pixel = 0; pixel < 16; ++pixel)
        {
            source[pixel * 4 + 0] = static_cast<unsigned char>(pixel * 13);
            source[pixel * 4 + 1] = static_cast<unsigned char>(255 - pixel * 11);
            source[pixel * 4 + 2] = static_cast<unsigned char>(pixel * 7);
            source[pixel * 4 + 3] = 255;
        }

        std::array<unsigned char, 16> compressed{};

        // Four RGBA8 pixels = 16 bytes per source row.
        if (CompressBlockBC7(source.data(), 16, compressed.data(), options.get()) != 0)
        {
            outError = "Compressonator BC7 block compression failed.";
            return false;
        }

        std::array<unsigned char, 64> decoded{};
        if (DecompressBlockBC7(compressed.data(), decoded.data(), options.get()) != 0)
        {
            outError = "Compressonator BC7 block decompression failed.";
            return false;
        }

        return true;
    }

    bool CookProjectTextures(CookContext& context)
    {
        const auto& projectRoot = context.ProjectRoot;
        const auto& outputRoot = context.OutputRoot;
        const auto& registry = context.AssetRegistry;
        const auto& previousCookCache = context.PreviousCache;
        auto& outNextCookCache = context.NextCache;
        auto& outRuntimeAssets = context.RuntimeAssets;

        bool success = true;

        for (const Project::AssetRecord& record : registry.Records)
        {
            if (record.BuiltIn || record.Type != Project::AssetType::Texture) continue;

            const std::filesystem::path sourcePath = ResolveAssetPath(projectRoot, record);
            if (sourcePath.empty() || !std::filesystem::is_regular_file(sourcePath))
            {
                std::cerr << "Texture '" << record.ID << "' points to a missing source file: " << sourcePath << '\n';
                success = false;
                continue;
            }

            const std::filesystem::path runtimePath = std::filesystem::path("Content") / "Textures" / (record.ID + ".qtexture");
            const std::filesystem::path cookedPath = outputRoot / runtimePath;

            std::string sourceHash;
            std::string sourceHashError;

            if (!ComputeFileHash(sourcePath, sourceHash, sourceHashError))
            {
                std::cerr << "Failed to fingerprint texture '" << record.ID << "': " << sourceHashError << '\n';
                success = false;
                continue;
            }

            const auto cachedIterator = previousCookCache.Assets.find(record.ID);
            if (cachedIterator != previousCookCache.Assets.end())
            {
                const CookCacheEntry& cached = cachedIterator->second;
                const std::string runtimePathString = runtimePath.generic_string();

                const bool metadataMatches =
                    cached.Type == Project::AssetType::Texture &&
                    cached.SourceHash == sourceHash &&
                    cached.CookSignature == TextureCookSignature &&
                    cached.RuntimePath == runtimePathString;

                if (metadataMatches)
                {
                    std::error_code cacheFileError;
                    const bool cookedFileExists = std::filesystem::is_regular_file(cookedPath, cacheFileError) && !cacheFileError;
                    if (cookedFileExists)
                    {
                        std::string currentCookedHash;
                        std::string cookedHashError;
                        const bool cookedHashSucceeded = ComputeFileHash(cookedPath, currentCookedHash, cookedHashError);

                        if (cookedHashSucceeded && currentCookedHash == cached.CookedHash)
                        {
                            Assets::QTextureData cachedTexture{};
                            std::string cachedTextureError;

                            const bool textureValid = Assets::QTextureReader::LoadFromFile(cookedPath, cachedTexture, cachedTextureError) &&
                                cachedTexture.IsValid() &&
                                cachedTexture.Format == Assets::QTexturePayloadFormat::Ktx2 &&
                                cachedTexture.VulkanFormat == static_cast<std::uint32_t>(VK_FORMAT_BC7_SRGB_BLOCK);

                            if (textureValid)
                            {
                                Project::RuntimeAssetRecord runtimeRecord{};
                                runtimeRecord.ID = record.ID;
                                runtimeRecord.Type = Project::AssetType::Texture;
                                runtimeRecord.Path = runtimePathString;

                                outRuntimeAssets.push_back(std::move(runtimeRecord));
                                outNextCookCache.Assets[record.ID] = cached;

                                std::uint64_t gpuDataBytes = 0;

                                for (const Assets::QTextureMipData& mip : cachedTexture.MipLevels)
                                {
                                    gpuDataBytes += static_cast<std::uint64_t>(mip.Data.size());
                                }

                                std::uint64_t fileSize = 0;
                                cacheFileError.clear();

                                const auto cachedFileSize = std::filesystem::file_size(cookedPath, cacheFileError);
                                if (!cacheFileError)
                                {
                                    fileSize = static_cast<std::uint64_t>(cachedFileSize);
                                }

                                std::cout
                                    << "Texture: " << record.ID << '\n'
                                    << "  Source:      " << sourcePath.filename().string() << '\n'
                                    << "  Cook:        up to date\n"
                                    << "  Dimensions:  " << cachedTexture.Width << 'x' << cachedTexture.Height << '\n'
                                    << "  Mip levels:  " << cachedTexture.MipLevels.size() << '\n'
                                    << "  BC7 data:    " << gpuDataBytes << " bytes\n"
                                    << "  QTexture:    " << cookedPath.filename().string() << '\n'
                                    << "  File size:   " << fileSize << " bytes\n"
                                    << "  GPU format:  BC7 sRGB (8 bpp)\n"
                                    << "  Cache:       hit\n\n";

                                continue;
                            }

                            std::cerr << "Cached QTexture for '" << record.ID << "' failed validation";
                            if (!cachedTextureError.empty())
                            {
                                std::cerr << ": " << cachedTextureError;
                            }
                            std::cerr << ". Recooking.\n";
                        }
                        else if (!cookedHashSucceeded)
                        {
                            std::cerr << "Could not fingerprint cached QTexture for '" << record.ID << "': " << cookedHashError << ". Recooking.\n";
                        }
                    }
                }
            }

            Assets::TexturePixelData pixels = Assets::TextureImporter::LoadPixelsFromFile(sourcePath);
            if (!pixels.IsValid())
            {
                std::cerr << "Failed to import texture '" << record.ID << "': " << pixels.Metadata.Error << '\n';
                success = false;
                continue;
            }

            std::vector<QTextureMipLevel> mipLevels;
            std::string mipError;

            if (!BuildRgba8SrgbMipChain(pixels, mipLevels, mipError))
            {
                std::cerr << "Failed to generate mipmaps for texture '" << record.ID << "': " << mipError << '\n';
                success = false;
                continue;
            }

            std::vector<QTextureBC7MipLevel> bc7MipLevels;
            QTextureBC7CompressionStats bc7Stats{};

            std::string compressionError;
            constexpr float BC7IntegrationQuality = 0.05f;
            const auto bc7Start = std::chrono::steady_clock::now();

            if (!CompressRgba8MipChainToBC7(mipLevels, BC7IntegrationQuality, bc7MipLevels, bc7Stats, compressionError))
            {
                std::cerr << "Failed to BC7-compress texture '" << record.ID << "': " << compressionError << '\n';
                success = false;
                continue;
            }

            const auto bc7End = std::chrono::steady_clock::now();
            const double bc7Seconds = std::chrono::duration<double>(bc7End - bc7Start).count();

            QTextureWriteStats stats{};
            std::string writeError;

            if (!WriteQTexture(cookedPath, bc7MipLevels, stats, writeError))
            {
                std::cerr << "Failed to write texture '" << record.ID << "': " << writeError << '\n';
                success = false;
                continue;
            }

            std::string validationError;

            if (!ValidateQTextureRoundTrip(cookedPath, bc7MipLevels, validationError))
            {
                std::cerr << "Failed to validate texture '" << record.ID << "': " << validationError << '\n';
                success = false;
                continue;
            }

            std::string cookedHash;
            std::string cookedHashError;

            const bool cookedHashSucceeded = ComputeFileHash(cookedPath, cookedHash, cookedHashError);
            if (!cookedHashSucceeded)
            {
                std::cerr << "Warning: texture '" << record.ID << "' was cooked successfully but could not be cached: " << cookedHashError << '\n';
            }

            Project::RuntimeAssetRecord runtimeRecord{};
            runtimeRecord.ID = record.ID;
            runtimeRecord.Type = Project::AssetType::Texture;
            runtimeRecord.Path = runtimePath.generic_string();
            outRuntimeAssets.push_back(std::move(runtimeRecord));

            if (cookedHashSucceeded)
            {
                CookCacheEntry cacheEntry{};
                cacheEntry.Type = Project::AssetType::Texture;
                cacheEntry.SourceHash = sourceHash;
                cacheEntry.CookedHash = cookedHash;
                cacheEntry.CookSignature = std::string(TextureCookSignature);
                cacheEntry.RuntimePath = runtimePath.generic_string();

                outNextCookCache.Assets[record.ID] = std::move(cacheEntry);
            }

            const auto percentage = [](const std::uint64_t value, const std::uint64_t reference)
            {
                return reference > 0 ? static_cast<double>(value) / static_cast<double>(reference) * 100.0 : 0.0;
            };

            const std::uint64_t baseRgbaBytes = static_cast<std::uint64_t>(mipLevels.front().Pixels.size());

            std::cout
            << "Texture: " << record.ID << '\n'
            << "  Source:      " << sourcePath.filename().string() << '\n'
            << "  Format:      " << pixels.Metadata.Format << " -> QTexture/KTX2 BC7 sRGB\n"
            << "  Dimensions:  " << stats.Width << 'x' << stats.Height << '\n'
            << "  Mip levels:  " << stats.MipLevels << '\n'
            << "  RGBA8 data:  " << baseRgbaBytes << " bytes\n"
            << "  Mip data:    " << bc7Stats.SourceBytes << " bytes (" << std::fixed << std::setprecision(3) << percentage(bc7Stats.SourceBytes, baseRgbaBytes) << "% of base)\n"
            << "  BC7 data:    " << bc7Stats.CompressedBytes << " bytes (" << std::fixed << std::setprecision(3) << percentage(bc7Stats.CompressedBytes, bc7Stats.SourceBytes) << "% of raw mip data)\n"
            << "  BC7 time:    " << std::fixed << std::setprecision(3) << bc7Seconds << " s\n"
            << "  KTX2 data:   " << stats.KtxBytes << " bytes (" << std::fixed << std::setprecision(3) << percentage(stats.KtxBytes, stats.BC7Bytes) << "% of BC7 data)\n"
            << "  QTexture:    " << cookedPath.filename().string() << '\n'
            << "  File size:   " << stats.FileBytes << " bytes\n"
            << "  GPU format:  BC7 sRGB (8 bpp)\n"
            << "  Outer comp.: none\n"
            << "  Round-trip:  OK\n"
            << "  Cache:       miss (recooked)\n\n";
        }

        return success;
    }
}
