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
    /**
     * @brief Records supported Scene snapshots for undo, redo, and dirty tracking.
     * @ingroup quiet_editor
     *
     * Maintains one pending mutation boundary and a clean comparison baseline.
     * Snapshot restoration reconstructs Actors and invalidates existing handles.
     */
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
            std::optional<Scene::TextComponent> Text;
            std::optional<Scene::UIRectComponent> UIRect;
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

        /**
         * @brief Clears history and establishes the current Scene as clean.
         */
        void Reset(const Scene::Scene& scene, EditorState& state);

        /**
         * @brief Updates the clean baseline without clearing history.
         * @note Does not write a Scene file or clear the pending mutation.
         */
        void MarkSaved(const Scene::Scene& scene, EditorState& state);

        /**
         * @brief Captures a pre-mutation snapshot in Edit mode when none is pending.
         * @note Repeated calls retain the original pending snapshot.
         */
        void BeginMutation(const Scene::Scene& scene, const EditorState& state);

        /**
         * @brief Compares final state and consumes the pending mutation boundary.
         * @return True when a differing snapshot is added to undo history.
         * @note Selection participates in history equality but not dirty equality.
         * @note Does not independently check Edit mode or roll back Scene changes.
         */
        bool EndMutation(const Scene::Scene& scene, EditorState& state);

        /**
         * @brief Restores the newest undo snapshot in Edit mode.
         * @return False outside Edit mode or when undo history is empty.
         * @note Restoration reconstructs Actors; resolve them again by UUID.
         * @note Exceptions propagate without atomic restoration guarantees.
         */
        bool Undo(Scene::Scene& scene, EditorState& state);

        /**
         * @brief Restores the newest redo snapshot in Edit mode.
         * @return False outside Edit mode or when redo history is empty.
         * @note Restoration reconstructs Actors; resolve them again by UUID.
         * @note Exceptions propagate without atomic restoration guarantees.
         */
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