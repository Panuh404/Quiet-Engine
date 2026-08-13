#pragma once

#include <cstdint>

#include <entt/entity/registry.hpp>
#include <vulkan/vulkan.h>

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
        const entt::registry* Registry = nullptr;
        entt::entity SelectedEntity = entt::null;

        VkExtent2D Extent{};
    };

    class SelectionMaskRenderSystem
    {
    public:
        SelectionMaskRenderSystem() = default;

        void Record(const SelectionMaskRenderContext& context) const;
    };
}