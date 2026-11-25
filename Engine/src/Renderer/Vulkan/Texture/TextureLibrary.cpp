#include "Renderer/Vulkan/Texture/TextureLibrary.h"

#include <algorithm>
#include <stdexcept>

#include "Quiet/Core/Log.h"
#include "Renderer/Vulkan/Texture/Texture2D.h"
#include "Quiet/Assets/QTextureReader.h"
#include "Quiet/Assets/TexturePixelData.h"


namespace Quiet::Renderer::Vulkan
{
    #pragma region Raw Texture

    void TextureLibrary::CreateTexture2D(Core::UUID assetID,
                                         const PhysicalDevice &physicalDevice,
                                         const LogicalDevice &logicalDevice,
                                         const GraphicsCommands &graphicsCommands,
                                         const Assets::TexturePixelData& pixelData,
                                         TextureStats stats)
    {
        if (!assetID.IsValid()) throw std::invalid_argument("Texture asset UUID is invalid.");
        if (Has(assetID))
        {
            QUIET_CORE_ERROR("Texture '{}' is already registered.", Core::UUIDToString(assetID));
            throw std::runtime_error("Texture already exists in TextureLibrary: " + Core::UUIDToString(assetID));
        }

        auto texture = std::make_shared<Texture2D>(physicalDevice, logicalDevice, graphicsCommands, pixelData);
        stats.Width = texture->GetWidth();
        stats.Height = texture->GetHeight();
        stats.DimensionsValid = true;
        stats.PixelBytes = pixelData.GetByteSize();
        stats.GpuReady = true;
        stats.WarningCount = static_cast<std::uint32_t>(stats.Warnings.size());

        m_Textures[assetID] = std::move(texture);
        m_Stats[assetID] = std::move(stats);

        QUIET_CORE_INFO("Registered Texture2D '{}'.", Core::UUIDToString(assetID));
    }

    void TextureLibrary::ReplaceTexture2D(Core::UUID assetID,
                                          const PhysicalDevice &physicalDevice,
                                          const LogicalDevice &logicalDevice,
                                          const GraphicsCommands &graphicsCommands,
                                          const Assets::TexturePixelData& pixelData,
                                          TextureStats stats)
    {
        if (!assetID.IsValid()) throw std::invalid_argument("Texture asset UUID is invalid.");

        auto texture = std::make_shared<Texture2D>(physicalDevice, logicalDevice, graphicsCommands, pixelData);
        stats.Width = texture->GetWidth();
        stats.Height = texture->GetHeight();
        stats.DimensionsValid = true;
        stats.PixelBytes = pixelData.GetByteSize();
        stats.GpuReady = true;
        stats.WarningCount = static_cast<std::uint32_t>(stats.Warnings.size());

        m_Textures[assetID] = std::move(texture);
        m_Stats[assetID] = std::move(stats);

        QUIET_CORE_INFO("Replaced Texture2D '{}'.", Core::UUIDToString(assetID));
    }

    #pragma endregion

    #pragma region Cooked Texture

    void TextureLibrary::CreateTexture2D(Core::UUID assetID,
                                         const PhysicalDevice& physicalDevice,
                                         const LogicalDevice& logicalDevice,
                                         const GraphicsCommands& graphicsCommands,
                                         const Assets::QTextureData& textureData,
                                         TextureStats stats)
    {
        if (!assetID.IsValid()) throw std::invalid_argument("Texture asset UUID is invalid.");
        if (Has(assetID))
        {
            QUIET_CORE_ERROR("Texture '{}' is already registered.", Core::UUIDToString(assetID));
            throw std::runtime_error("Texture already exists in TextureLibrary: " + Core::UUIDToString(assetID));
        }

        auto texture = std::make_shared<Texture2D>(physicalDevice, logicalDevice, graphicsCommands, textureData);
        std::size_t textureBytes = 0;

        for (const Assets::QTextureMipData& mip : textureData.MipLevels)
        {
            textureBytes += mip.Data.size();
        }

        stats.Width = texture->GetWidth();
        stats.Height = texture->GetHeight();
        stats.DimensionsValid = true;
        stats.PixelBytes = textureBytes;
        stats.GpuReady = true;
        stats.WarningCount = static_cast<std::uint32_t>(stats.Warnings.size());

        m_Textures[assetID] = std::move(texture);
        m_Stats[assetID] = std::move(stats);

        QUIET_CORE_INFO("Registered cooked Texture2D '{}'.", Core::UUIDToString(assetID));
    }

    void TextureLibrary::ReplaceTexture2D(Core::UUID assetID,
                                          const PhysicalDevice& physicalDevice,
                                          const LogicalDevice& logicalDevice,
                                          const GraphicsCommands& graphicsCommands,
                                          const Assets::QTextureData& textureData,
                                          TextureStats stats)
    {
        if (!assetID.IsValid()) throw std::invalid_argument("Texture asset UUID is invalid.");

        auto texture = std::make_shared<Texture2D>(physicalDevice, logicalDevice, graphicsCommands, textureData);
        std::size_t textureBytes = 0;

        for (const Assets::QTextureMipData& mip : textureData.MipLevels)
        {
            textureBytes += mip.Data.size();
        }

        stats.Width = texture->GetWidth();
        stats.Height = texture->GetHeight();
        stats.DimensionsValid = true;
        stats.PixelBytes = textureBytes;
        stats.GpuReady = true;
        stats.WarningCount = static_cast<std::uint32_t>(stats.Warnings.size());

        m_Textures[assetID] = std::move(texture);
        m_Stats[assetID] = std::move(stats);

        QUIET_CORE_INFO("Replaced cooked Texture2D '{}'.", Core::UUIDToString(assetID));
    }

    #pragma endregion

    void TextureLibrary::RegisterTexture(Core::UUID assetID, TextureStats stats)
    {
        if (!assetID.IsValid()) throw std::invalid_argument("Texture asset UUID is invalid.");
        if (Has(assetID))
        {
            QUIET_CORE_ERROR("Texture '{}' is already registered.", Core::UUIDToString(assetID));
            throw std::runtime_error("Texture already exists in TextureLibrary: " + Core::UUIDToString(assetID));
        }

        stats.WarningCount = static_cast<std::uint32_t>(stats.Warnings.size());
        m_Stats[assetID] = std::move(stats);
        QUIET_CORE_INFO("Registered texture metadata '{}'.", Core::UUIDToString(assetID));
    }

    void TextureLibrary::ReplaceTexture(Core::UUID assetID, TextureStats stats)
    {
        if (!assetID.IsValid()) throw std::invalid_argument("Texture asset UUID is invalid.");
        stats.WarningCount = static_cast<std::uint32_t>(stats.Warnings.size());
        m_Stats[assetID] = std::move(stats);
        QUIET_CORE_INFO("Replaced texture metadata '{}'.", Core::UUIDToString(assetID));
    }

    std::shared_ptr<Texture2D> TextureLibrary::TryGet(Core::UUID assetID) const
    {
        const auto it = m_Textures.find(assetID);
        if (it == m_Textures.end()) return nullptr;
        return it->second;
    }

    std::optional<VkDescriptorImageInfo> TextureLibrary::TryGetDescriptorImageInfo(Core::UUID assetID) const
    {
        const std::shared_ptr<Texture2D> texture = TryGet(assetID);
        if (!texture) return std::nullopt;
        return texture->GetDescriptorImageInfo();
    }

    bool TextureLibrary::Has(Core::UUID assetID) const
    {
        return m_Stats.contains(assetID);
    }

    std::optional<TextureStats> TextureLibrary::GetStats(Core::UUID assetID) const
    {
        const auto it = m_Stats.find(assetID);
        if (it == m_Stats.end())
            return std::nullopt;

        return it->second;
    }

    std::vector<Core::UUID> TextureLibrary::GetAssetIDs() const
    {
        std::vector<Core::UUID> assetIDs;
        assetIDs.reserve(m_Stats.size());

        for (const auto& [assetID, mesh] : m_Stats)
        {
            (void)mesh;
            assetIDs.push_back(assetID);
        }

        std::sort(assetIDs.begin(), assetIDs.end());
        return assetIDs;
    }

    void TextureLibrary::Clear()
    {
        m_Textures.clear();
        m_Stats.clear();
    }
}
