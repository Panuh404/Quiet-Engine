#pragma once

#include <optional>

#include "imgui.h"
#include "Quiet/Core/UUID.h"

namespace Quiet::Scene
{
    class Scene;
}

namespace Quiet::Editor
{
    struct EditorViewportState;
}

namespace Quiet::Renderer::Vulkan
{
    struct CameraUniformData;
}

namespace Quiet::Math
{
    struct Ray;
}

namespace Quiet::Editor
{
    float DistanceSquared(const ImVec2& a, const ImVec2& b);

    bool IsPointInsideDebugIcon(const ImVec2& point, const ImVec2& iconPosition);

    void TrySetBestDebugIconHit(const Core::UUID actorID,
                                const ImVec2& mousePosition,
                                const ImVec2& iconPosition,
                                std::optional<Core::UUID>& bestActorID,
                                float& bestDistanceSquared);

    std::optional<Core::UUID> PickViewportDebugIcon(const Scene::Scene& scene,
                                                    const Renderer::Vulkan::CameraUniformData& cameraData,
                                                    const EditorViewportState& viewportState,
                                                    const ImVec2& mousePosition);

    std::optional<Core::UUID> PickStaticMeshActor(const Scene::Scene& scene,
                                                  const Math::Ray& ray);
}