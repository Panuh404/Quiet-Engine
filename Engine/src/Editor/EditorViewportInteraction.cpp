#include "Quiet/Editor/EditorController.h"

#include <limits>

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

        Math::Ray ray{};
        if (!BuildViewportPickRay(input, camera, renderer, ray))
        {
            return;
        }

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