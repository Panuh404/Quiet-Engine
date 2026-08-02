#pragma once

#include <glm/vec3.hpp>
#include "imgui.h"
#include "Quiet/Math/Geometry.h"

namespace Quiet::Scene
{
    class FreeCamera;
    class Scene;
}

namespace Quiet::Editor
{
    struct EditorViewportState;
}

namespace Quiet::Renderer::Vulkan
{
    struct CameraUniformData;
    class StaticMesh;
    class VulkanRenderer;
}


namespace Quiet::Editor
{
    bool ProjectWorldToViewport(const glm::vec3& worldPosition,
                                const Renderer::Vulkan::CameraUniformData& cameraData,
                                const EditorViewportState& viewportState,
                                ImVec2& outScreenPosition);

    [[nodiscard]] bool BuildViewportPickRay(float mouseScreenX,
                                            float mouseScreenY,
                                            const EditorViewportState& viewportState,
                                            const Scene::FreeCamera& camera,
                                            const Renderer::Vulkan::VulkanRenderer& renderer,
                                            Math::Ray& outRay);

    [[nodiscard]] glm::vec3 CalculateViewportMeshPlacementPosition(const Math::Ray& ray, const Renderer::Vulkan::StaticMesh& mesh);

}