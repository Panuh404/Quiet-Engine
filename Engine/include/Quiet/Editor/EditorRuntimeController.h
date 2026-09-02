#pragma once
#include <functional>
#include <memory>

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
                                MouseCaptureSetter setMouseCaptureEnabled,
                                MouseCaptureGetter getMouseCaptureEnabled);

        void Shutdown();

        void EnterSimulateMode();
        void StopSimulateMode();

        void EnterPlayMode();
        void StopPlayMode();

        [[nodiscard]] Scene::Scene& GetActiveScene();
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

        MouseCaptureSetter m_SetMouseCaptureEnabled;
        MouseCaptureGetter m_GetMouseCaptureEnabled;

        bool m_MouseCaptureBeforePlay = false;
    };
}