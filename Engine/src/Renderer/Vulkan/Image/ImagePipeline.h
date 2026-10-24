#pragma once

#include <vulkan/vulkan.h>

namespace Quiet::Renderer::Vulkan
{
    class LogicalDevice;

    /**
     * @brief Owns graphics state and descriptor layout for textured UI rectangles.
     * @ingroup quiet_renderer
     *
     * The logical device must outlive this object.
     * GPU use must finish before destruction.
     * Descriptor sets and sampled textures are owned externally.
     */
    class ImagePipeline
    {
    public:
        /**
         * @brief Creates a color-only dynamic-rendering image pipeline.
         * @param logicalDevice Device that owns the pipeline resources.
         * @param colorAttachmentFormat Scene target color format.
         * @note Construction failures release resources already created.
         */
        ImagePipeline(const LogicalDevice& logicalDevice, VkFormat colorAttachmentFormat);
        ~ImagePipeline();

        ImagePipeline(const ImagePipeline&) = delete;
        ImagePipeline& operator=(const ImagePipeline&) = delete;
        ImagePipeline(ImagePipeline&&) = delete;
        ImagePipeline& operator=(ImagePipeline&&) = delete;

        [[nodiscard]] VkPipeline GetHandle() const { return m_Pipeline; }
        [[nodiscard]] VkPipelineLayout GetLayout() const { return m_PipelineLayout; }
        [[nodiscard]] VkDescriptorSetLayout GetDescriptorSetLayout() const { return m_DescriptorSetLayout; }

    private:
        void Destroy() noexcept;

        VkDevice m_Device = VK_NULL_HANDLE;
        VkDescriptorSetLayout m_DescriptorSetLayout = VK_NULL_HANDLE;
        VkPipelineLayout m_PipelineLayout = VK_NULL_HANDLE;
        VkPipeline m_Pipeline = VK_NULL_HANDLE;
    };
}