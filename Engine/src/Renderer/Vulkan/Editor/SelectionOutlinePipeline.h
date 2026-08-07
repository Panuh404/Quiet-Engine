#pragma once

#include <glm/vec4.hpp>
#include <vulkan/vulkan.h>

namespace Quiet::Renderer::Vulkan
{
    class LogicalDevice;

    struct SelectionOutlinePushConstants
    {
        alignas(16) glm::vec4 OutlineColor
        {
            0.180f,
            0.480f,
            0.950f,
            1.000f
        };

        alignas(16) glm::vec4 Parameters
        {
            2.0f,
            0.0f,
            0.0f,
            0.0f
        };
    };

    class SelectionOutlinePipeline
    {
    public:
        SelectionOutlinePipeline(const LogicalDevice& logicalDevice, VkFormat colorAttachmentFormat);
        ~SelectionOutlinePipeline();

        SelectionOutlinePipeline(const SelectionOutlinePipeline&) = delete;
        SelectionOutlinePipeline& operator=(const SelectionOutlinePipeline&) = delete;
        SelectionOutlinePipeline(SelectionOutlinePipeline&&) = delete;
        SelectionOutlinePipeline& operator=(SelectionOutlinePipeline&&) = delete;

        void UpdateMaskImage(VkSampler sampler, VkImageView imageView);

        [[nodiscard]] bool IsMaskImageReady() const { return m_MaskImageReady; }
        [[nodiscard]] VkPipelineLayout GetLayout() const { return m_PipelineLayout; }
        [[nodiscard]] VkPipeline GetHandle() const { return m_Pipeline; }
        [[nodiscard]] VkDescriptorSet GetDescriptorSet() const { return m_DescriptorSet; }

    private:
        VkDevice m_Device = VK_NULL_HANDLE;
        VkDescriptorSetLayout m_DescriptorSetLayout = VK_NULL_HANDLE;
        VkDescriptorPool m_DescriptorPool = VK_NULL_HANDLE;
        VkDescriptorSet m_DescriptorSet = VK_NULL_HANDLE;
        VkPipelineLayout m_PipelineLayout = VK_NULL_HANDLE;
        VkPipeline m_Pipeline = VK_NULL_HANDLE;

        bool m_MaskImageReady = false;
    };
}