#pragma once

#include <vulkan/vulkan.h>

namespace Quiet::Renderer::Vulkan
{
    class LogicalDevice;
    class Texture2D;

    /**
     * @brief Owns the graphics pipeline and descriptors for font coverage.
     * @ingroup quiet_renderer
     *
     * The logical device and supplied font texture must outlive this object.
     * GPU use must finish before destruction.
     */
    class TextPipeline
    {
    public:
        /**
         * @brief Creates a color-only dynamic-rendering text pipeline.
         * @param logicalDevice Device that owns the pipeline resources.
         * @param colorAttachmentFormat Scene target color format.
         * @param fontTexture R8_UNORM coverage texture retained externally.
         * @note Construction failures release resources already created.
         */
        TextPipeline(const LogicalDevice& logicalDevice, VkFormat colorAttachmentFormat, const Texture2D& fontTexture);
        ~TextPipeline();

        TextPipeline(const TextPipeline&) = delete;
        TextPipeline& operator=(const TextPipeline&) = delete;
        TextPipeline(TextPipeline&&) = delete;
        TextPipeline& operator=(TextPipeline&&) = delete;

        [[nodiscard]] VkPipeline GetHandle() const { return m_Pipeline; }
        [[nodiscard]] VkPipelineLayout GetLayout() const { return m_PipelineLayout; }
        [[nodiscard]] VkDescriptorSet GetDescriptorSet() const { return m_DescriptorSet; }

    private:
        void Destroy() noexcept;

        VkDevice m_Device = VK_NULL_HANDLE;
        VkDescriptorSetLayout m_DescriptorSetLayout = VK_NULL_HANDLE;
        VkDescriptorPool m_DescriptorPool = VK_NULL_HANDLE;
        VkDescriptorSet m_DescriptorSet = VK_NULL_HANDLE;
        VkPipelineLayout m_PipelineLayout = VK_NULL_HANDLE;
        VkPipeline m_Pipeline = VK_NULL_HANDLE;
    };
}