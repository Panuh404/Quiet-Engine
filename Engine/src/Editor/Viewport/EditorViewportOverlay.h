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
}


namespace Quiet::Editor
{
    struct ScreenRect
    {
        ImVec2 Min{};
        ImVec2 Max{};
        bool Valid = false;
    };

    void DrawDebugLabel(ImDrawList* drawList, const ImVec2& position, const char* label, bool selected);
    void DrawCameraDebugIcon(ImDrawList* drawList, const ImVec2& position, bool selected);
    void DrawDirectionalLightDebugIcon(ImDrawList* drawList, const ImVec2& position, bool selected);

    void DrawSelectedBoundsOverlay(ImDrawList* drawList, const ScreenRect& rect);
    void DrawSelectedOriginMarker(ImDrawList* drawList, const ImVec2& position);
    void DrawSelectedActorName(ImDrawList* drawList, const ImVec2& position, const std::string& actorName);

    void DrawSelectedActorViewportOverlay(const Scene::Actor& actor,
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
                                  bool selected);

    void DrawDirectionalLightDirectionOverlay(ImDrawList* drawList,
                                              const Scene::Actor& actor,
                                              const Scene::DirectionalLightComponent& lightComponent,
                                              const Renderer::Vulkan::CameraUniformData& cameraData,
                                              const EditorViewportState& viewportState,
                                              bool selected);

}
