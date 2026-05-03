#include "Editor/Viewport/EditorViewportOverlay.h"

#include <algorithm>
#include <array>
#include <cmath>

#include <glm/vec3.hpp>
#include <glm/geometric.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/trigonometric.hpp>

#include "Editor/Viewport/EditorViewportProjection.h"
#include "Quiet/Editor/EditorState.h"
#include "Renderer/Vulkan/Mesh/StaticMesh.h"
#include "Scene/Actor.h"
#include "Scene/Components/CameraComponent.h"
#include "Scene/Components/DirectionalLightComponent.h"

namespace
{
    void ExpandScreenRect(Quiet::Editor::ScreenRect& rect, const ImVec2& point)
    {
        if (!rect.Valid)
        {
            rect.Min = point;
            rect.Max = point;
            rect.Valid = true;
            return;
        }

        rect.Min.x = std::min(rect.Min.x, point.x);
        rect.Min.y = std::min(rect.Min.y, point.y);
        rect.Max.x = std::max(rect.Max.x, point.x);
        rect.Max.y = std::max(rect.Max.y, point.y);
    }

    bool ProjectStaticMeshBoundsToViewport(const Quiet::Scene::Actor& actor,
                                           const Quiet::Renderer::Vulkan::CameraUniformData& cameraData,
                                           const Quiet::Editor::EditorViewportState& viewportState,
                                           Quiet::Editor::ScreenRect& outRect)
    {
        if (!actor.StaticMesh.has_value() ||
            !actor.StaticMesh->IsValid() ||
            !actor.StaticMesh->Visible)
            return false;

        const auto& mesh = actor.StaticMesh->Mesh;
        const glm::mat4 model = actor.Transform.WorldTransform.ToMatrix();

        const auto corners = Quiet::Math::BuildAABBCorners(
            mesh->GetLocalBoundsMin(),
            mesh->GetLocalBoundsMax()
        );

        Quiet::Editor::ScreenRect rect{};
        for (const glm::vec3& corner : corners)
        {
            const glm::vec3 worldCorner = glm::vec3(model * glm::vec4(corner, 1.0f));
            ImVec2 screenPoint{};
            if (Quiet::Editor::ProjectWorldToViewport(
                    worldCorner,
                    cameraData,
                    viewportState,
                    screenPoint))
            {
                ExpandScreenRect(rect, screenPoint);
            }
        }

        if (!rect.Valid) return false;
        outRect = rect;
        return true;
    }
}

namespace Quiet::Editor
{
    void DrawDebugLabel(ImDrawList* drawList, const ImVec2& position, const char* label, bool selected)
    {
        const ImU32 textColor = selected
            ? IM_COL32(255, 255, 255, 255)
            : IM_COL32(210, 220, 230, 230);

        drawList->AddText(
            ImVec2(position.x + 14.0f, position.y - 8.0f),
            textColor,
            label
        );
    }

    void DrawCameraDebugIcon(ImDrawList* drawList, const ImVec2& position, bool selected)
    {
        const ImU32 color = selected
            ? IM_COL32(255, 255, 255, 255)
            : IM_COL32(80, 190, 255, 230);

        const ImU32 fill = selected
            ? IM_COL32(80, 190, 255, 90)
            : IM_COL32(80, 190, 255, 45);

        constexpr float halfWidth = 9.0f;
        constexpr float halfHeight = 6.0f;

        const ImVec2 min(position.x - halfWidth, position.y - halfHeight);
        const ImVec2 max(position.x + halfWidth, position.y + halfHeight);

        drawList->AddRectFilled(min, max, fill, 2.0f);
        drawList->AddRect(min, max, color, 2.0f, 0, 2.0f);

        drawList->AddTriangle(
            ImVec2(max.x, position.y - 4.0f),
            ImVec2(max.x + 8.0f, position.y - 8.0f),
            ImVec2(max.x + 8.0f, position.y + 8.0f),
            color,
            2.0f
        );
    }

    void DrawDirectionalLightDebugIcon(ImDrawList* drawList, const ImVec2& position, bool selected)
    {
        const ImU32 color = selected
            ? IM_COL32(255, 255, 255, 255)
            : IM_COL32(255, 220, 80, 235);

        const ImU32 fill = selected
            ? IM_COL32(255, 220, 80, 100)
            : IM_COL32(255, 220, 80, 55);

        drawList->AddCircleFilled(position, 7.0f, fill, 20);
        drawList->AddCircle(position, 7.0f, color, 20, 2.0f);

        drawList->AddLine(
            ImVec2(position.x - 14.0f, position.y - 14.0f),
            ImVec2(position.x - 5.0f, position.y - 5.0f),
            color,
            2.0f
        );

        drawList->AddLine(
            ImVec2(position.x + 5.0f, position.y + 5.0f),
            ImVec2(position.x + 14.0f, position.y + 14.0f),
            color,
            2.0f
        );

        drawList->AddLine(
            ImVec2(position.x + 14.0f, position.y - 14.0f),
            ImVec2(position.x + 5.0f, position.y - 5.0f),
            color,
            2.0f
        );

        drawList->AddLine(
            ImVec2(position.x - 5.0f, position.y + 5.0f),
            ImVec2(position.x - 14.0f, position.y + 14.0f),
            color,
            2.0f
        );
    }

    void DrawSelectedBoundsOverlay(ImDrawList* drawList, const ScreenRect& rect)
    {
        constexpr ImU32 color = IM_COL32(255, 210, 70, 255);
        constexpr ImU32 shadow = IM_COL32(0, 0, 0, 170);

        constexpr float padding = 4.0f;

        const ImVec2 min(rect.Min.x - padding, rect.Min.y - padding);
        const ImVec2 max(rect.Max.x + padding, rect.Max.y + padding);

        drawList->AddRect(
            ImVec2(min.x + 1.0f, min.y + 1.0f),
            ImVec2(max.x + 1.0f, max.y + 1.0f),
            shadow,
            2.0f,
            0,
            3.0f
        );

        drawList->AddRect(
            min,
            max,
            color,
            2.0f,
            0,
            2.0f
        );
    }

    void DrawSelectedOriginMarker(ImDrawList* drawList, const ImVec2& position)
    {
        constexpr ImU32 color = IM_COL32(255, 210, 70, 255);
        constexpr ImU32 shadow = IM_COL32(0, 0, 0, 170);

        drawList->AddCircleFilled(
            ImVec2(position.x + 1.0f, position.y + 1.0f),
            5.0f,
            shadow,
            16
        );

        drawList->AddCircleFilled(
            position,
            5.0f,
            color,
            16
        );

        drawList->AddCircle(
            position,
            9.0f,
            color,
            20,
            2.0f
        );
    }

    void DrawSelectedActorName(ImDrawList* drawList, const ImVec2& position, const std::string& actorName)
    {
        if (actorName.empty()) return;

        constexpr ImU32 textColor = IM_COL32(255, 255, 255, 255);
        constexpr ImU32 shadow = IM_COL32(0, 0, 0, 190);
        const ImVec2 textPosition(position.x + 12.0f, position.y + 10.0f);

        drawList->AddText(
            ImVec2(textPosition.x + 1.0f, textPosition.y + 1.0f),
            shadow,
            actorName.c_str()
        );

        drawList->AddText(
            textPosition,
            textColor,
            actorName.c_str()
        );
    }

    void DrawSelectedActorViewportOverlay(const Scene::Actor& actor,
                                          const Renderer::Vulkan::CameraUniformData& cameraData,
                                          const EditorViewportState& viewportState,
                                          ImDrawList* drawList)
    {
        ImVec2 actorScreenPosition{};
        if (!ProjectWorldToViewport(
                actor.Transform.WorldTransform.Position,
                cameraData,
                viewportState,
                actorScreenPosition))
            return;

        ScreenRect boundsRect{};
        if (ProjectStaticMeshBoundsToViewport(
                actor,
                cameraData,
                viewportState,
                boundsRect))
        {
            DrawSelectedBoundsOverlay(drawList, boundsRect);
        }

        DrawSelectedOriginMarker(drawList, actorScreenPosition);
        DrawSelectedActorName(drawList, actorScreenPosition, actor.Name);
    }

    bool DrawProjectedLine(ImDrawList* drawList,
                           const Quiet::Renderer::Vulkan::CameraUniformData& cameraData,
                           const Quiet::Editor::EditorViewportState& viewportState,
                           const glm::vec3& worldA,
                           const glm::vec3& worldB,
                           ImU32 color,
                           float thickness)
    {
        ImVec2 screenA{};
        ImVec2 screenB{};

        if (!ProjectWorldToViewport(worldA, cameraData, viewportState, screenA)) return false;
        if (!ProjectWorldToViewport(worldB, cameraData, viewportState, screenB)) return false;
        drawList->AddLine(screenA, screenB, color, thickness);
        return true;
    }

    void DrawScreenArrowHead(ImDrawList* drawList,
                             const ImVec2& start,
                             const ImVec2& end,
                             ImU32 color,
                             float thickness)
    {
        const ImVec2 delta(end.x - start.x, end.y - start.y);
        const float length = std::sqrt(delta.x * delta.x + delta.y * delta.y);

        if (length <= 0.00001f) return;

        const ImVec2 direction(delta.x / length, delta.y / length);
        const ImVec2 perpendicular(-direction.y, direction.x);

        constexpr float arrowLength = 11.0f;
        constexpr float arrowWidth = 6.0f;

        const ImVec2 left(
            end.x - direction.x * arrowLength + perpendicular.x * arrowWidth,
            end.y - direction.y * arrowLength + perpendicular.y * arrowWidth
        );

        const ImVec2 right(
            end.x - direction.x * arrowLength - perpendicular.x * arrowWidth,
            end.y - direction.y * arrowLength - perpendicular.y * arrowWidth
        );

        drawList->AddLine(end, left, color, thickness);
        drawList->AddLine(end, right, color, thickness);
    }

    void DrawCameraFrustumOverlay(ImDrawList* drawList,
                                  const Quiet::Scene::Actor& actor,
                                  const Quiet::Scene::CameraComponent& cameraComponent,
                                  const Quiet::Renderer::Vulkan::CameraUniformData& cameraData,
                                  const Quiet::Editor::EditorViewportState& viewportState,
                                  bool selected)
    {
        const ImU32 color = selected
            ? IM_COL32(255, 255, 255, 230)
            : IM_COL32(80, 190, 255, 150);

        const ImU32 centerColor = selected
            ? IM_COL32(255, 255, 255, 240)
            : IM_COL32(80, 190, 255, 180);

        const float aspect = viewportState.Height > 0
            ? static_cast<float>(viewportState.Width) / static_cast<float>(viewportState.Height)
            : 1.0f;

        const float fovRadians = glm::radians(cameraComponent.FieldOfViewDegrees);

        const float nearDistance = std::max(cameraComponent.NearClip, 0.05f);
        const float farDistance = std::max(nearDistance + 0.1f, 1.25f);

        const float nearHalfHeight = std::tan(fovRadians * 0.5f) * nearDistance;
        const float nearHalfWidth = nearHalfHeight * aspect;

        const float farHalfHeight = std::tan(fovRadians * 0.5f) * farDistance;
        const float farHalfWidth = farHalfHeight * aspect;

        const glm::vec3 nearCenter{ 0.0f, 0.0f, -nearDistance };
        const glm::vec3 farCenter{ 0.0f, 0.0f, -farDistance };

        const std::array<glm::vec3, 4> nearCorners =
        {
            nearCenter + glm::vec3{ -nearHalfWidth,  nearHalfHeight, 0.0f },
            nearCenter + glm::vec3{  nearHalfWidth,  nearHalfHeight, 0.0f },
            nearCenter + glm::vec3{  nearHalfWidth, -nearHalfHeight, 0.0f },
            nearCenter + glm::vec3{ -nearHalfWidth, -nearHalfHeight, 0.0f }
        };

        const std::array<glm::vec3, 4> farCorners =
        {
            farCenter + glm::vec3{ -farHalfWidth,  farHalfHeight, 0.0f },
            farCenter + glm::vec3{  farHalfWidth,  farHalfHeight, 0.0f },
            farCenter + glm::vec3{  farHalfWidth, -farHalfHeight, 0.0f },
            farCenter + glm::vec3{ -farHalfWidth, -farHalfHeight, 0.0f }
        };

        const glm::mat4 model = actor.Transform.WorldTransform.ToMatrix();
        const glm::vec3 worldOrigin = actor.Transform.WorldTransform.Position;

        std::array<glm::vec3, 4> worldNear{};
        std::array<glm::vec3, 4> worldFar{};

        for (std::size_t i = 0; i < 4; ++i)
        {
            worldNear[i] = Quiet::Math::TransformPoint(model, nearCorners[i]);
            worldFar[i] = Quiet::Math::TransformPoint(model, farCorners[i]);
        }

        for (std::size_t i = 0; i < 4; ++i)
        {
            const std::size_t next = (i + 1) % 4;

            DrawProjectedLine(drawList, cameraData, viewportState, worldNear[i], worldNear[next], color, 1.5f);
            DrawProjectedLine(drawList, cameraData, viewportState, worldFar[i], worldFar[next], color, 1.5f);
            DrawProjectedLine(drawList, cameraData, viewportState, worldNear[i], worldFar[i], color, 1.5f);
            DrawProjectedLine(drawList, cameraData, viewportState, worldOrigin, worldFar[i], color, 1.0f);
        }

        ImVec2 originScreen{};
        if (ProjectWorldToViewport(worldOrigin, cameraData, viewportState, originScreen))
        {
            drawList->AddCircleFilled(originScreen, selected ? 4.0f : 3.0f, centerColor, 16);
        }
    }

    void DrawDirectionalLightDirectionOverlay(ImDrawList* drawList,
                                              const Scene::Actor& actor,
                                              const Scene::DirectionalLightComponent& lightComponent,
                                              const Renderer::Vulkan::CameraUniformData& cameraData,
                                              const EditorViewportState& viewportState,
                                              bool selected)
    {
        const ImU32 color = selected
            ? IM_COL32(255, 255, 255, 240)
            : IM_COL32(255, 220, 80, 170);

        const glm::vec3 origin = actor.Transform.WorldTransform.Position;

        // DirectionalLightComponent::Direction is direction from the surface toward light.
        // The visible arrow shows the emitted light direction, so it points the opposite way.
        const glm::vec3 emittedDirection = Math::SafeNormalize(
            -lightComponent.Direction,
            { 0.0f, -1.0f, 0.0f }
        );

        constexpr float sideOffset = 0.18f;
        constexpr glm::vec3 upFallback{ 0.0f, 1.0f, 0.0f };
        glm::vec3 right = glm::cross(emittedDirection, upFallback);

        if (glm::length(right) <= 0.00001f)
            right = glm::cross(emittedDirection, glm::vec3{ 1.0f, 0.0f, 0.0f });

        right =  Math::SafeNormalize(right, { 1.0f, 0.0f, 0.0f });

        const std::array rayOrigins =
        {
            origin,
            origin + right * sideOffset,
            origin - right * sideOffset
        };

        for (const glm::vec3& rayOrigin : rayOrigins)
        {
            constexpr float rayLength = 1.75f;
            const glm::vec3 rayEnd = rayOrigin + emittedDirection * rayLength;
            ImVec2 screenStart{};
            ImVec2 screenEnd{};

            if (!ProjectWorldToViewport(rayOrigin, cameraData, viewportState, screenStart) ||
                !ProjectWorldToViewport(rayEnd, cameraData, viewportState, screenEnd))
                continue;

            drawList->AddLine(screenStart, screenEnd, color, selected ? 2.0f : 1.5f);
            DrawScreenArrowHead(drawList, screenStart, screenEnd, color, selected ? 2.0f : 1.5f);
        }
    }
}
