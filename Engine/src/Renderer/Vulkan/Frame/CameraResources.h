#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include <vulkan/vulkan.h>

namespace Quiet::Renderer::Vulkan
{
    class PhysicalDevice;
    class LogicalDevice;
    class Swapchain;
    class VulkanBuffer;
    struct CameraUniformData;

    class CameraResources
    {
    public:
        CameraResources(const PhysicalDevice& physicalDevice,
                        const LogicalDevice& logicalDevice,
                        const Swapchain& swapchain);
        ~CameraResources();

        CameraResources(const CameraResources&) = delete;
        CameraResources& operator=(const CameraResources&) = delete;
        CameraResources(CameraResources&&) = delete;
        CameraResources& operator=(CameraResources&&) = delete;

        VkDescriptorSetLayout GetDescriptorSetLayout() const { return m_DescriptorSetLayout; }
        VkDescriptorSet GetDescriptorSet(std::uint32_t imageIndex) const { return m_DescriptorSets.at(imageIndex); }

        void Update(std::uint32_t imageIndex, const CameraUniformData& data);

    private:
        VkDevice m_Device = VK_NULL_HANDLE;
        VkDescriptorSetLayout m_DescriptorSetLayout = VK_NULL_HANDLE;
        VkDescriptorPool m_DescriptorPool = VK_NULL_HANDLE;

        std::vector<std::unique_ptr<VulkanBuffer>> m_UniformBuffers;
        std::vector<void*> m_MappedUniformBuffers;
        std::vector<VkDescriptorSet> m_DescriptorSets;
    };
}