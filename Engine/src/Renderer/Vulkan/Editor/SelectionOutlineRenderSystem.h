#pragma once

#include <vulkan/vulkan.h>

namespace Quiet::Renderer::Vulkan
{
    class SelectionOutlinePipeline;

    struct SelectionOutlineRenderContext
    {
        VkCommandBuffer CommandBuffer = VK_NULL_HANDLE;
        const SelectionOutlinePipeline* Pipeline = nullptr;
        VkExtent2D Extent{};
    };

    class SelectionOutlineRenderSystem
    {
    public:
        SelectionOutlineRenderSystem() = default;
        
        void Record(const SelectionOutlineRenderContext& context) const;
    };
}