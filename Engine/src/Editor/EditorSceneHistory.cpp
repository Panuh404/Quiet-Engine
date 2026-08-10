#include "Quiet/Editor/EditorSceneHistory.h"

#include <cmath>
#include <utility>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

#include "Quiet/Editor/EditorState.h"
#include "Scene/Actor.h"
#include "Scene/Scene.h"

namespace
{
    constexpr float TransformEpsilon = 0.0001f;

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

    bool AreVec4Equal(const glm::vec4& lhs, const glm::vec4& rhs)
    {
        return AreFloatsEqual(lhs.x, rhs.x) &&
               AreFloatsEqual(lhs.y, rhs.y) &&
               AreFloatsEqual(lhs.z, rhs.z) &&
               AreFloatsEqual(lhs.w, rhs.w);
    }

    bool AreRenderSettingsEqual(const Quiet::Scene::SceneRenderSettings& lhs, const Quiet::Scene::SceneRenderSettings& rhs)
    {
        return AreVec3Equal(lhs.LightDirection, rhs.LightDirection) &&
               AreFloatsEqual(lhs.LightIntensity, rhs.LightIntensity) &&
               AreVec3Equal(lhs.AmbientColor, rhs.AmbientColor) &&
               AreFloatsEqual(lhs.AmbientIntensity, rhs.AmbientIntensity);
    }

    bool AreTransformsEqual(const Quiet::Scene::Transform& lhs,
                            const Quiet::Scene::Transform& rhs)
    {
        return AreVec3Equal(lhs.Position, rhs.Position) &&
               AreVec3Equal(lhs.RotationDegrees, rhs.RotationDegrees) &&
               AreVec3Equal(lhs.Scale, rhs.Scale);
    }

    bool AreRotatorsEqual(const std::optional<Quiet::Scene::RotatorComponent>& lhs,
                          const std::optional<Quiet::Scene::RotatorComponent>& rhs)
    {
        if (lhs.has_value() != rhs.has_value()) return false;
        if (!lhs.has_value()) return true;

        return AreFloatsEqual(lhs->RotationSpeedDegreesPerSecond, rhs->RotationSpeedDegreesPerSecond);
    }

    bool AreDirectionalLightsEqual(const std::optional<Quiet::Scene::DirectionalLightComponent>& lhs,
                                   const std::optional<Quiet::Scene::DirectionalLightComponent>& rhs)
    {
        if (lhs.has_value() != rhs.has_value()) return false;
        if (!lhs.has_value()) return true;

        return lhs->Enabled == rhs->Enabled &&
               AreVec3Equal(lhs->Direction, rhs->Direction) &&
               AreVec3Equal(lhs->Color, rhs->Color) &&
               AreFloatsEqual(lhs->Intensity, rhs->Intensity);
    }

    bool AreCamerasEqual(const std::optional<Quiet::Scene::CameraComponent>& lhs,
                         const std::optional<Quiet::Scene::CameraComponent>& rhs)
    {
        if (lhs.has_value() != rhs.has_value()) return false;
        if (!lhs.has_value()) return true;

        return lhs->Primary == rhs->Primary &&
               AreFloatsEqual(lhs->FieldOfViewDegrees, rhs->FieldOfViewDegrees) &&
               AreFloatsEqual(lhs->NearClip, rhs->NearClip) &&
               AreFloatsEqual(lhs->FarClip, rhs->FarClip);
    }

    bool AreMaterialsEqual(const Quiet::Scene::MaterialComponent& lhs,
                           const Quiet::Scene::MaterialComponent& rhs)
    {
        return AreVec4Equal(lhs.BaseColor, rhs.BaseColor) &&
               lhs.BaseColorTextureAssetID == rhs.BaseColorTextureAssetID;
    }

    bool AreOptionalMaterialsEqual(const std::optional<Quiet::Scene::MaterialComponent>& lhs,
                                   const std::optional<Quiet::Scene::MaterialComponent>& rhs)
    {
        if (lhs.has_value() != rhs.has_value()) return false;
        if (!lhs.has_value()) return true;
        return AreMaterialsEqual(lhs.value(), rhs.value());
    }

    bool AreMaterialSlotsEqual(const std::vector<Quiet::Scene::MaterialComponent>& lhs,
                               const std::vector<Quiet::Scene::MaterialComponent>& rhs)
    {
        if (lhs.size() != rhs.size()) return false;

        for (std::size_t i = 0; i < lhs.size(); ++i)
        {
            if (!AreMaterialsEqual(lhs[i], rhs[i]))
                return false;
        }

        return true;
    }

    bool AreStaticMeshesEqual(const std::optional<Quiet::Scene::StaticMeshComponent>& lhs,
                          const std::optional<Quiet::Scene::StaticMeshComponent>& rhs)
    {
        if (lhs.has_value() != rhs.has_value()) return false;
        if (!lhs.has_value()) return true;

        return lhs->MeshResourceID == rhs->MeshResourceID &&
               lhs->Visible == rhs->Visible &&
               AreMaterialSlotsEqual(lhs->MaterialSlots, rhs->MaterialSlots);
    }

    bool AreActorsEqual(const Quiet::Editor::EditorSceneHistory::ActorSnapshot& lhs, const Quiet::Editor::EditorSceneHistory::ActorSnapshot& rhs)
    {
        return
            lhs.ID == rhs.ID &&
            lhs.Name == rhs.Name &&
            AreTransformsEqual(lhs.Transform.WorldTransform, rhs.Transform.WorldTransform) &&
            AreStaticMeshesEqual(lhs.StaticMesh, rhs.StaticMesh) &&
            AreOptionalMaterialsEqual(lhs.Material, rhs.Material) &&
            AreDirectionalLightsEqual(lhs.DirectionalLight, rhs.DirectionalLight) &&
            AreRotatorsEqual(lhs.Rotator, rhs.Rotator) &&
            AreCamerasEqual(lhs.Camera, rhs.Camera);
    }

    bool AreActorListsEqual(const std::vector<Quiet::Editor::EditorSceneHistory::ActorSnapshot>& lhs,
                            const std::vector<Quiet::Editor::EditorSceneHistory::ActorSnapshot>& rhs)
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
    void EditorSceneHistory::Reset(const Scene::Scene& scene, EditorState& state)
    {
        m_UndoStack.clear();
        m_RedoStack.clear();
        m_PendingSnapshot.reset();
        m_CleanSnapshot = CaptureSnapshot(scene, state);

        RefreshHistoryFlags(state);
        RefreshDirtyState(scene, state);
    }

    void EditorSceneHistory::MarkSaved(const Scene::Scene& scene, EditorState& state)
    {
        m_CleanSnapshot = CaptureSnapshot(scene, state);
        RefreshDirtyState(scene, state);
    }

    void EditorSceneHistory::BeginMutation(const Scene::Scene& scene, const EditorState& state)
    {
        if (state.Scene.Mode != SceneMode::Edit) return;

        if (!m_PendingSnapshot)
        {
            m_PendingSnapshot = CaptureSnapshot(scene, state);
        }
    }

    bool EditorSceneHistory::EndMutation(const Scene::Scene& scene, EditorState& state)
    {
        if (!m_PendingSnapshot) return false;

        const SceneSnapshot currentSnapshot = CaptureSnapshot(scene, state);
        if (AreSnapshotsEqual(*m_PendingSnapshot, currentSnapshot))
        {
            m_PendingSnapshot.reset();
            RefreshDirtyState(scene, state);
            return false;
        }

        PushUndoSnapshot(std::move(m_PendingSnapshot.value()));
        m_PendingSnapshot.reset();

        m_RedoStack.clear();
        RefreshHistoryFlags(state);
        RefreshDirtyState(scene, state);
        return true;
    }

    bool EditorSceneHistory::Undo(Scene::Scene& scene, EditorState& state)
    {
        if (state.Scene.Mode != SceneMode::Edit || m_UndoStack.empty())
        {
            RefreshHistoryFlags(state);
            return false;
        }

        PushRedoSnapshot(CaptureSnapshot(scene, state));

        SceneSnapshot snapshot = std::move(m_UndoStack.back());
        m_UndoStack.pop_back();
        RestoreSnapshot(scene, state, snapshot);

        m_PendingSnapshot.reset();
        RefreshHistoryFlags(state);
        RefreshDirtyState(scene, state);
        return true;
    }

    bool EditorSceneHistory::Redo(Scene::Scene& scene, EditorState& state)
    {
        if (state.Scene.Mode != SceneMode::Edit || m_RedoStack.empty())
        {
            RefreshHistoryFlags(state);
            return false;
        }

        PushUndoSnapshot(CaptureSnapshot(scene, state));

        SceneSnapshot snapshot = std::move(m_RedoStack.back());
        m_RedoStack.pop_back();
        RestoreSnapshot(scene, state, snapshot);

        m_PendingSnapshot.reset();
        RefreshHistoryFlags(state);
        RefreshDirtyState(scene, state);
        return true;
    }

    EditorSceneHistory::SceneSnapshot EditorSceneHistory::CaptureSnapshot(const Scene::Scene& scene, const EditorState& state) const
    {
        SceneSnapshot snapshot{};

        const auto& actors = scene.GetActors();
        snapshot.Actors.reserve(actors.size());

        for (const Scene::Actor& actor : actors)
        {
            ActorSnapshot actorSnapshot{};
            actorSnapshot.ID = actor.GetID();
            actorSnapshot.Name = actor.GetName();
            actorSnapshot.Transform = actor.GetComponent<Scene::TransformComponent>();

            if (const auto* staticMesh = actor.TryGetComponent<Scene::StaticMeshComponent>())
            {
                actorSnapshot.StaticMesh = *staticMesh;
            }

            if (const auto* material = actor.TryGetComponent<Scene::MaterialComponent>())
            {
                actorSnapshot.Material = *material;
            }

            if (const auto* directionalLight = actor.TryGetComponent<Scene::DirectionalLightComponent>())
            {
                actorSnapshot.DirectionalLight = *directionalLight;
            }

            if (const auto* rotator = actor.TryGetComponent<Scene::RotatorComponent>())
            {
                actorSnapshot.Rotator = *rotator;
            }

            if (const auto* camera = actor.TryGetComponent<Scene::CameraComponent>())
            {
                actorSnapshot.Camera = *camera;
            }

            snapshot.Actors.push_back(std::move(actorSnapshot));
        }

        snapshot.Hierarchy = scene.GetHierarchy();
        snapshot.RenderSettings = scene.GetRenderSettings();
        snapshot.SelectedActorID = state.Selection.SelectedActorID;

        return snapshot;
    }

    void EditorSceneHistory::RestoreSnapshot(Scene::Scene& scene, EditorState& state, const SceneSnapshot& snapshot)
    {
        scene.Clear();

        for (const ActorSnapshot& actorSnapshot : snapshot.Actors)
        {
            Scene::Actor& actor = scene.CreateActorWithID(actorSnapshot.ID, actorSnapshot.Name);
            actor.GetComponent<Scene::TransformComponent>() = actorSnapshot.Transform;

            if (actorSnapshot.StaticMesh)
            {
                actor.AddComponent<Scene::StaticMeshComponent>(*actorSnapshot.StaticMesh);
            }

            if (actorSnapshot.Material)
            {
                actor.AddComponent<Scene::MaterialComponent>(*actorSnapshot.Material);
            }

            if (actorSnapshot.DirectionalLight)
            {
                actor.AddComponent<Scene::DirectionalLightComponent>(*actorSnapshot.DirectionalLight);
            }

            if (actorSnapshot.Rotator)
            {
                actor.AddComponent<Scene::RotatorComponent>(*actorSnapshot.Rotator);
            }

            if (actorSnapshot.Camera)
            {
                actor.AddComponent<Scene::CameraComponent>(*actorSnapshot.Camera);
            }
        }
        scene.SetHierarchy(snapshot.Hierarchy);
        scene.GetRenderSettings() = snapshot.RenderSettings;
        state.Selection.SelectedActorID = snapshot.SelectedActorID;
    }

    bool EditorSceneHistory::AreSnapshotsEqual(const SceneSnapshot& lhs, const SceneSnapshot& rhs)
    {
        return lhs.SelectedActorID == rhs.SelectedActorID &&
               AreActorListsEqual(lhs.Actors, rhs.Actors) &&
               lhs.Hierarchy == rhs.Hierarchy &&
               AreRenderSettingsEqual(lhs.RenderSettings, rhs.RenderSettings);
    }

    void EditorSceneHistory::PushUndoSnapshot(SceneSnapshot snapshot)
    {
        if (m_UndoStack.size() >= MaxHistoryEntries)
        {
            m_UndoStack.erase(m_UndoStack.begin());
        }
        m_UndoStack.push_back(std::move(snapshot));
    }

    void EditorSceneHistory::PushRedoSnapshot(SceneSnapshot snapshot)
    {
        if (m_RedoStack.size() >= MaxHistoryEntries)
        {
            m_RedoStack.erase(m_RedoStack.begin());
        }
        m_RedoStack.push_back(std::move(snapshot));
    }

    void EditorSceneHistory::RefreshHistoryFlags(EditorState& state) const
    {
        state.History.CanUndo = !m_UndoStack.empty();
        state.History.CanRedo = !m_RedoStack.empty();
    }

    void EditorSceneHistory::RefreshDirtyState(const Scene::Scene& scene, EditorState& state) const
    {
        if (!m_CleanSnapshot.has_value())
        {
            state.Scene.Dirty = false;
            return;
        }

        const SceneSnapshot currentSnapshot = CaptureSnapshot(scene, state);
        state.Scene.Dirty =
            !AreActorListsEqual(m_CleanSnapshot->Actors, currentSnapshot.Actors) ||
            m_CleanSnapshot->Hierarchy != currentSnapshot.Hierarchy ||
            !AreRenderSettingsEqual(m_CleanSnapshot->RenderSettings, currentSnapshot.RenderSettings);
    }
}