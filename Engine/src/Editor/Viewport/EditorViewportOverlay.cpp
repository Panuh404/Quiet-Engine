#include "Editor/Viewport/EditorViewportOverlay.h"

#include <algorithm>
#include <array>
#include <cmath>

#include <glm/vec3.hpp>
#include <glm/geometric.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/trigonometric.hpp>

#include "Quiet/Editor/EditorState.h"
#include "Editor/Viewport/EditorViewportProjection.h"
#include "Renderer/UI/ImGuiTheme.h"
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

        const auto corners = Quiet::Math::BuildAABBCorners(mesh->GetLocalBoundsMin(), mesh->GetLocalBoundsMax());

        Quiet::Editor::ScreenRect rect{};
        for (const glm::vec3& corner : corners)
        {
            const glm::vec3 worldCorner = glm::vec3(model * glm::vec4(corner, 1.0f));
            ImVec2 screenPoint{};
            if (Quiet::Editor::ProjectWorldToViewport(worldCorner, cameraData, viewportState, screenPoint))
            {
                ExpandScreenRect(rect, screenPoint);
            }
        }

        if (!rect.Valid) return false;
        outRect = rect;
        return true;
    }

    void DrawFilledScreenRect(ImDrawList* drawList, const ImVec2& min, const ImVec2& max, const ImU32 color)
    {
        drawList->AddQuadFilled(min, ImVec2(max.x, min.y), max, ImVec2(min.x, max.y), color);
    }

    void DrawScreenRectOutline(ImDrawList* drawList, const ImVec2& min, const ImVec2& max, const ImU32 color, const float thickness)
    {
        drawList->AddLine(min, ImVec2(max.x, min.y), color, thickness);
        drawList->AddLine(ImVec2(max.x, min.y), max, color, thickness);

        drawList->AddLine(max, ImVec2(min.x, max.y), color, thickness);
        drawList->AddLine(ImVec2(min.x, max.y), min, color, thickness);
    }

    ImU32 MakeOverlayColor(const ImVec4& sourceColor, const float alphaMultiplier = 1.0f)
    {
        ImVec4 color = sourceColor;
        color.w *= std::clamp(alphaMultiplier, 0.0f, 1.0f);

        return ImGui::ColorConvertFloat4ToU32(color);
    }

    void DrawScreenRectCorners(ImDrawList* drawList, const ImVec2& min, const ImVec2& max, const ImU32 color, const float requestedLength, const float thickness)
    {
        const float width = std::max(max.x - min.x, 0.0f);
        const float height = std::max(max.y - min.y, 0.0f);

        const float maximumLength = std::min(width, height) * 0.4f;
        const float length = std::clamp(requestedLength, 1.0f, std::max(maximumLength, 1.0f));

        drawList->AddLine(min, ImVec2(min.x + length, min.y), color, thickness);
        drawList->AddLine(min, ImVec2(min.x, min.y + length), color, thickness);
        drawList->AddLine(ImVec2(max.x - length, min.y), ImVec2(max.x, min.y), color, thickness);
        drawList->AddLine(ImVec2(max.x, min.y), ImVec2(max.x, min.y + length), color, thickness);
        drawList->AddLine(ImVec2(min.x, max.y - length), ImVec2(min.x, max.y), color, thickness);
        drawList->AddLine(ImVec2(min.x, max.y), ImVec2(min.x + length, max.y), color, thickness);
        drawList->AddLine(ImVec2(max.x - length, max.y), max, color, thickness);
        drawList->AddLine(ImVec2(max.x, max.y - length), max, color, thickness);
    }

    void DrawScreenDiamondFilled(ImDrawList* drawList, const ImVec2& center, const float radius, const ImU32 color)
    {
        drawList->AddQuadFilled(
            ImVec2(center.x, center.y - radius),
            ImVec2(center.x + radius, center.y),
            ImVec2(center.x, center.y + radius),
            ImVec2(center.x - radius, center.y),
            color
        );
    }

    void DrawScreenDiamondOutline(ImDrawList* drawList, const ImVec2& center, const float radius, const ImU32 color, const float thickness)
    {
        const ImVec2 top(center.x, center.y - radius);
        const ImVec2 right(center.x + radius, center.y);
        const ImVec2 bottom(center.x, center.y + radius);
        const ImVec2 left(center.x - radius, center.y);

        drawList->AddLine(top, right, color, thickness);
        drawList->AddLine(right, bottom, color, thickness);
        drawList->AddLine(bottom, left, color, thickness);
        drawList->AddLine(left, top, color, thickness);
    }

    void DrawScreenTriangleOutline(ImDrawList* drawList, const ImVec2& a, const ImVec2& b, const ImVec2& c, const ImU32 color, const float thickness)
    {
        drawList->AddLine(a, b, color, thickness);
        drawList->AddLine(b, c, color, thickness);
        drawList->AddLine(c, a, color, thickness);
    }

    void DrawScreenCross(ImDrawList* drawList, const ImVec2& center, const float radius, const ImU32 color, const float thickness)
    {
        drawList->AddLine(
            ImVec2(center.x - radius, center.y),
            ImVec2(center.x + radius, center.y),
            color,
            thickness
        );

        drawList->AddLine(
            ImVec2(center.x, center.y - radius),
            ImVec2(center.x, center.y + radius),
            color,
            thickness
        );
    }
}

namespace Quiet::Editor
{
    void DrawDebugLabel(ImDrawList* drawList, const ImVec2& position, const char* label, const bool selected, const bool hovered)
    {
        const ImU32 textColor =
            selected
                ? IM_COL32(255, 255, 255, 255)
                : hovered
                    ? MakeOverlayColor(Renderer::UI::EditorColor::AccentHover)
                    : IM_COL32(210, 220, 230, 230);

        drawList->AddText(ImVec2(position.x + 14.0f, position.y - 8.0f), textColor, label);
    }

    void DrawCameraDebugIcon(ImDrawList* drawList, const ImVec2& position, const bool selected, const bool hovered)
    {
        const ImU32 color =
            selected
                ? IM_COL32(255, 255, 255, 255)
                : hovered
                    ? IM_COL32(150, 220, 255, 255)
                    : IM_COL32(80, 190, 255, 230);

        const ImU32 fill =
            selected
                ? IM_COL32(80, 190, 255, 100)
                : hovered
                    ? IM_COL32(80, 190, 255, 90)
                    : IM_COL32(80, 190, 255, 45);

        constexpr float halfWidth = 9.0f;
        constexpr float halfHeight = 6.0f;

        const ImVec2 min(position.x - halfWidth, position.y - halfHeight);
        const ImVec2 max(position.x + halfWidth, position.y + halfHeight);

        DrawFilledScreenRect(drawList, min, max, fill);
        DrawScreenRectOutline(drawList, min, max, color, selected || hovered ? 2.5f : 2.0f);
        DrawScreenTriangleOutline(
            drawList,
            ImVec2(max.x, position.y - 4.0f),
            ImVec2(max.x + 8.0f, position.y - 8.0f),
            ImVec2(max.x + 8.0f, position.y + 8.0f),
            color,
            selected || hovered ? 2.5f : 2.0f
        );
    }

    void DrawDirectionalLightDebugIcon(ImDrawList* drawList, const ImVec2& position, bool selected, const bool hovered)
    {
        const ImU32 color =
            selected
                ? IM_COL32(255, 255, 255, 255)
                : hovered
                    ? IM_COL32(255, 238, 145, 255)
                    : IM_COL32(255, 220, 80, 235);

        const ImU32 fill =
            selected
                ? IM_COL32(255, 220, 80, 110)
                : hovered
                    ? IM_COL32(255, 220, 80, 95)
                    : IM_COL32(255, 220, 80, 55);

        DrawScreenDiamondFilled(drawList, position, 8.0f, fill);
        DrawScreenDiamondOutline(drawList, position, 8.0f, color, 2.0f);

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
        const float padding = Renderer::UI::EditorSizing::Px(5.0f);

        const ImVec2 min(rect.Min.x - padding, rect.Min.y - padding);
        const ImVec2 max(rect.Max.x + padding, rect.Max.y + padding);

        const ImU32 subtleOutline = MakeOverlayColor(Renderer::UI::EditorColor::Accent, 0.42f);
        const ImU32 cornerColor = MakeOverlayColor(Renderer::UI::EditorColor::AccentHover);

        constexpr ImU32 shadow = IM_COL32(0, 0, 0, 180);

        DrawScreenRectOutline(drawList, min, max, subtleOutline, Renderer::UI::EditorSizing::Px(1.0f));
        DrawScreenRectCorners(drawList,
            ImVec2(min.x + Renderer::UI::EditorSizing::Px(1.0f), min.y + Renderer::UI::EditorSizing::Px(1.0f)),
            ImVec2(max.x + Renderer::UI::EditorSizing::Px(1.0f), max.y + Renderer::UI::EditorSizing::Px(1.0f)),
            shadow,
            Renderer::UI::EditorSizing::Px(17.0f),
            Renderer::UI::EditorSizing::Px(3.5f)
        );

        DrawScreenRectCorners(drawList, min, max, cornerColor,
            Renderer::UI::EditorSizing::Px(17.0f),
            Renderer::UI::EditorSizing::Px(2.25f)
        );
    }

    void DrawHoveredBoundsOverlay(ImDrawList *drawList, const ScreenRect &rect)
    {
        const float padding = Renderer::UI::EditorSizing::Px(3.0f);

        const ImVec2 min(rect.Min.x - padding, rect.Min.y - padding);
        const ImVec2 max(rect.Max.x + padding, rect.Max.y + padding);

        const ImU32 outlineColor = MakeOverlayColor(Renderer::UI::EditorColor::AccentHover, 0.62f);

        DrawScreenRectOutline(drawList, min, max, outlineColor, Renderer::UI::EditorSizing::Px(1.0f));
        DrawScreenRectCorners(drawList, min, max, outlineColor, Renderer::UI::EditorSizing::Px(11.0f), Renderer::UI::EditorSizing::Px(1.5f));
    }

    void DrawSelectedOriginMarker(ImDrawList* drawList, const ImVec2& position)
    {
        const ImU32 color = MakeOverlayColor(Renderer::UI::EditorColor::AccentHover);
        constexpr ImU32 shadow = IM_COL32(0, 0, 0, 170);

        DrawScreenDiamondFilled(drawList, ImVec2(position.x + 1.0f, position.y + 1.0f), 6.0f, shadow);
        DrawScreenDiamondFilled(drawList, position, 5.0f, color);

        DrawScreenDiamondOutline(drawList, position, 10.0f, color, 2.0f);

        DrawScreenCross(drawList, position, 13.0f, color, 1.5f);
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

    void DrawHoveredActorName(ImDrawList* drawList, const ImVec2& position, const std::string& actorName)
    {
        if (actorName.empty()) return;

        const ImU32 textColor = MakeOverlayColor(Renderer::UI::EditorColor::AccentHover, 0.92f);
        constexpr ImU32 shadow = IM_COL32(0, 0, 0, 190);

        const ImVec2 textPosition(
            position.x + Renderer::UI::EditorSizing::Px(8.0f),
            position.y - ImGui::GetTextLineHeight() - Renderer::UI::EditorSizing::Px(7.0f));

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

    void DrawHoveredOriginMarker(ImDrawList* drawList, const ImVec2& position)
    {
        const ImU32 color = MakeOverlayColor(Renderer::UI::EditorColor::AccentHover, 0.8f);

        drawList->AddCircle(
            position,
            Renderer::UI::EditorSizing::Px(8.0f),
            color,
            0,
            Renderer::UI::EditorSizing::Px(1.5f)
        );

        drawList->AddCircleFilled(
            position,
            Renderer::UI::EditorSizing::Px(2.5f),
            color
        );
    }

    void DrawSelectedActorViewportOverlay(const Scene::Actor& actor,
                                          const Renderer::Vulkan::CameraUniformData& cameraData,
                                          const EditorViewportState& viewportState,
                                          ImDrawList* drawList)
    {
        ImVec2 actorScreenPosition{};

        const bool originVisible = ProjectWorldToViewport(actor.Transform.WorldTransform.Position, cameraData, viewportState, actorScreenPosition);

        ScreenRect boundsRect{};
        const bool boundsVisible = ProjectStaticMeshBoundsToViewport(actor, cameraData, viewportState, boundsRect);

        if (!originVisible && !boundsVisible) return;

        if (boundsVisible)
        {
            DrawSelectedBoundsOverlay(drawList, boundsRect);
        }
        if (originVisible)
        {
            DrawSelectedOriginMarker(drawList, actorScreenPosition);
            DrawSelectedActorName(drawList, actorScreenPosition, actor.Name);
        }
        else
        {
            DrawSelectedActorName(drawList, boundsRect.Min, actor.Name);
        }
    }

    void DrawHoveredActorViewportOverlay(const Scene::Actor &actor,
                                         const Renderer::Vulkan::CameraUniformData &cameraData,
                                         const EditorViewportState &viewportState,
                                         ImDrawList *drawList)
    {
        ImVec2 actorScreenPosition{};

        const bool originVisible = ProjectWorldToViewport(actor.Transform.WorldTransform.Position, cameraData, viewportState, actorScreenPosition);
        ScreenRect boundsRect{};

        const bool boundsVisible = ProjectStaticMeshBoundsToViewport(actor, cameraData, viewportState, boundsRect);
        if (!originVisible && !boundsVisible) return;

        if (boundsVisible)
        {
            DrawHoveredBoundsOverlay(drawList, boundsRect);
            DrawHoveredActorName(drawList, boundsRect.Min, actor.Name);
        }
        else
        {
            DrawHoveredOriginMarker(drawList, actorScreenPosition);
            DrawHoveredActorName(drawList, actorScreenPosition, actor.Name);
        }
    }

    bool DrawProjectedLine(ImDrawList* drawList,
                           const Renderer::Vulkan::CameraUniformData& cameraData,
                           const EditorViewportState& viewportState,
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
                                  const Scene::Actor& actor,
                                  const Scene::CameraComponent& cameraComponent,
                                  const Renderer::Vulkan::CameraUniformData& cameraData,
                                  const EditorViewportState& viewportState,
                                  bool selected,
                                  const bool hovered)
    {
        const ImU32 color =
            selected
                ? IM_COL32(255, 255, 255, 230)
                : hovered
                    ? IM_COL32(145, 215, 255, 225)
                    : IM_COL32(80, 190, 255, 150);

        const ImU32 centerColor =
            selected
                ? IM_COL32(255, 255, 255, 240)
                : hovered
                    ? IM_COL32(145, 215, 255, 235)
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
            worldNear[i] = Math::TransformPoint(model, nearCorners[i]);
            worldFar[i] = Math::TransformPoint(model, farCorners[i]);
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
            DrawScreenDiamondFilled(drawList, originScreen, selected ? 4.0f : 3.0f, centerColor);
        }
    }

    void DrawDirectionalLightDirectionOverlay(ImDrawList* drawList,
                                              const Scene::Actor& actor,
                                              const Scene::DirectionalLightComponent& lightComponent,
                                              const Renderer::Vulkan::CameraUniformData& cameraData,
                                              const EditorViewportState& viewportState,
                                              bool selected,
                                              const bool hovered)
    {
        const ImU32 color =
            selected
                ? IM_COL32(255, 255, 255, 240)
                : hovered
                    ? IM_COL32(255, 238, 145, 230)
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

            drawList->AddLine(screenStart, screenEnd, color, selected || hovered ? 2.0f : 1.5f);
            DrawScreenArrowHead(drawList, screenStart, screenEnd, color, selected || hovered ? 2.0f : 1.5f);
        }
    }
}
