#include "Quiet/Editor/EditorRuntimeController.h"

#include "Audio/AudioSubsystem.h"
#include "Quiet/Core/Log.h"
#include "Quiet/Editor/EditorController.h"
#include "Scene/Scene.h"
#include "Physics/PhysicsSubsystem.h"


namespace Quiet::Editor
{
    EditorRuntimeController::EditorRuntimeController(EditorController &editorController,
                                                     Scene::Scene &editorScene,
                                                     std::unique_ptr<Scene::Scene> &runtimeScene,
                                                     Physics::PhysicsSubsystem& physicsSubsystem,
                                                     Audio::AudioSubsystem& audioSubsystem,
                                                     const Project::ProjectContext& projectContext,
                                                     const Input::Input& input,
                                                     MouseCaptureSetter setMouseCaptureEnabled,
                                                     MouseCaptureGetter getMouseCaptureEnabled)
                                                        : m_EditorController(editorController)
                                                        , m_EditorScene(editorScene)
                                                        , m_RuntimeScene(runtimeScene)
                                                        , m_PhysicsSubsystem(physicsSubsystem)
                                                        , m_AudioSubsystem(audioSubsystem)
                                                        , m_ProjectContext(projectContext)
                                                        , m_Input(input)
                                                        , m_SetMouseCaptureEnabled(std::move(setMouseCaptureEnabled))
                                                        , m_GetMouseCaptureEnabled(std::move(getMouseCaptureEnabled)) {}

    void EditorRuntimeController::Shutdown()
    {
        DestroyRuntimeScene();
    }

    void EditorRuntimeController::EnterSimulateMode()
    {
        auto& editorState = m_EditorController.GetState();
        if (editorState.Scene.Mode != SceneMode::Edit) return;

        ResetEditorRuntimeState(editorState);
        CreateRuntimeScene(Scene::RuntimeMode::Simulate);

        editorState.Scene.Mode = SceneMode::Simulate;
        QUIET_CORE_INFO("Entered simulate mode.");
    }

    void EditorRuntimeController::StopSimulateMode()
    {
        auto& editorState = m_EditorController.GetState();
        if (editorState.Scene.Mode != SceneMode::Simulate) return;

        DestroyRuntimeScene();
        editorState.Scene.Mode = SceneMode::Edit;

        QUIET_CORE_INFO("Stopped simulate mode.");
    }

    void EditorRuntimeController::EnterPlayMode()
    {
        auto& editorState = m_EditorController.GetState();
        if (editorState.Scene.Mode != SceneMode::Edit) return;

        ResetEditorRuntimeState(editorState);
        CreateRuntimeScene(Scene::RuntimeMode::Play);

        editorState.Scene.Mode = SceneMode::Play;
        m_MouseCaptureBeforePlay = m_GetMouseCaptureEnabled();
        m_SetMouseCaptureEnabled(true);

        QUIET_CORE_INFO("Entered play mode.");
    }

    void EditorRuntimeController::StopPlayMode()
    {
        auto& editorState = m_EditorController.GetState();
        if (editorState.Scene.Mode != SceneMode::Play) return;

        DestroyRuntimeScene();
        editorState.Scene.Mode = SceneMode::Edit;
        m_SetMouseCaptureEnabled(m_MouseCaptureBeforePlay);

        QUIET_CORE_INFO("Stopped play mode.");
    }

    Scene::Scene & EditorRuntimeController::GetActiveScene()
    {
        const auto& editorState = m_EditorController.GetState();
        if (editorState.Scene.Mode != SceneMode::Edit && m_RuntimeScene)
            return *m_RuntimeScene;

        return m_EditorScene;
    }

    const Scene::Scene & EditorRuntimeController::GetActiveScene() const
    {
        const auto& editorState = m_EditorController.GetState();
        if (editorState.Scene.Mode != SceneMode::Edit && m_RuntimeScene)
            return *m_RuntimeScene;

        return m_EditorScene;
    }

    void EditorRuntimeController::ResetEditorRuntimeState(EditorState &editorState)
    {
        editorState.Selection.PilotedCameraActorID.reset();
    }

    void EditorRuntimeController::CreateRuntimeScene(Scene::RuntimeMode runtimeMode)
    {
        DestroyRuntimeScene();
        m_AudioSubsystem.StopPreview();

        m_RuntimeScene = std::make_unique<Scene::Scene>(m_EditorScene);
        m_RuntimeScene->BeginRuntime(runtimeMode, m_PhysicsSubsystem, m_AudioSubsystem, m_ProjectContext, m_Input);
    }

    void EditorRuntimeController::DestroyRuntimeScene()
    {
        if (!m_RuntimeScene) return;
        if (m_RuntimeScene->IsRuntimeActive())
            m_RuntimeScene->EndRuntime();

        m_RuntimeScene.reset();
    }
}
