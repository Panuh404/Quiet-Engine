#include "Editor/Viewport/EditorViewportOverlay.h"

#include <algorithm>
#include <array>
#include <cmath>

#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>
#include <glm/vec3.hpp>
#include <glm/geometric.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/trigonometric.hpp>

#include "Quiet/Editor/EditorState.h"
#include "Editor/Viewport/EditorViewportProjection.h"
#include "Renderer/UI/ImGuiTheme.h"
#include "Renderer/Vulkan/Mesh/StaticMesh.h"
#include "Scene/Actor.h"
#include "Scene/Transform.h"
#include "Scene/Components/CameraComponent.h"
#include "Scene/Components/DirectionalLightComponent.h"
#include "Scene/Components/BoxColliderComponent.h"
#include "Scene/Components/CapsuleColliderComponent.h"
#include "Scene/Components/RigidBodyComponent.h"
#include "Scene/Components/SphereColliderComponent.h"


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
        const auto* staticMesh = actor.TryGetComponent<Quiet::Scene::StaticMeshComponent>();
        if (!staticMesh || !staticMesh->IsValid() || !staticMesh->Visible)
        {
            return false;
        }

        const auto& mesh = staticMesh->Mesh;
        const auto& transform = actor.GetComponent<Quiet::Scene::TransformComponent>();

        const glm::mat4 model = transform.WorldTransform.ToMatrix();
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

#pragma region Collider Drawing

    glm::mat4 BuildPhysicsBodyModel(const Quiet::Scene::Transform& transform)
    {
        Quiet::Scene::Transform bodyTransform = transform;
        bodyTransform.Scale = { 1.0f, 1.0f, 1.0f };
        return bodyTransform.ToMatrix();
    }

    glm::vec3 TransformColliderPoint(const glm::mat4& bodyModel, const glm::vec3& localPoint)
    {
        return glm::vec3(bodyModel * glm::vec4(localPoint, 1.0f));
    }

    void DrawColliderWireCircle(ImDrawList* drawList,
                                const Quiet::Renderer::Vulkan::CameraUniformData& cameraData,
                                const Quiet::Editor::EditorViewportState& viewportState,
                                const glm::mat4& bodyModel,
                                const glm::vec3& center,
                                const glm::vec3& axisA,
                                const glm::vec3& axisB,
                                const float radius,
                                const ImU32 color,
                                const float thickness)
    {
        constexpr int segmentCount = 32;

        glm::vec3 previous = center + axisA * radius;
        for (int segment = 1; segment <= segmentCount; ++segment)
        {
            const float angle = glm::two_pi<float>() * static_cast<float>(segment) / static_cast<float>(segmentCount);
            const glm::vec3 current =
                center +
                axisA * std::cos(angle) * radius +
                axisB * std::sin(angle) * radius;

            Quiet::Editor::DrawProjectedLine(
                drawList,
                cameraData,
                viewportState,
                TransformColliderPoint(bodyModel, previous),
                TransformColliderPoint(bodyModel, current),
                color,
                thickness);

            previous = current;
        }
    }

    void DrawColliderCapsulePlane(ImDrawList* drawList,
                                  const Quiet::Renderer::Vulkan::CameraUniformData& cameraData,
                                  const Quiet::Editor::EditorViewportState& viewportState,
                                  const glm::mat4& bodyModel,
                                  const glm::vec3& center,
                                  const glm::vec3& radialAxis,
                                  const float halfCylinderHeight,
                                  const float radius,
                                  const ImU32 color,
                                  const float thickness)
    {
        constexpr glm::vec3 up{ 0.0f, 1.0f, 0.0f };
        constexpr int arcSegments = 16;

        glm::vec3 previous = center + radialAxis * radius + up * halfCylinderHeight;

        for (int segment = 1; segment <= arcSegments; ++segment)
        {
            const float angle = glm::pi<float>() * static_cast<float>(segment) / static_cast<float>(arcSegments);
            const glm::vec3 current = center + radialAxis * std::cos(angle) * radius + up * (halfCylinderHeight + std::sin(angle) * radius);

        Quiet::Editor::DrawProjectedLine(
            drawList,
            cameraData,
            viewportState,
            TransformColliderPoint(bodyModel, previous),
            TransformColliderPoint(bodyModel, current),
            color,
            thickness);

        previous = current;
    }

    previous = center - radialAxis * radius - up * halfCylinderHeight;
    for (int segment = 1; segment <= arcSegments; ++segment)
    {
        const float angle = glm::pi<float>() + glm::pi<float>() * static_cast<float>(segment) / static_cast<float>(arcSegments);
        const glm::vec3 current = center + radialAxis * std::cos(angle) * radius + up * (-halfCylinderHeight + std::sin(angle) * radius);

        Quiet::Editor::DrawProjectedLine(
            drawList,
            cameraData,
            viewportState,
            TransformColliderPoint(bodyModel, previous),
            TransformColliderPoint(bodyModel, current),
            color,
            thickness);

        previous = current;
    }

    const glm::vec3 rightTop = center + radialAxis * radius + up * halfCylinderHeight;
    const glm::vec3 rightBottom = center + radialAxis * radius - up * halfCylinderHeight;
    const glm::vec3 leftTop = center - radialAxis * radius + up * halfCylinderHeight;
    const glm::vec3 leftBottom = center - radialAxis * radius - up * halfCylinderHeight;

    Quiet::Editor::DrawProjectedLine(
        drawList,
        cameraData,
        viewportState,
        TransformColliderPoint(bodyModel, rightTop),
        TransformColliderPoint(bodyModel, rightBottom),
        color,
        thickness);

    Quiet::Editor::DrawProjectedLine(
        drawList,
        cameraData,
        viewportState,
        TransformColliderPoint(bodyModel, leftTop),
        TransformColliderPoint(bodyModel, leftBottom),
        color,
        thickness);
}

#pragma endregion
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
        const Scene::TransformComponent& transform = actor.GetComponent<Scene::TransformComponent>();
        const std::string& actorName = actor.GetName();

        ImVec2 actorScreenPosition{};
        const bool originVisible =  ProjectWorldToViewport(transform.WorldTransform.Position, cameraData, viewportState, actorScreenPosition);

        ScreenRect boundsRect{};
        const bool boundsVisible = ProjectStaticMeshBoundsToViewport(actor, cameraData, viewportState, boundsRect);

        if (!originVisible && !boundsVisible)
        {
            return;
        }

        if (originVisible)
        {
            DrawSelectedOriginMarker(drawList, actorScreenPosition);
            DrawSelectedActorName(drawList, actorScreenPosition, actorName);
            return;
        }

        DrawSelectedActorName(drawList, boundsRect.Min, actorName);
    }

    void DrawHoveredActorViewportOverlay(const Scene::Actor& actor,
                                         const Renderer::Vulkan::CameraUniformData& cameraData,
                                         const EditorViewportState& viewportState,
                                         ImDrawList* drawList)
    {
        const Scene::TransformComponent& transform = actor.GetComponent<Scene::TransformComponent>();
        const std::string& actorName = actor.GetName();

        ImVec2 actorScreenPosition{};
        const bool originVisible = ProjectWorldToViewport(transform.WorldTransform.Position, cameraData, viewportState, actorScreenPosition);

        ScreenRect boundsRect{};
        const bool boundsVisible = ProjectStaticMeshBoundsToViewport(actor, cameraData, viewportState, boundsRect);
        if (!originVisible && !boundsVisible)
        {
            return;
        }

        if (boundsVisible)
        {
            DrawHoveredBoundsOverlay(drawList, boundsRect);
            DrawHoveredActorName(drawList, boundsRect.Min, actorName);
        }
        else
        {
            DrawHoveredOriginMarker(drawList, actorScreenPosition);
            DrawHoveredActorName(drawList, actorScreenPosition, actorName);
        }
    }

    void DrawPhysicsColliderOverlay(const Scene::Actor &actor,
                                    const Renderer::Vulkan::CameraUniformData &cameraData,
                                    const EditorViewportState &viewportState,
                                    ImDrawList *drawList,
                                    bool selected)
    {
        const auto* boxCollider = actor.TryGetComponent<Scene::BoxColliderComponent>();
        const auto* sphereCollider = actor.TryGetComponent<Scene::SphereColliderComponent>();
        const auto* capsuleCollider = actor.TryGetComponent<Scene::CapsuleColliderComponent>();

        const int colliderCount =
            (boxCollider ? 1 : 0) +
            (sphereCollider ? 1 : 0) +
            (capsuleCollider ? 1 : 0);

        if (colliderCount != 1) return;

        const auto& transform = actor.GetComponent<Scene::TransformComponent>().WorldTransform;
        const auto* rigidBody = actor.TryGetComponent<Scene::RigidBodyComponent>();

        ImVec4 sourceColor = Renderer::UI::EditorColor::Error;
        if (rigidBody)
        {
            switch (rigidBody->MotionType)
            {
                case Scene::RigidBodyMotionType::Static:
                    sourceColor = Renderer::UI::EditorColor::TextMuted;
                    break;

                case Scene::RigidBodyMotionType::Dynamic:
                    sourceColor = Renderer::UI::EditorColor::Success;
                    break;

                case Scene::RigidBodyMotionType::Kinematic:
                    sourceColor = Renderer::UI::EditorColor::Warning;
                    break;
            }
        }

        sourceColor.w = selected ? 1.0f : 0.72f;

        const ImU32 color = MakeOverlayColor(sourceColor);
        const float thickness = Renderer::UI::EditorSizing::Px(selected ? 2.25f : 1.25f);

        const glm::mat4 bodyModel = BuildPhysicsBodyModel(transform);
        constexpr float minimumSize = 0.001f;

        if (boxCollider)
        {
            glm::vec3 effectiveSize = glm::abs(boxCollider->Size * transform.Scale);
            effectiveSize = glm::max(effectiveSize, glm::vec3(minimumSize));

            const glm::vec3 halfExtent = effectiveSize * 0.5f;
            const glm::vec3 center = boxCollider->Center * transform.Scale;

            const auto corners = Math::BuildAABBCorners(center - halfExtent, center + halfExtent);
            constexpr std::array<std::array<std::size_t, 2>, 12> edges =
            {{
                {{0, 1}}, {{0, 2}}, {{1, 3}}, {{2, 3}},

                {{4, 5}}, {{4, 6}}, {{5, 7}}, {{6, 7}},

                {{0, 4}}, {{1, 5}}, {{2, 6}}, {{3, 7}}
            }};

            for (const auto& edge : edges)
            {
                DrawProjectedLine(
                    drawList,
                    cameraData,
                    viewportState,
                    TransformColliderPoint(bodyModel, corners[edge[0]]),
                    TransformColliderPoint(bodyModel, corners[edge[1]]),
                    color,
                    thickness);
            }

            return;
        }

        if (sphereCollider)
        {
            const glm::vec3 absoluteScale = glm::abs(transform.Scale);
            const float maximumScale = std::max({ absoluteScale.x, absoluteScale.y, absoluteScale.z });
            const float radius = std::max((std::isfinite(sphereCollider->Radius) ? sphereCollider->Radius : 0.5f) * maximumScale, minimumSize);
            const glm::vec3 center = sphereCollider->Center * transform.Scale;

            DrawColliderWireCircle(
                drawList,
                cameraData,
                viewportState,
                bodyModel,
                center,
                {1.0f, 0.0f, 0.0f},
                {0.0f, 1.0f, 0.0f},
                radius,
                color,
                thickness);

            DrawColliderWireCircle(
                drawList,
                cameraData,
                viewportState,
                bodyModel,
                center,
                {1.0f, 0.0f, 0.0f},
                {0.0f, 0.0f, 1.0f},
                radius,
                color,
                thickness);

            DrawColliderWireCircle(
                drawList,
                cameraData,
                viewportState,
                bodyModel,
                center,
                {0.0f, 1.0f, 0.0f},
                {0.0f, 0.0f, 1.0f},
                radius,
                color,
                thickness);

            return;
        }

        const glm::vec3 absoluteScale = glm::abs(transform.Scale);
        const float radialScale = std::max(absoluteScale.x, absoluteScale.z);
        const float verticalScale = absoluteScale.y;
        const float radius = std::max((std::isfinite(capsuleCollider->Radius) ? capsuleCollider->Radius : 0.5f) * radialScale, minimumSize);
        const float minimumHeight = radius * 2.0f + minimumSize;
        const float height = std::max((std::isfinite(capsuleCollider->Height) ? capsuleCollider->Height : 2.0f) * verticalScale, minimumHeight);
        const float halfCylinderHeight = (height - radius * 2.0f) * 0.5f;

        const glm::vec3 center = capsuleCollider->Center * transform.Scale;

        DrawColliderWireCircle(
            drawList,
            cameraData,
            viewportState,
            bodyModel,
            center + glm::vec3(0.0f, halfCylinderHeight, 0.0f),
            {1.0f, 0.0f, 0.0f},
            {0.0f, 0.0f, 1.0f},
            radius,
            color,
            thickness);

        DrawColliderWireCircle(
            drawList,
            cameraData,
            viewportState,
            bodyModel,
            center - glm::vec3(0.0f, halfCylinderHeight, 0.0f),
            {1.0f, 0.0f, 0.0f},
            {0.0f, 0.0f, 1.0f},
            radius,
            color,
            thickness);

        DrawColliderCapsulePlane(
            drawList,
            cameraData,
            viewportState,
            bodyModel,
            center,
            {1.0f, 0.0f, 0.0f},
            halfCylinderHeight,
            radius,
            color,
            thickness);

        DrawColliderCapsulePlane(
            drawList,
            cameraData,
            viewportState,
            bodyModel,
            center,
            {0.0f, 0.0f, 1.0f},
            halfCylinderHeight,
            radius,
            color,
            thickness);
    }

    void DrawViewportMeshDropPreview(const Renderer::Vulkan::StaticMesh &mesh,
                                     const glm::vec3 &worldPosition,
                                     const Renderer::Vulkan::CameraUniformData &cameraData,
                                     const EditorViewportState &viewportState,
                                     ImDrawList *drawList)
    {
        const auto localCorners = Math::BuildAABBCorners(mesh.GetLocalBoundsMin(), mesh.GetLocalBoundsMax());
        std::array<glm::vec3, 8> worldCorners{};

        for (std::size_t index = 0; index < localCorners.size(); ++index)
        {
            worldCorners[index] = localCorners[index] + worldPosition;
        }

        constexpr std::array<std::array<std::size_t, 2>, 12> edges =
        {{
            // Near face.
            {{0, 1}},
            {{0, 2}},
            {{1, 3}},
            {{2, 3}},

            // Far face.
            {{4, 5}},
            {{4, 6}},
            {{5, 7}},
            {{6, 7}},

            // Connecting edges.
            {{0, 4}},
            {{1, 5}},
            {{2, 6}},
            {{3, 7}}
        }};

        const ImU32 previewColor = MakeOverlayColor(Renderer::UI::EditorColor::Success, 0.95f);
        constexpr ImU32 shadowColor = IM_COL32(0, 0, 0, 190);
        const float previewThickness = Renderer::UI::EditorSizing::Px(2.0f);

        for (const auto& edge : edges)
        {
            const glm::vec3& worldA = worldCorners[edge[0]];
            const glm::vec3& worldB = worldCorners[edge[1]];

            DrawProjectedLine(
                drawList,
                cameraData,
                viewportState,
                worldA,
                worldB,
                shadowColor,
                previewThickness + Renderer::UI::EditorSizing::Px(2.0f));

            DrawProjectedLine(
                drawList,
                cameraData,
                viewportState,
                worldA,
                worldB,
                previewColor,
                previewThickness
            );
        }

        const glm::vec3 placementAnchor = worldPosition + glm::vec3(0.0f, mesh.GetLocalBoundsMin().y, 0.0f);
        ImVec2 groundScreenPosition{};

        if (!ProjectWorldToViewport(placementAnchor, cameraData, viewportState, groundScreenPosition))
        {
            return;
        }

        const float markerRadius = Renderer::UI::EditorSizing::Px(9.0f);
        const float markerCrossSize = Renderer::UI::EditorSizing::Px(13.0f);

        drawList->AddCircle(
            groundScreenPosition,
            markerRadius,
            previewColor,
            0,
            Renderer::UI::EditorSizing::Px(2.0f)
        );

        drawList->AddLine(
            ImVec2(groundScreenPosition.x - markerCrossSize, groundScreenPosition.y),
            ImVec2(groundScreenPosition.x + markerCrossSize, groundScreenPosition.y),
            previewColor,
            Renderer::UI::EditorSizing::Px(1.5f)
        );

        drawList->AddLine(
            ImVec2(groundScreenPosition.x, groundScreenPosition.y - markerCrossSize),
            ImVec2(groundScreenPosition.x, groundScreenPosition.y + markerCrossSize),
            previewColor,
            Renderer::UI::EditorSizing::Px(1.5f)
        );
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

        const Scene::TransformComponent& transform = actor.GetComponent<Scene::TransformComponent>();
        const glm::mat4 model = transform.WorldTransform.ToMatrix();
        const glm::vec3 worldOrigin = transform.WorldTransform.Position;

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

        const glm::vec3 origin = actor.GetComponent<Scene::TransformComponent>().WorldTransform.Position;

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
