#pragma once

#include <cstdint>
#include <vulkan/vulkan.h>

namespace Quiet::Scene
{
    struct Actor;
}

namespace Quiet::Renderer::Vulkan
{
    class CameraResources;
    class SelectionMaskPipeline;

    struct SelectionMaskRenderContext
    {
        VkCommandBuffer CommandBuffer = VK_NULL_HANDLE;
        std::uint32_t ImageIndex = 0;

        const SelectionMaskPipeline* Pipeline = nullptr;
        const CameraResources* Camera = nullptr;
        const Scene::Actor* SelectedActor = nullptr;

        VkExtent2D Extent{};
    };

    class SelectionMaskRenderSystem
    {
    public:
        SelectionMaskRenderSystem() = default;

        void Record(const SelectionMaskRenderContext& context) const;
    };
}