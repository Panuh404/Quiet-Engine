#include "Editor/Viewport/EditorViewportPicking.h"

#include <limits>

#include "Editor/Viewport/EditorViewportProjection.h"
#include "Quiet/Math/Geometry.h"
#include "Renderer/Vulkan/Mesh/StaticMesh.h"
#include "Scene/Actor.h"
#include "Scene/Scene.h"

namespace
{
    float DistanceSquared(const ImVec2& a, const ImVec2& b)
    {
        const float dx = a.x - b.x;
        const float dy = a.y - b.y;
        return dx * dx + dy * dy;
    }

    bool IsPointInsideDebugIcon(const ImVec2& point, const ImVec2& iconPosition)
    {
        constexpr float pickRadius = 22.0f;
        return DistanceSquared(point, iconPosition) <= pickRadius * pickRadius;
    }

    void TrySetBestDebugIconHit(const Quiet::Core::UUID actorID,
                                const ImVec2& mousePosition,
                                const ImVec2& iconPosition,
                                std::optional<Quiet::Core::UUID>& bestActorID,
                                float& bestDistanceSquared)
    {
        if (!IsPointInsideDebugIcon(mousePosition, iconPosition)) return;

        const float distanceSquared = DistanceSquared(mousePosition, iconPosition);
        if (distanceSquared < bestDistanceSquared)
        {
            bestDistanceSquared = distanceSquared;
            bestActorID = actorID;
        }
    }
}

namespace Quiet::Editor
{
    std::optional<Core::UUID> PickViewportDebugIcon(const Scene::Scene& scene,
                                                    const Renderer::Vulkan::CameraUniformData& cameraData,
                                                    const EditorViewportState& viewportState,
                                                    const ImVec2& mousePosition)
    {
        std::optional<Core::UUID> bestActorID;
        float bestDistanceSquared = std::numeric_limits<float>::max();

        for (const Scene::Actor& actor : scene.GetActors())
        {
            const bool hasCamera = actor.HasComponent<Scene::CameraComponent>();
            const bool hasDirectionalLight = actor.HasComponent<Scene::DirectionalLightComponent>();

            if (!hasCamera && !hasDirectionalLight)
            {
                continue;
            }

            const Scene::TransformComponent& transform = actor.GetComponent<Scene::TransformComponent>();
            ImVec2 screenPosition{};

            if (!ProjectWorldToViewport(transform.WorldTransform.Position, cameraData, viewportState, screenPosition))
            {
                continue;
            }

            float verticalOffset = 0.0f;

            if (hasCamera)
            {
                const ImVec2 iconPosition(screenPosition.x, screenPosition.y + verticalOffset);
                TrySetBestDebugIconHit(actor.GetID(), mousePosition, iconPosition, bestActorID, bestDistanceSquared);
                verticalOffset += 22.0f;
            }

            if (hasDirectionalLight)
            {
                const ImVec2 iconPosition(screenPosition.x, screenPosition.y + verticalOffset);
                TrySetBestDebugIconHit(actor.GetID(), mousePosition, iconPosition, bestActorID, bestDistanceSquared);
            }
        }

        return bestActorID;
    }

    std::optional<Core::UUID> PickStaticMeshActor(const Scene::Scene& scene, const Math::Ray& ray)
    {
        float closestHit = std::numeric_limits<float>::max();
        std::optional<Core::UUID> bestHitID;

        for (const Scene::Actor& actor : scene.GetActors())
        {
            const Scene::StaticMeshComponent* staticMesh = actor.TryGetComponent<Scene::StaticMeshComponent>();
            if (!staticMesh || !staticMesh->IsValid() || !staticMesh->Visible)
            {
                continue;
            }

            const auto& mesh = staticMesh->Mesh;
            const glm::mat4 model = actor.GetComponent<Scene::TransformComponent>().WorldTransform.ToMatrix();

            const Math::AABB worldBounds = Math::TransformAABB(model, mesh->GetLocalBoundsMin(), mesh->GetLocalBoundsMax());
            float hitDistance = 0.0f;

            if (Math::IntersectRayAABB(ray, worldBounds, hitDistance) && hitDistance < closestHit)
            {
                closestHit = hitDistance;
                bestHitID = actor.GetID();
            }
        }

        return bestHitID;
    }
}
