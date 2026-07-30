#pragma once

#include <glm/vec4.hpp>
#include <vulkan/vulkan.h>

namespace Quiet::Renderer::Vulkan
{
    class LogicalDevice;

    struct EditorGridPushConstants
    {
        alignas(16) glm::vec4 MinorColor{};
        alignas(16) glm::vec4 MajorColor{};
        alignas(16) glm::vec4 XAxisColor{};
        alignas(16) glm::vec4 ZAxisColor{};

        // x = base minor spacing
        // y = camera-height LOD reference
        // z = base fade-start distance
        // w = base fade-end distance
        alignas(16) glm::vec4 Parameters{};
    };

    class EditorGridPipeline
    {
    public:
        EditorGridPipeline(const LogicalDevice& logicalDevice,
                           VkFormat colorAttachmentFormat,
                           VkFormat depthAttachmentFormat,
                           VkDescriptorSetLayout cameraSetLayout);
        ~EditorGridPipeline();

        EditorGridPipeline(const EditorGridPipeline&) = delete;
        EditorGridPipeline& operator=(const EditorGridPipeline&) = delete;
        EditorGridPipeline(EditorGridPipeline&&) = delete;
        EditorGridPipeline& operator=(EditorGridPipeline&&) = delete;

        [[nodiscard]] VkPipelineLayout GetLayout() const { return m_PipelineLayout;}
        [[nodiscard]] VkPipeline GetHandle() const { return m_Pipeline; }

    private:
        VkDevice m_Device = VK_NULL_HANDLE;
        VkPipelineLayout m_PipelineLayout = VK_NULL_HANDLE;
        VkPipeline m_Pipeline = VK_NULL_HANDLE;
    };
}