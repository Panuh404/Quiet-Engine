#include "Editor/Viewport/EditorViewportProjection.h"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/vec3.hpp>

#include <cmath>

#include "Quiet/Editor/EditorState.h"
#include "Quiet/Renderer/RenderConventions.h"
#include "Renderer/Vulkan/RenderTypes.h"
#include "Renderer/Vulkan/VulkanRenderer.h"
#include "Renderer/Vulkan/Mesh/StaticMesh.h"
#include "Scene/Camera/FreeCamera.h"

namespace Quiet::Editor
{
    bool ProjectWorldToViewport(const glm::vec3& worldPosition,
                                const Renderer::Vulkan::CameraUniformData& cameraData,
                                const EditorViewportState& viewportState,
                                ImVec2& outScreenPosition)
    {
        if (viewportState.Width == 0 || viewportState.Height == 0) return false;

        const glm::vec4 clip = cameraData.Projection * cameraData.View * glm::vec4(worldPosition, 1.0f);
        if (clip.w <= 0.0001f) return false;

        const glm::vec3 ndc = glm::vec3(clip) / clip.w;
        if (ndc.z < 0.0f || ndc.z > 1.0f) return false;

        const float viewportWidth = static_cast<float>(viewportState.Width);
        const float viewportHeight = static_cast<float>(viewportState.Height);

        outScreenPosition.x = viewportState.ScreenX + ((ndc.x * 0.5f) + 0.5f) * viewportWidth;
        outScreenPosition.y = viewportState.ScreenY + (1.0f - ((ndc.y * 0.5f) + 0.5f)) * viewportHeight;

        return true;
    }

    bool BuildViewportPickRay(float mouseScreenX,
                              float mouseScreenY,
                              const EditorViewportState& viewportState,
                              const Scene::FreeCamera& camera,
                              const Renderer::Vulkan::VulkanRenderer& renderer,
                              Math::Ray& outRay)
    {
        const VkExtent2D sceneExtent = renderer.GetSceneViewportExtent();
        if (sceneExtent.width == 0 || sceneExtent.height == 0) return false;
        if (viewportState.Width == 0 || viewportState.Height == 0) return false;

        const float localX = mouseScreenX - viewportState.ScreenX;
        const float localY = mouseScreenY - viewportState.ScreenY;

        if (localX < 0.0f || localY < 0.0f ||
            localX > static_cast<float>(viewportState.Width) ||
            localY > static_cast<float>(viewportState.Height))
            return false;

        const auto viewportHeight = static_cast<float>(viewportState.Height);
        const float flippedY = viewportHeight - localY;

        const float aspect =
            static_cast<float>(sceneExtent.width) /
            static_cast<float>(sceneExtent.height);

        const auto cameraData =
            camera.BuildUniformData(aspect, Renderer::UseEditorScreenProjectionYFlip);

        const glm::vec4 viewport(
            0.0f,
            0.0f,
            static_cast<float>(viewportState.Width),
            static_cast<float>(viewportState.Height)
        );

        const glm::vec3 nearPoint = glm::unProjectZO(
            glm::vec3(localX, flippedY, 0.0f),
            cameraData.View,
            cameraData.Projection,
            viewport
        );

        const glm::vec3 farPoint = glm::unProjectZO(
            glm::vec3(localX, flippedY, 1.0f),
            cameraData.View,
            cameraData.Projection,
            viewport
        );

        const glm::vec3 direction = farPoint - nearPoint;
        if (glm::length(direction) <= 0.00001f) return false;

        outRay.Origin = nearPoint;
        outRay.Direction = glm::normalize(direction);
        return true;
    }

    glm::vec3 CalculateViewportMeshPlacementPosition(const Math::Ray &ray, const Renderer::Vulkan::StaticMesh &mesh)
    {
        constexpr float fallbackDistance = 5.0f;
        constexpr float minimumHitDistance = 0.05f;
        constexpr float maximumGroundDistance = 1000.0f;

        glm::vec3 position = ray.Origin + ray.Direction * fallbackDistance;

        if (std::abs(ray.Direction.y) <= 0.00001f)
        {
            return position;
        }

        const float groundHitDistance = -ray.Origin.y / ray.Direction.y;
        if (groundHitDistance < minimumHitDistance ||
            groundHitDistance > maximumGroundDistance)
        {
            return position;
        }

        position = ray.Origin + ray.Direction * groundHitDistance;

        // The actor is created with an identity transform. Move its
        // lowest local-space point onto the world ground plane.
        position.y -= mesh.GetLocalBoundsMin().y;
        return position;
    }
}
