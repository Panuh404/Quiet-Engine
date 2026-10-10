#pragma once
#include <functional>
#include <memory>

namespace Quiet::Input
{
    class Input;
}

namespace Quiet::Project
{
    class ProjectContext;
}

namespace Quiet::Audio
{
    class AudioSubsystem;
}

namespace Quiet::Scene
{
    enum class RuntimeMode;
    class Scene;
}

namespace Quiet::Physics
{
    class PhysicsSubsystem;
}

namespace Quiet::Editor
{
    class EditorController;
    struct EditorState;

    /**
     * @brief Coordinates Editor Play and Simulate runtime transitions.
     * @ingroup quiet_editor
     *
     * Borrows the editable Scene and runtime dependencies.
     * Creates and releases the runtime Scene through a reference to an
     * externally owned unique pointer.
     */
    class EditorRuntimeController
    {
    public:
        using MouseCaptureSetter = std::function<void(bool)>;
        using MouseCaptureGetter = std::function<bool()>;

        EditorRuntimeController(EditorController& editorController,
                                Scene::Scene& editorScene,
                                std::unique_ptr<Scene::Scene>& runtimeScene,
                                Physics::PhysicsSubsystem& physicsSubsystem,
                                Audio::AudioSubsystem& audioSubsystem,
                                const Project::ProjectContext& projectContext,
                                const Input::Input& input,
                                MouseCaptureSetter setMouseCaptureEnabled,
                                MouseCaptureGetter getMouseCaptureEnabled);

        /**
         * @brief Ends and releases the runtime Scene when present.
         * @note Does not reset Editor mode or restore Play mouse capture.
         */
        void Shutdown();

        /**
         * @brief Creates Simulate runtime when currently in Edit mode.
         * @note Exceptions propagate without transition rollback.
         */
        void EnterSimulateMode();

        /**
         * @brief Discards Simulate runtime and returns to Edit mode.
         * @note Does nothing outside Simulate mode.
         */
        void StopSimulateMode();

        /**
         * @brief Creates Play runtime from Edit mode and requests mouse capture.
         * @note Records capture state after runtime creation.
         * @note Exceptions propagate without transition rollback.
         */
        void EnterPlayMode();

        /**
         * @brief Discards Play runtime and restores the recorded capture state.
         * @note Does nothing outside Play mode.
         */
        void StopPlayMode();

        /**
         * @brief Returns runtime in non-Edit mode when present, otherwise editable.
         * @note The returned reference must not outlive its selected Scene.
         */
        [[nodiscard]] Scene::Scene& GetActiveScene();

        /**
         * @brief Returns runtime in non-Edit mode when present, otherwise editable.
         * @note The returned reference must not outlive its selected Scene.
         */
        [[nodiscard]] const Scene::Scene& GetActiveScene() const;

    private:
        void ResetEditorRuntimeState(EditorState& editorState);
        void CreateRuntimeScene(Scene::RuntimeMode runtimeMode);
        void DestroyRuntimeScene();

    private:
        EditorController& m_EditorController;
        Scene::Scene& m_EditorScene;
        std::unique_ptr<Scene::Scene>& m_RuntimeScene;

        Physics::PhysicsSubsystem& m_PhysicsSubsystem;
        Audio::AudioSubsystem& m_AudioSubsystem;
        const Project::ProjectContext& m_ProjectContext;
        const Input::Input& m_Input;

        MouseCaptureSetter m_SetMouseCaptureEnabled;
        MouseCaptureGetter m_GetMouseCaptureEnabled;

        bool m_MouseCaptureBeforePlay = false;
    };
}