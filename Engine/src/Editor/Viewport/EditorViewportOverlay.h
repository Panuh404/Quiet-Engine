#pragma once

#include <string>
#include <glm/vec3.hpp>
#include "imgui.h"

namespace Quiet::Scene
{
    struct CameraComponent;
    struct Actor;
    struct DirectionalLightComponent;
}

namespace Quiet::Editor
{
    struct EditorViewportState;
}

namespace Quiet::Renderer::Vulkan
{
    struct CameraUniformData;
    class StaticMesh;
}

namespace Quiet::Editor
{
    struct ScreenRect
    {
        ImVec2 Min{};
        ImVec2 Max{};
        bool Valid = false;
    };

    void DrawDebugLabel(ImDrawList* drawList, const ImVec2& position, const char* label, bool selected, bool hovered);
    void DrawCameraDebugIcon(ImDrawList* drawList, const ImVec2& position, bool selected, bool hovered);
    void DrawDirectionalLightDebugIcon(ImDrawList* drawList, const ImVec2& position, bool selected, bool hovered);
    void DrawHoveredBoundsOverlay(ImDrawList* drawList, const ScreenRect& rect);
    void DrawSelectedOriginMarker(ImDrawList* drawList, const ImVec2& position);
    void DrawSelectedActorName(ImDrawList* drawList, const ImVec2& position, const std::string& actorName);

    void DrawSelectedActorViewportOverlay(const Scene::Actor& actor,
                                          const Renderer::Vulkan::CameraUniformData& cameraData,
                                          const EditorViewportState& viewportState,
                                          ImDrawList* drawList);

    void DrawHoveredActorViewportOverlay(const Scene::Actor& actor,
                                         const Renderer::Vulkan::CameraUniformData& cameraData,
                                         const EditorViewportState& viewportState,
                                         ImDrawList* drawList);

    void DrawPhysicsColliderOverlay(const Scene::Actor& actor,
                                    const Renderer::Vulkan::CameraUniformData& cameraData,
                                    const EditorViewportState& viewportState,
                                    ImDrawList* drawList,
                                    bool selected);

    void DrawViewportMeshDropPreview(const Renderer::Vulkan::StaticMesh& mesh,
                                     const glm::vec3& worldPosition,
                                     const Renderer::Vulkan::CameraUniformData& cameraData,
                                     const EditorViewportState& viewportState,
                                     ImDrawList* drawList);

    bool DrawProjectedLine(ImDrawList* drawList,
                           const Renderer::Vulkan::CameraUniformData& cameraData,
                           const EditorViewportState& viewportState,
                           const glm::vec3& worldA,
                           const glm::vec3& worldB,
                           ImU32 color,
                           float thickness);

    void DrawScreenArrowHead(ImDrawList* drawList,
                             const ImVec2& start,
                             const ImVec2& end,
                             ImU32 color,
                             float thickness);

    void DrawCameraFrustumOverlay(ImDrawList* drawList,
                                  const Scene::Actor& actor,
                                  const Scene::CameraComponent& cameraComponent,
                                  const Renderer::Vulkan::CameraUniformData& cameraData,
                                  const EditorViewportState& viewportState,
                                  bool selected,
                                  bool hovered);

    void DrawDirectionalLightDirectionOverlay(ImDrawList* drawList,
                                              const Scene::Actor& actor,
                                              const Scene::DirectionalLightComponent&
                                              lightComponent,
                                              const Renderer::Vulkan::CameraUniformData& cameraData,
                                              const EditorViewportState& viewportState,
                                              bool selected,
                                              bool hovered    );

}
