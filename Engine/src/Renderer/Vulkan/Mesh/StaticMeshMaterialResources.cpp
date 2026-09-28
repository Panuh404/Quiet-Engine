#include "Renderer/Vulkan/Mesh/StaticMeshMaterialResources.h"

#include <stdexcept>

#include "Quiet/Core/Log.h"
#include "Renderer/Vulkan/Context/LogicalDevice.h"
#include "Renderer/Vulkan/Texture/Texture2D.h"
#include "Renderer/Vulkan/Texture/TextureLibrary.h"
#include "Quiet/Assets/TexturePixelData.h"

namespace
{
    Quiet::Assets::TexturePixelData CreateFallbackWhitePixel()
    {
        Quiet::Assets::TexturePixelData pixelData{};
        pixelData.Metadata.Success = true;
        pixelData.Metadata.Format = "RGBA8";
        pixelData.Metadata.Width = 1;
        pixelData.Metadata.Height = 1;
        pixelData.Metadata.DimensionsValid = true;
        pixelData.Metadata.FileSizeBytes = 4;

        pixelData.Width = 1;
        pixelData.Height = 1;
        pixelData.Channels = 4;
        pixelData.Pixels = { 255, 255, 255, 255 };

        return pixelData;
    }
}

namespace Quiet::Renderer::Vulkan
{
    StaticMeshMaterialResources::StaticMeshMaterialResources(const PhysicalDevice &physicalDevice,
                                                             const LogicalDevice &logicalDevice,
                                                             const GraphicsCommands &graphicsCommands)
                                                                 : m_Device(logicalDevice.GetHandle())
    {
        VkDescriptorSetLayoutBinding textureBinding{};
        textureBinding.binding = 0;
        textureBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        textureBinding.descriptorCount = 1;
        textureBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        textureBinding.pImmutableSamplers = nullptr;

        VkDescriptorSetLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = 1;
        layoutInfo.pBindings = &textureBinding;

        if (vkCreateDescriptorSetLayout(m_Device, &layoutInfo, nullptr, &m_DescriptorSetLayout) != VK_SUCCESS)
            throw std::runtime_error("Failed to create static mesh material descriptor set layout.");

        CreateDescriptorPool();

        try
        {
            Assets::TexturePixelData fallbackPixel = CreateFallbackWhitePixel();

            m_FallbackWhiteTexture = std::make_unique<Texture2D>(
                physicalDevice,
                logicalDevice,
                graphicsCommands,
                fallbackPixel
            );
            m_FallbackDescriptorSet = AllocateDescriptorSetOrThrow(m_FallbackWhiteTexture->GetDescriptorImageInfo());
        }
        catch (...)
        {
            Destroy();
            throw;
        }

        QUIET_CORE_INFO("Created static mesh material resources.");
    }

    StaticMeshMaterialResources::~StaticMeshMaterialResources()
    {
        Destroy();
    }

    VkDescriptorSet StaticMeshMaterialResources::GetDescriptorSetForTexture(const TextureLibrary &textureLibrary,
                                                                            std::string_view textureAssetID)
    {
        if (textureAssetID.empty())
            return m_FallbackDescriptorSet;

        const std::string textureID(textureAssetID);
        if (const auto it = m_TextureDescriptorSets.find(textureID); it != m_TextureDescriptorSets.end())
            return it->second;

        const std::optional<VkDescriptorImageInfo> imageInfo = textureLibrary.TryGetDescriptorImageInfo(textureID);
        if (!imageInfo.has_value())
            return m_FallbackDescriptorSet;

        VkDescriptorSet descriptorSet = AllocateDescriptorSet(imageInfo.value());
        m_TextureDescriptorSets.emplace(textureID, descriptorSet);

        return descriptorSet;
    }

    void StaticMeshMaterialResources::ClearTextureDescriptorCache()
    {
        m_TextureDescriptorSets.clear();
        m_FallbackDescriptorSet = VK_NULL_HANDLE;

        DestroyDescriptorPool();
        CreateDescriptorPool();

        if (m_FallbackWhiteTexture)
            m_FallbackDescriptorSet = AllocateDescriptorSetOrThrow(m_FallbackWhiteTexture->GetDescriptorImageInfo());

        QUIET_CORE_INFO("Reset static mesh material descriptor cache.");
    }

    void StaticMeshMaterialResources::CreateDescriptorPool()
    {
        VkDescriptorPoolSize poolSize{};
        poolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        poolSize.descriptorCount = kMaxTextureDescriptorSets + 1;

        VkDescriptorPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.poolSizeCount = 1;
        poolInfo.pPoolSizes = &poolSize;
        poolInfo.maxSets = kMaxTextureDescriptorSets + 1;

        if (vkCreateDescriptorPool(m_Device, &poolInfo, nullptr, &m_DescriptorPool) != VK_SUCCESS)
        {
            Destroy();
            throw std::runtime_error("Failed to create static mesh material descriptor pool.");
        }
    }

    void StaticMeshMaterialResources::DestroyDescriptorPool()
    {
        if (m_DescriptorPool != VK_NULL_HANDLE)
        {
            vkDestroyDescriptorPool(m_Device, m_DescriptorPool, nullptr);
            m_DescriptorPool = VK_NULL_HANDLE;
        }
    }

    VkDescriptorSet StaticMeshMaterialResources::AllocateDescriptorSetOrThrow(const VkDescriptorImageInfo &imageInfo)
    {
        VkDescriptorSet descriptorSet = VK_NULL_HANDLE;

        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = m_DescriptorPool;
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts = &m_DescriptorSetLayout;

        if (vkAllocateDescriptorSets(m_Device, &allocInfo, &descriptorSet) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to allocate material texture descriptor set.");
        }

        WriteDescriptorSet(descriptorSet, imageInfo);
        return descriptorSet;
    }

    VkDescriptorSet StaticMeshMaterialResources::AllocateDescriptorSet(const VkDescriptorImageInfo &imageInfo)
    {
        VkDescriptorSet descriptorSet = VK_NULL_HANDLE;

        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = m_DescriptorPool;
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts = &m_DescriptorSetLayout;

        if (vkAllocateDescriptorSets(m_Device, &allocInfo, &descriptorSet) != VK_SUCCESS)
        {
            QUIET_CORE_WARN("Failed to allocate material texture descriptor set. Using fallback texture.");
            return m_FallbackDescriptorSet;
        }

        WriteDescriptorSet(descriptorSet, imageInfo);
        return descriptorSet;
    }

    void StaticMeshMaterialResources::WriteDescriptorSet(VkDescriptorSet descriptorSet,
                                                         const VkDescriptorImageInfo &imageInfo) const
    {
        VkWriteDescriptorSet write{};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet = descriptorSet;
        write.dstBinding = 0;
        write.dstArrayElement = 0;
        write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        write.descriptorCount = 1;
        write.pImageInfo = &imageInfo;

        vkUpdateDescriptorSets(m_Device, 1, &write, 0, nullptr);
    }

    void StaticMeshMaterialResources::Destroy()
    {
        m_TextureDescriptorSets.clear();

        m_FallbackDescriptorSet = VK_NULL_HANDLE;
        m_FallbackWhiteTexture.reset();

        DestroyDescriptorPool();

        if (m_DescriptorSetLayout != VK_NULL_HANDLE)
        {
            vkDestroyDescriptorSetLayout(m_Device, m_DescriptorSetLayout, nullptr);
            m_DescriptorSetLayout = VK_NULL_HANDLE;
        }
    }
}