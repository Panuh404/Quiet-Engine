#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include <vulkan/vulkan.h>

#include "Quiet/Core/UUID.h"

namespace Quiet::Assets
{
    struct QTextureData;
    struct TexturePixelData;
}

namespace Quiet::Renderer::Vulkan
{
    class Texture2D;
    class PhysicalDevice;
    class LogicalDevice;
    class GraphicsCommands;

    struct TextureStats
    {
        std::string Format;

        std::uint32_t Width = 0;
        std::uint32_t Height = 0;
        bool DimensionsValid = false;

        std::uintmax_t FileSizeBytes = 0;
        std::size_t PixelBytes = 0;

        bool GpuReady = false;

        std::uint32_t WarningCount = 0;
        std::vector<std::string> Warnings;
    };

    class TextureLibrary
    {
    public:
        void CreateTexture2D(Core::UUID assetID,
                             const PhysicalDevice& physicalDevice,
                             const LogicalDevice& logicalDevice,
                             const GraphicsCommands& graphicsCommands,
                             const Assets::TexturePixelData& pixelData,
                             TextureStats stats);

        void ReplaceTexture2D(Core::UUID assetID,
                              const PhysicalDevice& physicalDevice,
                              const LogicalDevice& logicalDevice,
                              const GraphicsCommands& graphicsCommands,
                              const Assets::TexturePixelData& pixelData,
                              TextureStats stats);

        void CreateTexture2D(Core::UUID assetID,
                             const PhysicalDevice& physicalDevice,
                             const LogicalDevice& logicalDevice,
                             const GraphicsCommands& graphicsCommands,
                             const Assets::QTextureData& textureData,
                             TextureStats stats);

        void ReplaceTexture2D(Core::UUID assetID,
                              const PhysicalDevice& physicalDevice,
                              const LogicalDevice& logicalDevice,
                              const GraphicsCommands& graphicsCommands,
                              const Assets::QTextureData& textureData,
                              TextureStats stats);

        void RegisterTexture(Core::UUID assetID, TextureStats stats);
        void ReplaceTexture(Core::UUID assetID, TextureStats stats);

        [[nodiscard]] std::shared_ptr<Texture2D> TryGet(Core::UUID assetID) const;
        [[nodiscard]] std::optional<VkDescriptorImageInfo> TryGetDescriptorImageInfo(Core::UUID assetID) const;
        [[nodiscard]] std::optional<TextureStats> GetStats(Core::UUID assetID) const;
        [[nodiscard]] std::vector<Core::UUID> GetAssetIDs() const;
        [[nodiscard]] std::size_t GetCount() const { return m_Stats.size(); }
        [[nodiscard]] bool Has(Core::UUID assetID) const;

        void Clear();

    private:
        std::unordered_map<Core::UUID, std::shared_ptr<Texture2D>> m_Textures;
        std::unordered_map<Core::UUID, TextureStats> m_Stats;
    };
}