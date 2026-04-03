#pragma once

#include <cstdint>

#include <vulkan/vulkan.h>

namespace Quiet::Scene
{
    class Scene;
}

namespace Quiet::Renderer::Vulkan
{
    class StaticMeshPipeline;
    class CameraResources;

    struct StaticMeshRenderContext
    {
        VkCommandBuffer CommandBuffer = VK_NULL_HANDLE;
        std::uint32_t ImageIndex = 0;
        const StaticMeshPipeline* Pipeline = nullptr;
        const CameraResources* Camera = nullptr;
        const Quiet::Scene::Scene* SceneData = nullptr;
        VkExtent2D Extent{};
    };

    class StaticMeshRenderSystem
    {
    public:
        StaticMeshRenderSystem() = default;

        void Record(const StaticMeshRenderContext& context) const;
    };
}