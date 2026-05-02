#include "Quiet/Editor/EditorController.h"

#include <algorithm>
#include <array>
#include <limits>
#include <optional>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/ext/matrix_projection.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "imgui.h"
#include "ImGuizmo.h"

#include "Quiet/Input/Input.h"
#include "Quiet/Math/Geometry.h"
#include "Quiet/Core/Log.h"
#include "Quiet/Renderer/RenderConventions.h"
#include "Renderer/Vulkan/Mesh/StaticMesh.h"
#include "Renderer/Vulkan/VulkanRenderer.h"
#include "Scene/Actor.h"
#include "Scene/Components/CameraComponent.h"
#include "Scene/Components/DirectionalLightComponent.h"
#include "Scene/Camera/FreeCamera.h"
#include "Scene/Scene.h"

namespace
{
    ImGuizmo::OPERATION ToImGuizmoOperation(Quiet::Editor::GizmoOperation operation)
    {
        switch (operation)
        {
            case Quiet::Editor::GizmoOperation::Translate:  return ImGuizmo::TRANSLATE;
            case Quiet::Editor::GizmoOperation::Rotate:     return ImGuizmo::ROTATE;
            case Quiet::Editor::GizmoOperation::Scale:      return ImGuizmo::SCALE;
            default:                                        return ImGuizmo::TRANSLATE;
        }
    }

    ImGuizmo::MODE ToImGuizmoMode(Quiet::Editor::GizmoMode mode)
    {
        switch (mode)
        {
            case Quiet::Editor::GizmoMode::Local:   return ImGuizmo::LOCAL;
            case Quiet::Editor::GizmoMode::World:   return ImGuizmo::WORLD;
            default:                                return ImGuizmo::WORLD;
        }
    }

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

    bool ProjectWorldToViewport(const glm::vec3& worldPosition,
                                const Quiet::Renderer::Vulkan::CameraUniformData& cameraData,
                                const Quiet::Editor::EditorViewportState& viewportState,
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

    std::optional<Quiet::Core::UUID> PickViewportDebugIcon(const Quiet::Scene::Scene& scene,
                                                           const Quiet::Renderer::Vulkan::CameraUniformData& cameraData,
                                                           const Quiet::Editor::EditorViewportState& viewportState,
                                                           const ImVec2& mousePosition)
    {
        std::optional<Quiet::Core::UUID> bestActorID;
        float bestDistanceSquared = std::numeric_limits<float>::max();

        for (const Quiet::Scene::Actor& actor : scene.GetActors())
        {
            if (!actor.Camera.has_value() && !actor.DirectionalLight.has_value()) continue;

            ImVec2 screenPosition{};
            if (!ProjectWorldToViewport(
                    actor.Transform.WorldTransform.Position,
                    cameraData,
                    viewportState,
                    screenPosition))
            {
                continue;
            }

            float verticalOffset = 0.0f;
            if (actor.Camera.has_value())
            {
                const ImVec2 iconPosition(
                    screenPosition.x,
                    screenPosition.y + verticalOffset
                );

                TrySetBestDebugIconHit(
                    actor.ID,
                    mousePosition,
                    iconPosition,
                    bestActorID,
                    bestDistanceSquared
                );
                verticalOffset += 22.0f;
            }

            if (actor.DirectionalLight.has_value())
            {
                const ImVec2 iconPosition(
                    screenPosition.x,
                    screenPosition.y + verticalOffset
                );

                TrySetBestDebugIconHit(
                    actor.ID,
                    mousePosition,
                    iconPosition,
                    bestActorID,
                    bestDistanceSquared
                );
            }
        }
        return bestActorID;
    }

    struct ScreenRect
    {
        ImVec2 Min{};
        ImVec2 Max{};
        bool Valid = false;
    };

    void ExpandScreenRect(ScreenRect& rect, const ImVec2& point)
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

    std::array<glm::vec3, 8> BuildAABBCorners(const glm::vec3& boundsMin,
                                              const glm::vec3& boundsMax)
    {
        return {
            glm::vec3{ boundsMin.x, boundsMin.y, boundsMin.z },
            glm::vec3{ boundsMax.x, boundsMin.y, boundsMin.z },
            glm::vec3{ boundsMin.x, boundsMax.y, boundsMin.z },
            glm::vec3{ boundsMax.x, boundsMax.y, boundsMin.z },
            glm::vec3{ boundsMin.x, boundsMin.y, boundsMax.z },
            glm::vec3{ boundsMax.x, boundsMin.y, boundsMax.z },
            glm::vec3{ boundsMin.x, boundsMax.y, boundsMax.z },
            glm::vec3{ boundsMax.x, boundsMax.y, boundsMax.z }
        };
    }

    bool ProjectStaticMeshBoundsToViewport(const Quiet::Scene::Actor& actor,
                                           const Quiet::Renderer::Vulkan::CameraUniformData& cameraData,
                                           const Quiet::Editor::EditorViewportState& viewportState,
                                           ScreenRect& outRect)
    {
        if (!actor.StaticMesh.has_value() ||
            !actor.StaticMesh->IsValid() ||
            !actor.StaticMesh->Visible)
            return false;

        const auto& mesh = actor.StaticMesh->Mesh;
        const glm::mat4 model = actor.Transform.WorldTransform.ToMatrix();

        const auto corners = BuildAABBCorners(
            mesh->GetLocalBoundsMin(),
            mesh->GetLocalBoundsMax()
        );

        ScreenRect rect{};
        for (const glm::vec3& corner : corners)
        {
            const glm::vec3 worldCorner = glm::vec3(model * glm::vec4(corner, 1.0f));
            ImVec2 screenPoint{};
            if (ProjectWorldToViewport(
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

    void DrawSelectedBoundsOverlay(ImDrawList* drawList,
                                   const ScreenRect& rect)
    {
        const ImU32 color = IM_COL32(255, 210, 70, 255);
        const ImU32 shadow = IM_COL32(0, 0, 0, 170);

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

    void DrawSelectedOriginMarker(ImDrawList* drawList,
                                  const ImVec2& position)
    {
        const ImU32 color = IM_COL32(255, 210, 70, 255);
        const ImU32 shadow = IM_COL32(0, 0, 0, 170);

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

    void DrawSelectedActorName(ImDrawList* drawList,
                               const ImVec2& position,
                               const std::string& actorName)
    {
        if (actorName.empty()) return;

        const ImU32 textColor = IM_COL32(255, 255, 255, 255);
        const ImU32 shadow = IM_COL32(0, 0, 0, 190);
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

    void DrawSelectedActorViewportOverlay(const Quiet::Scene::Actor& actor,
                                          const Quiet::Renderer::Vulkan::CameraUniformData& cameraData,
                                          const Quiet::Editor::EditorViewportState& viewportState,
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
}

namespace Quiet::Editor
{
    void EditorController::TrySelectActorFromViewport(const Input::Input& input,
                                                      Scene::Scene& scene,
                                                      const Scene::FreeCamera& camera,
                                                      const Renderer::Vulkan::VulkanRenderer& renderer,
                                                      bool mouseCaptureEnabled)
    {
        if (mouseCaptureEnabled) return;
        if (m_State.Scene.Mode != SceneMode::Edit) return;
        if (!m_State.Viewport.ImageHovered) return;
        if (!input.IsMouseButtonPressed(Input::MouseCode::ButtonLeft)) return;
        if (ImGuizmo::IsUsing() || ImGuizmo::IsOver()) return;

        const VkExtent2D sceneExtent = renderer.GetSceneViewportExtent();
        if (sceneExtent.width == 0 || sceneExtent.height == 0) return;

        const float aspect =
            static_cast<float>(sceneExtent.width) /
            static_cast<float>(sceneExtent.height);

        const auto cameraData =
            camera.BuildUniformData(aspect, Renderer::UseEditorScreenProjectionYFlip);

        const ImVec2 mousePosition = ImGui::GetMousePos();
        const std::optional<Core::UUID> debugIconHit =
            PickViewportDebugIcon(
                scene,
                cameraData,
                m_State.Viewport,
                mousePosition
            );

        if (debugIconHit.has_value())
        {
            m_State.Selection.SelectedActorID = debugIconHit;
            QUIET_CORE_TRACE("Viewport debug icon selected actor.");
            return;
        }

        Math::Ray ray{};
        if (!BuildViewportPickRay(input, camera, renderer, ray)) return;

        auto& actors = scene.GetActors();
        float closestHit = std::numeric_limits<float>::max();
        std::optional<Core::UUID> bestHitID;

        for (std::size_t i = 0; i < actors.size(); ++i)
        {
            auto& actor = actors[i];
            if (!actor.StaticMesh.has_value() ||
                !actor.StaticMesh->IsValid() ||
                !actor.StaticMesh->Visible)
            {
                continue;
            }

            const auto& mesh = actor.StaticMesh->Mesh;
            const glm::mat4 model = actor.Transform.WorldTransform.ToMatrix();

            const Math::AABB worldBounds = Math::TransformAABB(
                model,
                mesh->GetLocalBoundsMin(),
                mesh->GetLocalBoundsMax()
            );

            float hitDistance = 0.0f;
            if (Math::IntersectRayAABB(ray, worldBounds, hitDistance))
            {
                if (hitDistance < closestHit)
                {
                    closestHit = hitDistance;
                    bestHitID = actor.ID;
                }
            }
        }

        if (bestHitID.has_value())
        {
            m_State.Selection.SelectedActorID = bestHitID;
            QUIET_CORE_TRACE("Viewport pick selected actor.");
        }
        else
        {
            m_State.Selection.SelectedActorID.reset();
        }
    }

    void EditorController::BuildSelectedActorGizmo(Scene::Scene& scene,
                                                   Scene::FreeCamera& camera,
                                                   const Renderer::Vulkan::VulkanRenderer& renderer,
                                                   bool mouseCaptureEnabled)
    {
        if (mouseCaptureEnabled) return;
        if (m_State.Scene.Mode != SceneMode::Edit) return;
        if (!renderer.IsSceneViewportReady()) return;
        if (!m_State.Selection.SelectedActorID.has_value()) return;
        if (m_State.Viewport.Width == 0 || m_State.Viewport.Height == 0) return;

        const VkExtent2D sceneExtent = renderer.GetSceneViewportExtent();
        const float aspect =
            static_cast<float>(sceneExtent.width) /
            static_cast<float>(sceneExtent.height);

        const auto cameraData = camera.BuildUniformData(aspect, Renderer::UseEditorScreenProjectionYFlip);
        Scene::Actor* actor = scene.FindActorByID(m_State.Selection.SelectedActorID.value());
        if (!actor)
        {
            m_State.Selection.SelectedActorID.reset();
            return;
        }

        auto& transform = actor->Transform.WorldTransform;
        float translation[3] =
        {
            transform.Position.x,
            transform.Position.y,
            transform.Position.z
        };
        float rotation[3] =
        {
            transform.RotationDegrees.x,
            transform.RotationDegrees.y,
            transform.RotationDegrees.z
        };
        float scale[3] =
        {
            transform.Scale.x,
            transform.Scale.y,
            transform.Scale.z
        };

        float modelArray[16]{};
        ImGuizmo::RecomposeMatrixFromComponents(
            translation,
            rotation,
            scale,
            modelArray
        );

        glm::mat4 model = glm::make_mat4(modelArray);
        ImGuizmo::SetOrthographic(false);
        ImGuizmo::SetDrawlist();
        ImGuizmo::SetRect(
            m_State.Viewport.ScreenX,
            m_State.Viewport.ScreenY,
            static_cast<float>(m_State.Viewport.Width),
            static_cast<float>(m_State.Viewport.Height)
        );

        const ImGuizmo::OPERATION operation = ToImGuizmoOperation(m_State.Gizmo.Operation);
        const ImGuizmo::MODE mode = ToImGuizmoMode(m_State.Gizmo.Mode);

        float snapValues[3] = { 0.0f, 0.0f, 0.0f };
        const float* snapPointer = nullptr;

        if (m_State.Gizmo.SnapEnabled)
        {
            switch (m_State.Gizmo.Operation)
            {
                case GizmoOperation::Translate:
                    snapValues[0] = m_State.Gizmo.TranslateSnap[0];
                    snapValues[1] = m_State.Gizmo.TranslateSnap[1];
                    snapValues[2] = m_State.Gizmo.TranslateSnap[2];
                    snapPointer = snapValues;
                    break;

                case GizmoOperation::Rotate:
                    snapValues[0] = m_State.Gizmo.RotateSnap;
                    snapPointer = snapValues;
                    break;

                case GizmoOperation::Scale:
                    snapValues[0] = m_State.Gizmo.ScaleSnap;
                    snapPointer = snapValues;
                    break;
            }
        }

        const bool changed = ImGuizmo::Manipulate(
            glm::value_ptr(cameraData.View),
            glm::value_ptr(cameraData.Projection),
            operation,
            mode,
            glm::value_ptr(model),
            nullptr,
            snapPointer
        );

        const bool isUsingGizmo = ImGuizmo::IsUsing();
        if ((changed || isUsingGizmo) && !m_WasUsingGizmo)
        {
            BeginSceneMutation(scene);
        }

        if (changed || isUsingGizmo)
        {
            float outTranslation[3]{};
            float outRotation[3]{};
            float outScale[3]{};

            ImGuizmo::DecomposeMatrixToComponents(
                glm::value_ptr(model),
                outTranslation,
                outRotation,
                outScale
            );

            transform.Position = { outTranslation[0], outTranslation[1], outTranslation[2] };
            transform.RotationDegrees = { outRotation[0], outRotation[1], outRotation[2] };
            transform.Scale = { outScale[0], outScale[1], outScale[2] };
        }

        if (!isUsingGizmo && m_WasUsingGizmo)
        {
            EndSceneMutation(scene);
        }

        m_WasUsingGizmo = isUsingGizmo;
    }

    void EditorController::DrawViewportDebugIcons(const Scene::Scene &scene,
                                                  const Scene::FreeCamera &camera,
                                                  const Renderer::Vulkan::VulkanRenderer &renderer) const
    {
        if (m_State.Scene.Mode == SceneMode::Play) return;
        if (!renderer.IsSceneViewportReady()) return;
        if (m_State.Viewport.Width == 0 || m_State.Viewport.Height == 0) return;

        const VkExtent2D sceneExtent = renderer.GetSceneViewportExtent();
        if (sceneExtent.width == 0 || sceneExtent.height == 0) return;

        const float aspect =
            static_cast<float>(sceneExtent.width) /
            static_cast<float>(sceneExtent.height);

        const auto cameraData =
            camera.BuildUniformData(aspect, Renderer::UseEditorScreenProjectionYFlip);

        ImDrawList* drawList = ImGui::GetWindowDrawList();

        const ImVec2 viewportMin(
            m_State.Viewport.ScreenX,
            m_State.Viewport.ScreenY
        );

        const ImVec2 viewportMax(
            m_State.Viewport.ScreenX + static_cast<float>(m_State.Viewport.Width),
            m_State.Viewport.ScreenY + static_cast<float>(m_State.Viewport.Height)
        );

        drawList->PushClipRect(viewportMin, viewportMax, true);
        for (const Scene::Actor& actor : scene.GetActors())
        {
            if (!actor.Camera.has_value() && !actor.DirectionalLight.has_value())
                continue;

            ImVec2 screenPosition{};
            if (!ProjectWorldToViewport(
                    actor.Transform.WorldTransform.Position,
                    cameraData,
                    m_State.Viewport,
                    screenPosition))
            {
                continue;
            }

            const bool selected =
                m_State.Selection.SelectedActorID.has_value() &&
                m_State.Selection.SelectedActorID.value() == actor.ID;

            float verticalOffset = 0.0f;

            if (actor.Camera.has_value())
            {
                const ImVec2 iconPosition(
                    screenPosition.x,
                    screenPosition.y + verticalOffset
                );

                DrawCameraDebugIcon(drawList, iconPosition, selected);
                DrawDebugLabel(drawList, iconPosition, "Camera", selected);

                verticalOffset += 22.0f;
            }

            if (actor.DirectionalLight.has_value())
            {
                const ImVec2 iconPosition(
                    screenPosition.x,
                    screenPosition.y + verticalOffset
                );

                DrawDirectionalLightDebugIcon(drawList, iconPosition, selected);
                DrawDebugLabel(drawList, iconPosition, "Light", selected);
            }
        }

        if (m_State.Selection.SelectedActorID.has_value())
        {
            const Scene::Actor* selectedActor =
                scene.FindActorByID(m_State.Selection.SelectedActorID.value());

            if (selectedActor)
            {
                DrawSelectedActorViewportOverlay(
                    *selectedActor,
                    cameraData,
                    m_State.Viewport,
                    drawList
                );
            }
        }

        drawList->PopClipRect();
    }

    bool EditorController::BuildViewportPickRay(const Input::Input& input,
                                                const Scene::FreeCamera& camera,
                                                const Renderer::Vulkan::VulkanRenderer& renderer,
                                                Math::Ray& outRay) const
    {
        const VkExtent2D sceneExtent = renderer.GetSceneViewportExtent();
        if (sceneExtent.width == 0 || sceneExtent.height == 0) return false;
        if (m_State.Viewport.Width == 0 || m_State.Viewport.Height == 0) return false;

        const auto [mouseX, mouseY] = input.GetMousePosition();
        const float localX = static_cast<float>(mouseX) - m_State.Viewport.ScreenX;
        const float localY = static_cast<float>(mouseY) - m_State.Viewport.ScreenY;

        if (localX < 0.0f || localY < 0.0f ||
            localX > static_cast<float>(m_State.Viewport.Width) ||
            localY > static_cast<float>(m_State.Viewport.Height))
        {
            return false;
        }

        const auto viewportHeight = static_cast<float>(m_State.Viewport.Height);
        const float flippedY = viewportHeight - localY;

        const float aspect =
            static_cast<float>(sceneExtent.width) /
            static_cast<float>(sceneExtent.height);

        const auto cameraData = camera.BuildUniformData(aspect, Renderer::UseEditorScreenProjectionYFlip);
        const glm::vec4 viewport(
            0.0f,
            0.0f,
            static_cast<float>(m_State.Viewport.Width),
            static_cast<float>(m_State.Viewport.Height)
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

        outRay.Origin = nearPoint;
        outRay.Direction = glm::normalize(farPoint - nearPoint);
        return true;
    }

}