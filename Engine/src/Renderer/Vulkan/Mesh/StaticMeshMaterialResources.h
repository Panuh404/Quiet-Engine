#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>

#include <vulkan/vulkan.h>

namespace Quiet::Renderer::Vulkan
{
    class PhysicalDevice;
    class LogicalDevice;
    class GraphicsCommands;
    class Texture2D;
    class TextureLibrary;
    class  StaticMeshMaterialResources
    {
    public:
        StaticMeshMaterialResources(const PhysicalDevice& physicalDevice,
                                    const LogicalDevice& logicalDevice,
                                    const GraphicsCommands& graphicsCommands);
        ~StaticMeshMaterialResources();

        StaticMeshMaterialResources(const StaticMeshMaterialResources&) = delete;
        StaticMeshMaterialResources& operator=(const StaticMeshMaterialResources&) = delete;
        StaticMeshMaterialResources(StaticMeshMaterialResources&&) = delete;
        StaticMeshMaterialResources& operator=(StaticMeshMaterialResources&&) = delete;

        [[nodiscard]] VkDescriptorSetLayout GetDescriptorSetLayout() const { return m_DescriptorSetLayout; }
        [[nodiscard]] VkDescriptorSet GetFallbackDescriptorSet() const { return m_FallbackDescriptorSet; }
        [[nodiscard]] VkDescriptorSet GetDescriptorSetForTexture(const TextureLibrary& textureLibrary, std::string_view textureAssetID);

        void ClearTextureDescriptorCache();

    private:
        void CreateDescriptorPool();
        void DestroyDescriptorPool();

        [[nodiscard]] VkDescriptorSet AllocateDescriptorSet(const VkDescriptorImageInfo& imageInfo);
        [[nodiscard]] VkDescriptorSet AllocateDescriptorSetOrThrow(const VkDescriptorImageInfo& imageInfo);

        void WriteDescriptorSet(VkDescriptorSet descriptorSet, const VkDescriptorImageInfo& imageInfo) const;
        void Destroy();

    private:
        static constexpr std::uint32_t kMaxTextureDescriptorSets = 1024;

        VkDevice m_Device = VK_NULL_HANDLE;
        VkDescriptorSetLayout m_DescriptorSetLayout = VK_NULL_HANDLE;
        VkDescriptorPool m_DescriptorPool = VK_NULL_HANDLE;

        std::unique_ptr<Texture2D> m_FallbackWhiteTexture;
        VkDescriptorSet m_FallbackDescriptorSet = VK_NULL_HANDLE;

        std::unordered_map<std::string, VkDescriptorSet> m_TextureDescriptorSets;
    };
}