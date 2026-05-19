#pragma once

#include <vulkan/vulkan.h>

namespace Quiet::Renderer::Vulkan
{
    class LogicalDevice;

    class StaticMeshPipeline
    {
    public:
        StaticMeshPipeline(const LogicalDevice& logicalDevice,
                           VkFormat colorAttachmentFormat,
                           VkFormat depthAttachmentFormat,
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