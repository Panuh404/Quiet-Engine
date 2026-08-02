#include "Quiet/Editor/EditorController.h"

#include <optional>

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "imgui.h"
#include "ImGuizmo.h"

#include "Quiet/Core/Log.h"
#include "Quiet/Input/Input.h"
#include "Quiet/Renderer/RenderConventions.h"

#include "Renderer/Vulkan/VulkanRenderer.h"
#include "Renderer/Vulkan/Mesh/MeshLibrary.h"
#include "Renderer/Vulkan/Mesh/StaticMesh.h"

#include "Scene/Actor.h"
#include "Scene/Scene.h"
#include "Scene/Camera/FreeCamera.h"

#include "Editor/Viewport/EditorViewportOverlay.h"
#include "Editor/Viewport/EditorViewportPicking.h"
#include "Editor/Viewport/EditorViewportProjection.h"
#include "Editor/Viewport/EditorViewportGizmo.h"

namespace
{
    std::optional<Quiet::Core::UUID> PickActorAtViewportPosition(const Quiet::Scene::Scene& scene,
                                                                 const Quiet::Scene::FreeCamera& camera,
                                                                 const Quiet::Renderer::Vulkan::VulkanRenderer& renderer,
                                                                 const Quiet::Editor::EditorViewportState& viewportState,
                                                                 const ImVec2& mousePosition)
    {
        const VkExtent2D sceneExtent = renderer.GetSceneViewportExtent();
        if (sceneExtent.width == 0 || sceneExtent.height == 0) return std::nullopt;

        const float aspect = static_cast<float>(sceneExtent.width) / static_cast<float>(sceneExtent.height);
        const auto cameraData = camera.BuildUniformData(aspect, Quiet::Renderer::UseEditorScreenProjectionYFlip);

        if (viewportState.ShowDebugIcons)
        {
            const auto debugIconHit = Quiet::Editor::PickViewportDebugIcon(scene, cameraData, viewportState, mousePosition);
            if (debugIconHit.has_value()) return debugIconHit;
        }

        Quiet::Math::Ray ray{};
        if (!Quiet::Editor::BuildViewportPickRay(mousePosition.x, mousePosition.y, viewportState, camera, renderer, ray))
        {
            return std::nullopt;
        }

        return Quiet::Editor::PickStaticMeshActor(scene, ray);
    }
}

namespace Quiet::Editor
{
    void EditorController::TrySelectActorFromViewport(const Input::Input& input, Scene::Scene& scene, const bool mouseCaptureEnabled)
    {
        if (mouseCaptureEnabled) return;
        if (m_State.Scene.Mode != SceneMode::Edit) return;
        if (m_State.Viewport.NavigationCaptureRequested != ViewportNavigationMode::None ||
            m_State.Viewport.NavigationMode != ViewportNavigationMode::None) return;

        const bool altDown = input.IsKeyDown(Input::KeyCode::LeftAlt) || input.IsKeyDown(Input::KeyCode::RightAlt);
        if (altDown) return;
        if (!m_State.Viewport.ImageHovered) return;
        if (ImGui::GetDragDropPayload()) return;
        if (!input.IsMouseButtonPressed(Input::MouseCode::ButtonLeft)) return;
        if (ImGuizmo::IsUsing() || ImGuizmo::IsOver()) return;

        const auto hoveredActorID = m_State.Selection.HoveredActorID;
        if (hoveredActorID.has_value() && scene.FindActorByID(hoveredActorID.value()))
        {
            m_State.Selection.SelectedActorID = hoveredActorID;
            QUIET_CORE_TRACE("Viewport selected hovered actor.");
        }
        else
        {
            m_State.Selection.SelectedActorID.reset();
        }
    }

    void EditorController::UpdateHoveredActorFromViewport(const Scene::Scene& scene,
                                                          const Scene::FreeCamera& camera,
                                                          const Renderer::Vulkan::VulkanRenderer& renderer,
                                                          const bool mouseCaptureEnabled)
    {
        auto& hoveredActorID = m_State.Selection.HoveredActorID;
        hoveredActorID.reset();

        if (mouseCaptureEnabled) return;
        if (m_State.Scene.Mode != SceneMode::Edit) return;
        if (!m_State.Viewport.ImageHovered) return;
        if (m_State.Viewport.NavigationCaptureRequested != ViewportNavigationMode::None ||
            m_State.Viewport.NavigationMode != ViewportNavigationMode::None) return;
        if (ImGui::GetDragDropPayload()) return;
        if (ImGuizmo::IsUsing() || ImGuizmo::IsOver()) return;

        hoveredActorID = PickActorAtViewportPosition(scene, camera, renderer, m_State.Viewport, ImGui::GetMousePos());
    }

    void EditorController::BuildSelectedActorGizmo(Scene::Scene& scene,
                                                   Scene::FreeCamera& camera,
                                                   const Renderer::Vulkan::VulkanRenderer& renderer,
                                                   bool mouseCaptureEnabled)
    {
        if (mouseCaptureEnabled) return;
        if (m_State.Scene.Mode != SceneMode::Edit) return;
        if (m_State.Viewport.NavigationCaptureRequested != ViewportNavigationMode::None || m_State.Viewport.NavigationMode != ViewportNavigationMode::None) return;
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
        const GizmoTransformComponents components = MakeGizmoTransformComponents(transform);
        glm::mat4 model = ComposeGizmoMatrix(components);

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

        const GizmoSnapSettings snapSettings = BuildGizmoSnapSettings(m_State.Gizmo);

        const bool changed = ImGuizmo::Manipulate(
            glm::value_ptr(cameraData.View),
            glm::value_ptr(cameraData.Projection),
            operation,
            mode,
            glm::value_ptr(model),
            nullptr,
            snapSettings.GetPointer()
        );

        const bool isUsingGizmo = ImGuizmo::IsUsing();
        if ((changed || isUsingGizmo) && !m_WasUsingGizmo)
            BeginSceneMutation(scene);

        if (changed || isUsingGizmo)
            ApplyGizmoMatrixToTransform(model, transform);

        if (!isUsingGizmo && m_WasUsingGizmo)
            EndSceneMutation(scene);

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

        const float aspect = static_cast<float>(sceneExtent.width) / static_cast<float>(sceneExtent.height);
        const auto cameraData = camera.BuildUniformData(aspect, Renderer::UseEditorScreenProjectionYFlip);

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

        if (m_State.Viewport.ShowDebugIcons)
        {
            for (const Scene::Actor& actor : scene.GetActors())
            {
                if (!actor.Camera.has_value() && !actor.DirectionalLight.has_value())
                    continue;

                ImVec2 screenPosition{};
                if (!ProjectWorldToViewport(actor.Transform.WorldTransform.Position, cameraData, m_State.Viewport, screenPosition))
                    continue;

                const bool actorSelected =
                    m_State.Selection.SelectedActorID.has_value() &&
                    m_State.Selection.SelectedActorID.value() == actor.ID;

                const bool actorHovered = !actorSelected &&
                    m_State.Selection.HoveredActorID.has_value() &&
                    m_State.Selection.HoveredActorID.value() == actor.ID;

                const bool selected = m_State.Viewport.ShowSelectionFeedback && actorSelected;
                const bool hovered = m_State.Viewport.ShowHoverFeedback && actorHovered;

                if (actor.Camera.has_value())
                    DrawCameraFrustumOverlay(drawList, actor, actor.Camera.value(), cameraData, m_State.Viewport, selected, hovered);

                if (actor.DirectionalLight.has_value())
                    DrawDirectionalLightDirectionOverlay(drawList, actor, actor.DirectionalLight.value(), cameraData, m_State.Viewport, selected, hovered);

                float verticalOffset = 0.0f;

                if (actor.Camera.has_value())
                {
                    const ImVec2 iconPosition(screenPosition.x, screenPosition.y + verticalOffset);

                    DrawCameraDebugIcon(drawList, iconPosition, selected, hovered);
                    DrawDebugLabel(drawList, iconPosition, "Camera", selected, hovered);

                    verticalOffset += 22.0f;
                }

                if (actor.DirectionalLight.has_value())
                {
                    const ImVec2 iconPosition(screenPosition.x, screenPosition.y + verticalOffset);

                    DrawDirectionalLightDebugIcon(drawList, iconPosition, selected, hovered);
                    DrawDebugLabel(drawList, iconPosition, "Light", selected, hovered);
                }
            }
        }

        if (m_State.Viewport.ActiveMeshAssetDrop.has_value())
        {
            const EditorViewportAssetDropRequest& previewRequest = m_State.Viewport.ActiveMeshAssetDrop.value();
            const auto previewMesh = renderer.GetMeshLibrary() .TryGet(previewRequest.AssetID);
            Math::Ray previewRay{};

            if (previewMesh && BuildViewportPickRay(previewRequest.ScreenX, previewRequest.ScreenY, m_State.Viewport, camera, renderer, previewRay))
            {
                const glm::vec3 previewPosition = CalculateViewportMeshPlacementPosition(previewRay, *previewMesh);
                DrawViewportMeshDropPreview(
                    *previewMesh,
                    previewPosition,
                    cameraData,
                    m_State.Viewport,
                    drawList
                );
            }
        }

        if (m_State.Viewport.ShowHoverFeedback && m_State.Selection.HoveredActorID.has_value())
        {
            const Core::UUID hoveredActorID = m_State.Selection.HoveredActorID.value();

            const bool hoveredActorIsSelected =
                m_State.Selection.SelectedActorID.has_value() &&
                m_State.Selection.SelectedActorID.value() == hoveredActorID;

            if (!hoveredActorIsSelected)
            {
                const Scene::Actor* hoveredActor = scene.FindActorByID(hoveredActorID);
                if (hoveredActor)
                {
                    DrawHoveredActorViewportOverlay(*hoveredActor, cameraData, m_State.Viewport, drawList);
                }
            }
        }

        if (m_State.Viewport.ShowSelectionFeedback && m_State.Selection.SelectedActorID.has_value())
        {
            const Scene::Actor* selectedActor = scene.FindActorByID(m_State.Selection.SelectedActorID.value());
            if (selectedActor)
            {
                DrawSelectedActorViewportOverlay(*selectedActor, cameraData, m_State.Viewport, drawList);
            }
        }

        drawList->PopClipRect();
    }
}