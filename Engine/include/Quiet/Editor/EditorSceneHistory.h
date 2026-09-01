#pragma once

#include <string>
#include <optional>
#include <vector>

#include "Quiet/Core/UUID.h"
#include "Scene/Actor.h"
#include "Scene/SceneAudioSettings.h"
#include "Scene/SceneHierarchy.h"
#include "Scene/ScenePhysicsSettings.h"
#include "Scene/SceneRenderSettings.h"

namespace Quiet::Editor
{
    struct EditorState;
}

namespace Quiet::Scene
{
    class Scene;
}

namespace Quiet::Editor
{
    class EditorSceneHistory
    {
    public:
        struct ActorSnapshot
        {
            Core::UUID ID{};
            std::string Name;
            Scene::TransformComponent Transform{};
            std::optional<Scene::StaticMeshComponent> StaticMesh;
            std::optional<Scene::MaterialComponent> Material;
            std::optional<Scene::DirectionalLightComponent> DirectionalLight;
            std::optional<Scene::RotatorComponent> Rotator;
            std::optional<Scene::CameraComponent> Camera;
            std::optional<Scene::AudioSourceComponent> AudioSource;
            std::optional<Scene::AudioListenerComponent> AudioListener;
            std::optional<Scene::ScriptComponent> Script;
            std::optional<Scene::RigidBodyComponent> RigidBody;
            std::optional<Scene::BoxColliderComponent> BoxCollider;
            std::optional<Scene::SphereColliderComponent> SphereCollider;
            std::optional<Scene::CapsuleColliderComponent> CapsuleCollider;
        };

        struct SceneSnapshot
        {
            std::vector<ActorSnapshot> Actors;
            Scene::SceneHierarchyState Hierarchy;
            Scene::SceneAudioSettings AudioSettings;
            Scene::ScenePhysicsSettings PhysicsSettings;
            Scene::SceneRenderSettings RenderSettings;
            std::optional<Core::UUID> SelectedActorID;
        };

        void Reset(const Scene::Scene& scene, EditorState& state);
        void MarkSaved(const Scene::Scene& scene, EditorState& state);

        void BeginMutation(const Scene::Scene& scene, const EditorState& state);
        bool EndMutation(const Scene::Scene& scene, EditorState& state);

        bool Undo(Scene::Scene& scene, EditorState& state);
        bool Redo(Scene::Scene& scene, EditorState& state);

    private:
        [[nodiscard]] SceneSnapshot CaptureSnapshot(const Scene::Scene& scene, const EditorState& state) const;
        void RestoreSnapshot(Scene::Scene& scene, EditorState& state, const SceneSnapshot& snapshot);

        static bool AreSnapshotsEqual(const SceneSnapshot& lhs, const SceneSnapshot& rhs);

        void PushUndoSnapshot(SceneSnapshot snapshot);
        void PushRedoSnapshot(SceneSnapshot snapshot);

        void RefreshHistoryFlags(EditorState& state) const;
        void RefreshDirtyState(const Scene::Scene& scene, EditorState& state) const;

    private:
        static constexpr std::size_t MaxHistoryEntries = 128;

        std::vector<SceneSnapshot> m_UndoStack;
        std::vector<SceneSnapshot> m_RedoStack;
        std::optional<SceneSnapshot> m_PendingSnapshot;
        std::optional<SceneSnapshot> m_CleanSnapshot;

    };
}