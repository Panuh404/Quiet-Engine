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
#include "Scene/Actor.h"
#include "Scene/Camera/FreeCamera.h"
#include "Scene/Scene.h"

#include "Editor/Viewport/EditorViewportOverlay.h"
#include "Editor/Viewport/EditorViewportPicking.h"
#include "Editor/Viewport/EditorViewportProjection.h"
#include "Renderer/Vulkan/Mesh/StaticMesh.h"

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
        if (m_State.Viewport.ShowDebugIcons)
        {
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
        }

        Math::Ray ray{};
        if (!BuildViewportPickRay(
                mousePosition.x,
                mousePosition.y,
                m_State.Viewport,
                camera,
                renderer,
                ray))
            return;

        const std::optional<Core::UUID> staticMeshHit =
            PickStaticMeshActor(scene, ray);

        if (staticMeshHit.has_value())
        {
            m_State.Selection.SelectedActorID = staticMeshHit;
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

        if (m_State.Viewport.ShowDebugIcons)
        {
            for (const Scene::Actor& actor : scene.GetActors())
            {
                if (!actor.Camera.has_value() && !actor.DirectionalLight.has_value())
                {
                    continue;
                }

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

                if (actor.Camera.has_value())
                {
                    DrawCameraFrustumOverlay(
                        drawList,
                        actor,
                        actor.Camera.value(),
                        cameraData,
                        m_State.Viewport,
                        selected
                    );
                }

                if (actor.DirectionalLight.has_value())
                {
                    DrawDirectionalLightDirectionOverlay(
                        drawList,
                        actor,
                        actor.DirectionalLight.value(),
                        cameraData,
                        m_State.Viewport,
                        selected
                    );
                }

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
}