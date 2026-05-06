#include "Renderer/Vulkan/Texture/TextureLibrary.h"

#include <algorithm>
#include <stdexcept>

#include "Quiet/Core/Log.h"
#include "Renderer/Vulkan/Texture/Texture2D.h"
#include "Renderer/Vulkan/Texture/TexturePixelData.h"

namespace Quiet::Renderer::Vulkan
{
    void TextureLibrary::CreateTexture2D(const std::string &name,
                                         const PhysicalDevice &physicalDevice,
                                         const LogicalDevice &logicalDevice,
                                         const GraphicsCommands &graphicsCommands,
                                         const TexturePixelData &pixelData,
                                         TextureStats stats)
    {
        if (Has(name))
        {
            QUIET_CORE_ERROR("Texture '{}' is already registered.", name);
            throw std::runtime_error("Texture already exists in TextureLibrary: " + name);
        }

        auto texture = std::make_shared<Texture2D>(
            physicalDevice,
            logicalDevice,
            graphicsCommands,
            pixelData
        );

        stats.Width = texture->GetWidth();
        stats.Height = texture->GetHeight();
        stats.DimensionsValid = true;
        stats.PixelBytes = pixelData.GetByteSize();
        stats.GpuReady = true;
        stats.WarningCount = static_cast<std::uint32_t>(stats.Warnings.size());

        m_Textures[name] = std::move(texture);
        m_Stats[name] = std::move(stats);

        QUIET_CORE_INFO("Registered Texture2D '{}'.", name);
    }

    void TextureLibrary::ReplaceTexture2D(const std::string &name,
                                          const PhysicalDevice &physicalDevice,
                                          const LogicalDevice &logicalDevice,
                                          const GraphicsCommands &graphicsCommands,
                                          const TexturePixelData &pixelData,
                                          TextureStats stats)
    {
        auto texture = std::make_shared<Texture2D>(
                physicalDevice,
                logicalDevice,
                graphicsCommands,
                pixelData
            );

        stats.Width = texture->GetWidth();
        stats.Height = texture->GetHeight();
        stats.DimensionsValid = true;
        stats.PixelBytes = pixelData.GetByteSize();
        stats.GpuReady = true;
        stats.WarningCount = static_cast<std::uint32_t>(stats.Warnings.size());

        m_Textures[name] = std::move(texture);
        m_Stats[name] = std::move(stats);

        QUIET_CORE_INFO("Replaced Texture2D '{}'.", name);
    }

    void TextureLibrary::RegisterTexture(const std::string &name, TextureStats stats)
    {
        if (Has(name))
        {
            QUIET_CORE_ERROR("Texture '{}' is already registered.", name);
            throw std::runtime_error("Texture already exists in TextureLibrary: " + name);
        }

        stats.WarningCount = static_cast<std::uint32_t>(stats.Warnings.size());
        m_Stats[name] = std::move(stats);
        QUIET_CORE_INFO("Registered texture metadata '{}'.", name);
    }

    void TextureLibrary::ReplaceTexture(const std::string &name, TextureStats stats)
    {
        stats.WarningCount = static_cast<std::uint32_t>(stats.Warnings.size());
        m_Stats[name] = std::move(stats);
        QUIET_CORE_INFO("Replaced texture metadata '{}'.", name);
    }

    std::shared_ptr<Texture2D> TextureLibrary::TryGet(const std::string &name) const
    {
        const auto it = m_Textures.find(name);
        if (it == m_Textures.end()) return nullptr;

        return it->second;
    }

    std::optional<VkDescriptorImageInfo> TextureLibrary::TryGetDescriptorImageInfo(const std::string &name) const
    {
        const std::shared_ptr<Texture2D> texture = TryGet(name);
        if (!texture) return std::nullopt;

        return texture->GetDescriptorImageInfo();
    }

    bool TextureLibrary::Has(const std::string &name) const
    {
        return m_Stats.contains(name);
    }

    std::optional<TextureStats> TextureLibrary::GetStats(const std::string &name) const
    {
        const auto it = m_Stats.find(name);
        if (it == m_Stats.end())
            return std::nullopt;

        return it->second;
    }

    std::vector<std::string> TextureLibrary::GetNames() const
    {
        std::vector<std::string> names;
        names.reserve(m_Stats.size());

        for (const auto& [name, stats] : m_Stats)
        {
            (void)stats;
            names.push_back(name);
        }

        std::sort(names.begin(), names.end());
        return names;
    }

    void TextureLibrary::Clear()
    {
        m_Textures.clear();
        m_Stats.clear();
    }
}
