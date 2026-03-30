#pragma once

#include <vulkan/vulkan.h>

namespace Quiet::Renderer::Vulkan
{
    class LogicalDevice;
    class Swapchain;
    class DepthResources;

    class TrianglePipeline
    {
    public:
        TrianglePipeline(const LogicalDevice& logicalDevice,
                         const Swapchain& swapchain,
                         const DepthResources& depthResources);
        ~TrianglePipeline();

        TrianglePipeline(const TrianglePipeline&) = delete;
        TrianglePipeline& operator=(const TrianglePipeline&) = delete;
        TrianglePipeline(TrianglePipeline&&) = delete;
        TrianglePipeline& operator=(TrianglePipeline&&) = delete;

        [[nodiscard]] VkPipelineLayout GetLayout() const { return m_PipelineLayout; }
        [[nodiscard]] VkPipeline GetHandle() const { return m_Pipeline; }

    private:
        VkDevice m_Device = VK_NULL_HANDLE;
        VkPipelineLayout m_PipelineLayout = VK_NULL_HANDLE;
        VkPipeline m_Pipeline = VK_NULL_HANDLE;
    };
}