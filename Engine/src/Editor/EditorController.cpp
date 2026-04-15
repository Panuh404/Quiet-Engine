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
#include "ImGuizmo.h"

#include "../../include/Quiet/Input/Input.h"
#include "Quiet/Math/Geometry.h"
#include "Quiet/Core/Log.h"
#include "Renderer/Vulkan/StaticMesh.h"
#include "Renderer/Vulkan/VulkanRenderer.h"
#include "Scene/Actor.h"
#include "Scene/FreeCamera.h"
#include "Scene/Scene.h"

namespace
{
    constexpr float TransformEpsilon = 0.0001f;

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

    bool AreFloatsEqual(float lhs, float rhs)
    {
        return std::fabs(lhs - rhs) <= TransformEpsilon;
    }

    bool AreVec3Equal(const glm::vec3& lhs, const glm::vec3& rhs)
    {
        return AreFloatsEqual(lhs.x, rhs.x) &&
               AreFloatsEqual(lhs.y, rhs.y) &&
               AreFloatsEqual(lhs.z, rhs.z);
    }

    bool AreTransformsEqual(const Quiet::Scene::Transform& lhs,
                            const Quiet::Scene::Transform& rhs)
    {
        return AreVec3Equal(lhs.Position, rhs.Position) &&
               AreVec3Equal(lhs.RotationDegrees, rhs.RotationDegrees) &&
               AreVec3Equal(lhs.Scale, rhs.Scale);
    }

    bool AreStaticMeshesEqual(const std::optional<Quiet::Scene::StaticMeshComponent>& lhs,
                              const std::optional<Quiet::Scene::StaticMeshComponent>& rhs)
    {
        if (lhs.has_value() != rhs.has_value()) return false;
        if (!lhs.has_value()) return true;

        return lhs->MeshResourceID == rhs->MeshResourceID &&
               lhs->Visible == rhs->Visible;
    }

    bool AreRotatorsEqual(const std::optional<Quiet::Scene::RotatorComponent>& lhs,
                          const std::optional<Quiet::Scene::RotatorComponent>& rhs)
    {
        if (lhs.has_value() != rhs.has_value()) return false;
        if (!lhs.has_value()) return true;

        return AreFloatsEqual(lhs->RotationSpeedDegreesPerSecond, rhs->RotationSpeedDegreesPerSecond);
    }

    bool AreActorsEqual(const Quiet::Scene::Actor& lhs,
                        const Quiet::Scene::Actor& rhs)
    {
        return lhs.ID == rhs.ID &&
               lhs.Name == rhs.Name &&
               AreTransformsEqual(lhs.Transform.WorldTransform, rhs.Transform.WorldTransform) &&
               AreStaticMeshesEqual(lhs.StaticMesh, rhs.StaticMesh) &&
               AreRotatorsEqual(lhs.Rotator, rhs.Rotator);
    }

    bool AreActorListsEqual(const std::vector<Quiet::Scene::Actor>& lhs,
                            const std::vector<Quiet::Scene::Actor>& rhs)
    {
        if (lhs.size() != rhs.size()) return false;
        for (std::size_t i = 0; i < lhs.size(); ++i)
        {
            if (!AreActorsEqual(lhs[i], rhs[i]))
                return false;
        }
        return true;
    }
}

namespace Quiet::Editor
{
    void EditorController::ResetSelection()
    {
        m_State.SelectedActorID.reset();
    }

    void EditorController::ResetSceneHistory(const Scene::Scene& scene)
    {
        m_UndoStack.clear();
        m_RedoStack.clear();
        m_PendingSnapshot.reset();
        m_CleanSnapshot = CaptureSnapshot(scene);
        RefreshHistoryFlags();
        RefreshDirtyState(scene);
    }

    void EditorController::MarkSceneSaved(const Scene::Scene& scene)
    {
        m_CleanSnapshot = CaptureSnapshot(scene);
        RefreshDirtyState(scene);
    }

    void EditorController::BeginSceneMutation(const Scene::Scene& scene)
    {
        if (m_State.CurrentSceneMode != SceneMode::Edit) return;
        if (!m_PendingSnapshot) m_PendingSnapshot = CaptureSnapshot(scene);
    }

    bool EditorController::EndSceneMutation(const Scene::Scene& scene)
    {
        if (!m_PendingSnapshot) return false;

        auto currentSnapshot = CaptureSnapshot(scene);
        if (AreSnapshotsEqual(*m_PendingSnapshot, currentSnapshot))
        {
            m_PendingSnapshot.reset();
            RefreshDirtyState(scene);
            return false;
        }

        PushUndoSnapshot(std::move(m_PendingSnapshot.value()));
        m_PendingSnapshot.reset();

        m_RedoStack.clear();
        RefreshHistoryFlags();
        RefreshDirtyState(scene);
        return true;
    }

    bool EditorController::Undo(Scene::Scene& scene)
    {
        if (m_State.CurrentSceneMode != SceneMode::Edit || m_UndoStack.empty())
        {
            RefreshHistoryFlags();
            return false;
        }
        PushRedoSnapshot(CaptureSnapshot(scene));

        auto snapshot = std::move(m_UndoStack.back());
        m_UndoStack.pop_back();
        RestoreSnapshot(scene, snapshot);

        m_PendingSnapshot.reset();
        RefreshHistoryFlags();
        RefreshDirtyState(scene);
        return true;
    }

    bool EditorController::Redo(Scene::Scene& scene)
    {
        if (m_State.CurrentSceneMode != SceneMode::Edit || m_RedoStack.empty())
        {
            RefreshHistoryFlags();
            return false;
        }
        PushUndoSnapshot(CaptureSnapshot(scene));

        SceneSnapshot snapshot = std::move(m_RedoStack.back());
        m_RedoStack.pop_back();
        RestoreSnapshot(scene, snapshot);

        m_PendingSnapshot.reset();
        RefreshHistoryFlags();
        RefreshDirtyState(scene);
        return true;
    }

    void EditorController::HandleShortcuts(const Input::Input& input, bool mouseCaptureEnabled)
    {
        if (mouseCaptureEnabled) return;

        const ImGuiIO& io = ImGui::GetIO();
        if (io.WantCaptureKeyboard || io.WantTextInput) return;
        if (m_State.CurrentSceneMode != SceneMode::Edit) return;

        using Input::KeyCode;

        const bool ctrlDown =
            input.IsKeyDown(KeyCode::LeftControl) ||
            input.IsKeyDown(KeyCode::RightControl);

        if (ctrlDown) return;
        if (input.IsKeyPressed(KeyCode::T)) m_State.CurrentGizmoOperation = GizmoOperation::Translate;
        if (input.IsKeyPressed(KeyCode::R)) m_State.CurrentGizmoOperation = GizmoOperation::Rotate;
        if (input.IsKeyPressed(KeyCode::S)) m_State.CurrentGizmoOperation = GizmoOperation::Scale;

        if (input.IsKeyPressed(KeyCode::L))
        {
            if (m_State.CurrentGizmoOperation != GizmoOperation::Scale)
            {
                m_State.CurrentGizmoMode =
                    (m_State.CurrentGizmoMode == GizmoMode::World)
                        ? GizmoMode::Local
                        : GizmoMode::World;
            }
        }

        if (input.IsKeyPressed(KeyCode::G)) m_State.GizmoSnapEnabled = !m_State.GizmoSnapEnabled;
    }

    void EditorController::FocusSelectedActor(Scene::Scene& scene,
                                              Scene::FreeCamera& camera)
    {
        if (!m_State.SelectedActorID.has_value()) return;

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

    void EditorController::TrySelectActorFromViewport(const Input::Input& input,
                                                      Scene::Scene& scene,
                                                      const Scene::FreeCamera& camera,
                                                      const Renderer::Vulkan::VulkanRenderer& renderer,
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
            m_State.SelectedActorID = bestHitID;
            QUIET_CORE_TRACE("Viewport pick selected actor.");
        }
        else
        {
            m_State.SelectedActorID.reset();
        }
    }

    void EditorController::BuildSelectedActorGizmo(Scene::Scene& scene,
                                                   Scene::FreeCamera& camera,
                                                   const Renderer::Vulkan::VulkanRenderer& renderer,
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
        if (m_State.ViewportWidth == 0 || m_State.ViewportHeight == 0) return false;

        const auto [mouseX, mouseY] = input.GetMousePosition();
        const float localX = static_cast<float>(mouseX) - m_State.ViewportScreenX;
        const float localY = static_cast<float>(mouseY) - m_State.ViewportScreenY;

        if (localX < 0.0f || localY < 0.0f ||
            localX > static_cast<float>(m_State.ViewportWidth) ||
            localY > static_cast<float>(m_State.ViewportHeight))
        {
            return false;
        }

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

    EditorController::SceneSnapshot EditorController::CaptureSnapshot(const Scene::Scene& scene) const
    {
        SceneSnapshot snapshot{};
        snapshot.Actors = scene.GetActors();
        snapshot.SelectedActorID = m_State.SelectedActorID;
        return snapshot;
    }

    void EditorController::RestoreSnapshot(Scene::Scene& scene, const SceneSnapshot& snapshot)
    {
        scene.GetActors() = snapshot.Actors;
        m_State.SelectedActorID = snapshot.SelectedActorID;
    }

    bool EditorController::AreSnapshotsEqual(const SceneSnapshot& lhs, const SceneSnapshot& rhs)
    {
        return lhs.SelectedActorID == rhs.SelectedActorID &&
               AreActorListsEqual(lhs.Actors, rhs.Actors);
    }

    void EditorController::PushUndoSnapshot(SceneSnapshot snapshot)
    {
        if (m_UndoStack.size() >= MaxHistoryEntries)
            m_UndoStack.erase(m_UndoStack.begin());

        m_UndoStack.push_back(std::move(snapshot));
    }

    void EditorController::PushRedoSnapshot(SceneSnapshot snapshot)
    {
        if (m_RedoStack.size() >= MaxHistoryEntries)
            m_RedoStack.erase(m_RedoStack.begin());

        m_RedoStack.push_back(std::move(snapshot));
    }

    void EditorController::RefreshHistoryFlags()
    {
        m_State.CanUndo = !m_UndoStack.empty();
        m_State.CanRedo = !m_RedoStack.empty();
    }

    void EditorController::RefreshDirtyState(const Scene::Scene& scene)
    {
        if (!m_CleanSnapshot.has_value())
        {
            m_State.SceneDirty = false;
            return;
        }

        const SceneSnapshot currentSnapshot = CaptureSnapshot(scene);
        m_State.SceneDirty = !AreActorListsEqual(m_CleanSnapshot->Actors, currentSnapshot.Actors);
    }
}