#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include <vulkan/vulkan.h>

namespace Quiet::Assets
{
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
        void CreateTexture2D(const std::string& name,
                             const PhysicalDevice& physicalDevice,
                             const LogicalDevice& logicalDevice,
                             const GraphicsCommands& graphicsCommands,
                             const Assets::TexturePixelData& pixelData,
                             TextureStats stats);

        void ReplaceTexture2D(const std::string& name,
                              const PhysicalDevice& physicalDevice,
                              const LogicalDevice& logicalDevice,
                              const GraphicsCommands& graphicsCommands,
                              const Assets::TexturePixelData& pixelData,
                              TextureStats stats);

        void RegisterTexture(const std::string& name, TextureStats stats);
        void ReplaceTexture(const std::string& name, TextureStats stats);

        [[nodiscard]] std::shared_ptr<Texture2D> TryGet(const std::string& name) const;
        [[nodiscard]] std::optional<VkDescriptorImageInfo> TryGetDescriptorImageInfo(const std::string& name) const;
        [[nodiscard]] std::optional<TextureStats> GetStats(const std::string& name) const;
        [[nodiscard]] std::vector<std::string> GetNames() const;
        [[nodiscard]] std::size_t GetCount() const { return m_Stats.size(); }
        [[nodiscard]] bool Has(const std::string& name) const;

        void Clear();

    private:
        std::unordered_map<std::string, std::shared_ptr<Texture2D>> m_Textures;
        std::unordered_map<std::string, TextureStats> m_Stats;
    };
}