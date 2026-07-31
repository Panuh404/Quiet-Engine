#include "Quiet/Editor/EditorController.h"

#include <memory>
#include <cmath>
#include <limits>
#include <utility>


#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/ext/matrix_projection.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "imgui.h"

#include "Quiet/Input/Input.h"
#include "Quiet/Math/Geometry.h"
#include "Quiet/Core/Log.h"
#include "Renderer/Vulkan/Mesh/StaticMesh.h"
#include "Renderer/Vulkan/VulkanRenderer.h"
#include "Scene/Actor.h"
#include "Scene/Camera/FreeCamera.h"
#include "Scene/Scene.h"

namespace Quiet::Editor
{
    void EditorController::ResetSelection()
    {
        m_State.Selection.SelectedActorID.reset();
    }

    void EditorController::ResetSceneHistory(const Scene::Scene& scene)
    {
        m_History.Reset(scene, m_State);
    }

    void EditorController::MarkSceneSaved(const Scene::Scene& scene)
    {
        m_History.MarkSaved(scene, m_State);
    }

    void EditorController::BeginSceneMutation(const Scene::Scene& scene)
    {
        m_History.BeginMutation(scene, m_State);
    }

    bool EditorController::EndSceneMutation(const Scene::Scene& scene)
    {
        return m_History.EndMutation(scene, m_State);
    }

    bool EditorController::Undo(Scene::Scene& scene)
    {
        return m_History.Undo(scene, m_State);
    }

    bool EditorController::Redo(Scene::Scene& scene)
    {
        return m_History.Redo(scene, m_State);
    }

    void EditorController::HandleShortcuts(const Input::Input& input, bool mouseCaptureEnabled)
    {
        if (mouseCaptureEnabled) return;

        const ImGuiIO& io = ImGui::GetIO();
        if (io.WantCaptureKeyboard || io.WantTextInput) return;
        if (m_State.Scene.Mode != SceneMode::Edit) return;

        using Input::KeyCode;

        const bool ctrlDown =
            input.IsKeyDown(KeyCode::LeftControl) ||
            input.IsKeyDown(KeyCode::RightControl);

        if (ctrlDown) return;

        if (m_State.Viewport.NavigationCaptureRequested != ViewportNavigationMode::None ||
            m_State.Viewport.NavigationMode != ViewportNavigationMode::None)
        {
            return;
        }

        if (input.IsKeyPressed(KeyCode::T)) m_State.Gizmo.Operation = GizmoOperation::Translate;
        if (input.IsKeyPressed(KeyCode::R)) m_State.Gizmo.Operation = GizmoOperation::Rotate;
        if (input.IsKeyPressed(KeyCode::S)) m_State.Gizmo.Operation = GizmoOperation::Scale;
        if (input.IsKeyPressed(KeyCode::L))
        {
            if (m_State.Gizmo.Operation != GizmoOperation::Scale)
            {
                m_State.Gizmo.Mode =
                    (m_State.Gizmo.Mode == GizmoMode::World)
                        ? GizmoMode::Local
                        : GizmoMode::World;
            }
        }
        if (input.IsKeyPressed(KeyCode::G)) m_State.Gizmo.SnapEnabled = !m_State.Gizmo.SnapEnabled;
    }

    void EditorController::FocusSelectedActor(Scene::Scene& scene, Scene::FreeCamera& camera)
    {
        if (!m_State.Selection.SelectedActorID.has_value()) return;

        Scene::Actor* actor = scene.FindActorByID(m_State.Selection.SelectedActorID.value());
        if (!actor)
        {
            m_State.Selection.SelectedActorID.reset();
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

            const Math::AABB worldBounds = Math::TransformAABB(
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

        QUIET_CORE_TRACE("Focused selected actor at ({:.2f}, {:.2f}, {:.2f}).", target.x, target.y, target.z);
    }
}