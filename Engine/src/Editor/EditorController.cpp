#include "Quiet/Editor/EditorController.h"

#include <limits>
#include <stdexcept>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/ext/matrix_projection.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "imgui.h"
#include "ImGuizmo.h"

#include "Input/Input.h"
#include "Quiet/Math/Geometry.h"
#include "Quiet/Core/Log.h"
#include "Renderer/Vulkan/StaticMesh.h"
#include "Renderer/Vulkan/VulkanRenderer.h"
#include "Scene/FreeCamera.h"
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
    void EditorController::ResetSelection()
    {
        m_State.SelectedActorID.reset();
    }

    void EditorController::HandleShortcuts(bool mouseCaptureEnabled)
    {
        if (mouseCaptureEnabled) return;
        const ImGuiIO& io = ImGui::GetIO();
        if (io.WantCaptureKeyboard || io.WantTextInput) return;

        const bool ctrlDown = io.KeyCtrl;
        const bool isEditMode = m_State.CurrentSceneMode == SceneMode::Edit;

        if (ctrlDown && ImGui::IsKeyPressed(ImGuiKey_S, false))
        {
            if (isEditMode) m_State.RequestSaveScene = true;
            return;
        }
        if (ImGui::IsKeyPressed(ImGuiKey_F, false)) m_State.RequestFocusSelected = true;
        if (!isEditMode) return;
        if (ImGui::IsKeyPressed(ImGuiKey_T, false)) m_State.CurrentGizmoOperation = GizmoOperation::Translate;
        if (ImGui::IsKeyPressed(ImGuiKey_R, false)) m_State.CurrentGizmoOperation = GizmoOperation::Rotate;
        if (ImGui::IsKeyPressed(ImGuiKey_S, false)) m_State.CurrentGizmoOperation = GizmoOperation::Scale;

        if (ImGui::IsKeyPressed(ImGuiKey_L, false))
        {
            if (m_State.CurrentGizmoOperation != GizmoOperation::Scale)
            {
                m_State.CurrentGizmoMode =
                    (m_State.CurrentGizmoMode == GizmoMode::World)
                        ? GizmoMode::Local
                        : GizmoMode::World;
            }
        }

        if (ImGui::IsKeyPressed(ImGuiKey_G, false)) m_State.GizmoSnapEnabled = !m_State.GizmoSnapEnabled;
    }

    void EditorController::FocusSelectedActor(Scene::Scene& scene,
                                              Scene::FreeCamera& camera)
    {
        if (!m_State.SelectedActorID.has_value())
        {
            return;
        }

        Scene::Actor* actor = scene.FindActorByID(m_State.SelectedActorID.value());
        if (!actor)
        {
            m_State.SelectedActorID.reset();
            return;
        }

        glm::vec3 target = actor->Transform.WorldTransform.Position;
        float radius = 1.0f;

        if (actor->StaticMesh.has_value() &&
            actor->StaticMesh->IsValid() &&
            actor->StaticMesh->Visible)
        {
            const auto& mesh = actor->StaticMesh->Mesh;
            const auto localBounds = mesh->GetLocalBounds();

            const Quiet::Math::AABB worldBounds = Quiet::Math::TransformAABB(
                actor->Transform.WorldTransform.ToMatrix(),
                localBounds.Min,
                localBounds.Max
            );

            target = (worldBounds.Min + worldBounds.Max) * 0.5f;
            radius = glm::max(0.5f, glm::length(worldBounds.Max - worldBounds.Min) * 0.5f);
        }

        const float fovRadians = glm::radians(camera.GetFovDegrees());
        float distance = radius / std::tan(fovRadians * 0.5f);
        distance = glm::max(distance * 1.5f, 1.5f);

        const glm::vec3 forward = camera.GetForwardVector();
        const glm::vec3 newPosition = target - forward * distance;

        camera.SetPosition(newPosition);

        QUIET_CORE_TRACE(
            "Focused selected actor at ({:.2f}, {:.2f}, {:.2f}).",
            target.x,
            target.y,
            target.z
        );
    }

    void EditorController::TrySelectActorFromViewport(const Input::Input &input,
                                                      Scene::Scene &scene,
                                                      const Scene::FreeCamera &camera,
                                                      const Renderer::Vulkan::VulkanRenderer &renderer,
                                                      bool mouseCaptureEnabled)
    {
        if (mouseCaptureEnabled) return;
        if (!m_State.ViewportImageHovered) return;
        if (!input.IsMouseButtonPressed(Quiet::Input::MouseCode::ButtonLeft)) return;
        if (ImGuizmo::IsOver() || ImGuizmo::IsUsing()) return;

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
                continue;

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
            m_State.SelectedActorID = bestHitID;
            QUIET_CORE_TRACE("Viewport pick selected actor.");
        }
        else
        {
            m_State.SelectedActorID.reset();
        }
    }

    void EditorController::BuildSelectedActorGizmo(Scene::Scene &scene,
                                                   Scene::FreeCamera &camera,
                                                   const Renderer::Vulkan::VulkanRenderer &renderer,
                                                   bool mouseCaptureEnabled)
    {
        if (mouseCaptureEnabled) return;
        if (m_State.CurrentSceneMode != SceneMode::Edit) return;
        if (!renderer.IsSceneViewportReady()) return;
        if (!m_State.SelectedActorID.has_value()) return;
        if (m_State.ViewportWidth == 0 || m_State.ViewportHeight == 0) return;
        if (!m_State.ViewportImageHovered && !ImGuizmo::IsUsing()) return;

        const VkExtent2D sceneExtent = renderer.GetSceneViewportExtent();
        const float aspect =
            static_cast<float>(sceneExtent.width) /
            static_cast<float>(sceneExtent.height);

        const auto cameraData = camera.BuildUniformData(aspect, false);
        Scene::Actor* actor = scene.FindActorByID(m_State.SelectedActorID.value());
        if (!actor)
        {
            m_State.SelectedActorID.reset();
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
            m_State.ViewportScreenX,
            m_State.ViewportScreenY,
            static_cast<float>(m_State.ViewportWidth),
            static_cast<float>(m_State.ViewportHeight)
        );

        const ImGuizmo::OPERATION operation = ToImGuizmoOperation(m_State.CurrentGizmoOperation);
        const ImGuizmo::MODE mode = ToImGuizmoMode(m_State.CurrentGizmoMode);

        float snapValues[3] = { 0.0f, 0.0f, 0.0f };
        const float* snapPointer = nullptr;

        if (m_State.GizmoSnapEnabled)
        {
            switch (m_State.CurrentGizmoOperation)
            {
            case GizmoOperation::Translate:
                snapValues[0] = m_State.TranslateSnap[0];
                snapValues[1] = m_State.TranslateSnap[1];
                snapValues[2] = m_State.TranslateSnap[2];
                snapPointer = snapValues;
                break;

            case GizmoOperation::Rotate:
                snapValues[0] = m_State.RotateSnap;
                snapPointer = snapValues;
                break;

            case GizmoOperation::Scale:
                snapValues[0] = m_State.ScaleSnap;
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

        if (changed || ImGuizmo::IsUsing())
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

            if (changed) m_State.SceneDirty = true;
        }
    }

    bool EditorController::BuildViewportPickRay(const Input::Input &input,
                                                const Scene::FreeCamera &camera,
                                                const Renderer::Vulkan::VulkanRenderer &renderer,
                                                Math::Ray &outRay) const
    {
        const VkExtent2D sceneExtent = renderer.GetSceneViewportExtent();
        if (sceneExtent.width == 0 || sceneExtent.height == 0) return false;
        if (m_State.ViewportWidth == 0 || m_State.ViewportHeight == 0) return false;

        const auto [mouseX, mouseY] = input.GetMousePosition();
        const float localX = static_cast<float>(mouseX) - m_State.ViewportScreenX;
        const float localY = static_cast<float>(mouseY) - m_State.ViewportScreenY;

        if (localX < 0.0f || localY < 0.0f ||
            localX > static_cast<float>(m_State.ViewportWidth) ||
            localY > static_cast<float>(m_State.ViewportHeight))
            return false;

        const auto viewportHeight = static_cast<float>(m_State.ViewportHeight);
        const float flippedY = viewportHeight - localY;

        const float aspect =
            static_cast<float>(sceneExtent.width) /
            static_cast<float>(sceneExtent.height);

        const auto cameraData = camera.BuildUniformData(aspect, false);
        const glm::vec4 viewport(
            0.0f,
            0.0f,
            static_cast<float>(m_State.ViewportWidth),
            static_cast<float>(m_State.ViewportHeight)
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
