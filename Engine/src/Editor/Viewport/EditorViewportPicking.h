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
    std::optional<Core::UUID> PickViewportDebugIcon(const Scene::Scene& scene,
                                                    const Renderer::Vulkan::CameraUniformData& cameraData,
                                                    const EditorViewportState& viewportState,
                                                    const ImVec2& mousePosition);

    std::optional<Core::UUID> PickStaticMeshActor(const Scene::Scene& scene,
                                                  const Math::Ray& ray);
}