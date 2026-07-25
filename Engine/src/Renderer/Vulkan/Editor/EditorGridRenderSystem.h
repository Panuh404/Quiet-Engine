#pragma once

#include <cstdint>

#include <vulkan/vulkan.h>

namespace Quiet::Renderer::Vulkan
{
    class CameraResources;
    class EditorGridPipeline;

    struct EditorGridRenderContext
    {
        VkCommandBuffer CommandBuffer = VK_NULL_HANDLE;
        std::uint32_t ImageIndex = 0;
        const EditorGridPipeline* Pipeline = nullptr;
        const CameraResources* Camera = nullptr;
        VkExtent2D Extent{};
    };

    class EditorGridRenderSystem
    {
    public:
        EditorGridRenderSystem() = default;

        void Record(const EditorGridRenderContext& context) const;
    };
}