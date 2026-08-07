#pragma once

#include <glm/mat4x4.hpp>
#include <vulkan/vulkan.h>


namespace Quiet::Renderer::Vulkan
{
    class LogicalDevice;

    struct SelectionMaskPushConstants
    {
        alignas(16) glm::mat4 Model{ 1.0f };
    };

    class SelectionMaskPipeline
    {
    public:
        SelectionMaskPipeline(const LogicalDevice& logicalDevice,
                              VkFormat colorAttachmentFormat,
                              VkFormat depthAttachmentFormat,
                              VkDescriptorSetLayout cameraSetLayout);

        ~SelectionMaskPipeline();

        SelectionMaskPipeline(const SelectionMaskPipeline&) = delete;
        SelectionMaskPipeline& operator=(const SelectionMaskPipeline&) = delete;
        SelectionMaskPipeline(SelectionMaskPipeline&&) = delete;
        SelectionMaskPipeline& operator=(SelectionMaskPipeline&&) = delete;

        [[nodiscard]] VkPipelineLayout GetLayout() const { return m_PipelineLayout; }
        [[nodiscard]] VkPipeline GetHandle() const { return m_Pipeline; }

    private:
        VkDevice m_Device = VK_NULL_HANDLE;
        VkPipelineLayout m_PipelineLayout = VK_NULL_HANDLE;
        VkPipeline m_Pipeline = VK_NULL_HANDLE;
    };
}
