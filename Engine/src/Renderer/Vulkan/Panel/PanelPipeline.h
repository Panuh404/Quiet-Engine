#pragma once

#include <vulkan/vulkan.h>

namespace Quiet::Renderer::Vulkan
{
    class LogicalDevice;

    /**
     * @brief Owns the graphics pipeline for solid screen-space panels.
     * @ingroup quiet_renderer
     *
     * The logical device must outlive this object.
     * GPU use must finish before destruction.
     */
    class PanelPipeline
    {
    public:
        /**
         * @brief Creates a color-only dynamic-rendering panel pipeline.
         * @param logicalDevice Device that owns the pipeline resources.
         * @param colorAttachmentFormat Scene target color format.
         * @note Construction failures release resources already created.
         */
        PanelPipeline(const LogicalDevice& logicalDevice, VkFormat colorAttachmentFormat);

        ~PanelPipeline();

        PanelPipeline(const PanelPipeline&) = delete;
        PanelPipeline& operator=(const PanelPipeline&) = delete;
        PanelPipeline(PanelPipeline&&) = delete;
        PanelPipeline& operator=(PanelPipeline&&) = delete;

        [[nodiscard]] VkPipeline GetHandle() const { return m_Pipeline; }
        [[nodiscard]] VkPipelineLayout GetLayout() const { return m_PipelineLayout; }

    private:
        void Destroy() noexcept;

        VkDevice m_Device = VK_NULL_HANDLE;
        VkPipelineLayout m_PipelineLayout = VK_NULL_HANDLE;
        VkPipeline m_Pipeline = VK_NULL_HANDLE;
    };
}