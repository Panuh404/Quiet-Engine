#pragma once

#include <memory>
#include <optional>

#include "EditorState.h"

namespace Quiet::Input
{
    class Input;
}

namespace Quiet::Scene
{
    class Actor;
    class Scene;
    class FreeCamera;
}

namespace Quiet::Math
{
    struct Ray;
}

namespace Quiet::Renderer::Vulkan
{
    class VulkanRenderer;
}

namespace Quiet::Editor
{
    class EditorController
    {
    public:
        EditorController() = default;
        ~EditorController() = default;

        [[nodiscard]] EditorState& GetState() { return m_State; }
        [[nodiscard]] const EditorState& GetState() const { return m_State; }

        void ResetSelection();

        void ResetSceneHistory(const Scene::Scene& scene);
        void MarkSceneSaved(const Scene::Scene& scene);
        void BeginSceneMutation(const Scene::Scene& scene);
        bool EndSceneMutation(const Scene::Scene& scene);
        bool Undo(Scene::Scene& scene);
        bool Redo(Scene::Scene& scene);

        void HandleShortcuts(bool mouseCaptureEnabled);
        void FocusSelectedActor(Scene::Scene& scene,
                                Scene::FreeCamera& camera);

        void TrySelectActorFromViewport(const Input::Input& input,
                                        Scene::Scene& scene,
                                        const Scene::FreeCamera& camera,
                                        const Renderer::Vulkan::VulkanRenderer& renderer,
                                        bool mouseCaptureEnabled);

        void BuildSelectedActorGizmo(Scene::Scene& scene,
                                     Scene::FreeCamera& camera,
                                     const Renderer::Vulkan::VulkanRenderer& renderer,
                                     bool mouseCaptureEnabled);

    private:
        struct SceneSnapshot
        {
            std::vector<Scene::Actor> Actors;
            std::optional<Core::UUID> SelectedActorID;
        };

        [[nodiscard]] bool BuildViewportPickRay(const Input::Input& input,
                                                const Scene::FreeCamera& camera,
                                                const Renderer::Vulkan::VulkanRenderer& renderer,
                                                Quiet::Math::Ray& outRay) const;

        [[nodiscard]] SceneSnapshot CaptureSnapshot(const Scene::Scene& scene) const;
        void RestoreSnapshot(Scene::Scene& scene, const SceneSnapshot& snapshot);
        static bool AreSnapshotsEqual(const SceneSnapshot& lhs, const SceneSnapshot& rhs);
        void PushUndoSnapshot(SceneSnapshot snapshot);
        void PushRedoSnapshot(SceneSnapshot snapshot);

        void RefreshHistoryFlags();
        void RefreshDirtyState(const Scene::Scene& scene);

    private:
        static constexpr std::size_t MaxHistoryEntries = 128;

        EditorState m_State{};
        std::vector<SceneSnapshot> m_UndoStack;
        std::vector<SceneSnapshot> m_RedoStack;
        std::optional<SceneSnapshot> m_PendingSnapshot;
        std::optional<SceneSnapshot> m_CleanSnapshot;
        bool m_WasUsingGizmo = false;
    };
}