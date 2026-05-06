#pragma once

#include <vulkan/vulkan.h>

namespace Quiet::Renderer::Vulkan
{
    class LogicalDevice;
    class Swapchain;
    class DepthResources;

    class StaticMeshPipeline
    {
    public:
        StaticMeshPipeline(const LogicalDevice& logicalDevice,
                           const Swapchain& swapchain,
                           const DepthResources& depthResources,
                           VkDescriptorSetLayout cameraSetLayout,
                           VkDescriptorSetLayout materialSetLayout);
        ~StaticMeshPipeline();

        StaticMeshPipeline(const StaticMeshPipeline&) = delete;
        StaticMeshPipeline& operator=(const StaticMeshPipeline&) = delete;
        StaticMeshPipeline(StaticMeshPipeline&&) = delete;
        StaticMeshPipeline& operator=(StaticMeshPipeline&&) = delete;

        [[nodiscard]] VkPipelineLayout GetLayout() const { return m_PipelineLayout; }
        [[nodiscard]] VkPipeline GetHandle() const { return m_Pipeline; }

    private:
        VkDevice m_Device = VK_NULL_HANDLE;
        VkPipelineLayout m_PipelineLayout = VK_NULL_HANDLE;
        VkPipeline m_Pipeline = VK_NULL_HANDLE;
    };
}