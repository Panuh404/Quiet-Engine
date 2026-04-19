#pragma once

#include <memory>

#include "AppSpec.h"
#include "glm/vec3.hpp"
#include "ProjectContext.h"

#pragma region Forward declarations

namespace Quiet::Platform
{
    class Window;
}

namespace Quiet::Editor
{
    class EditorController;
    struct EditorState;
}

namespace Quiet::Input
{
    class Input;
}

namespace Quiet::Scene
{
    struct Actor;
    enum class RuntimeMode;
    class FreeCamera;
    class Scene;
}

namespace Quiet::Renderer::Vulkan
{
    struct CameraUniformData;
    struct ImGuiStats;
    class ImGuiLayer;
    class VulkanRenderer;
}

namespace Quiet::Math
{
    struct Ray;
}

#pragma endregion

namespace Quiet::Core
{
    class Application
    {
    public:
        explicit Application(AppSpec spec);
        ~Application();

        int Run();

    private:
        void ToggleMouseCapture();
        void SetMouseCaptureEnabled(bool enabled);

        void NewScene();
        void SaveScene();
        void LoadScene();

        void ProcessSandboxViewportResize();
        void ProcessEditorViewportResize(const Editor::EditorState& editorState);
        void ProcessEditorRequests(Editor::EditorState& editorState);

        void ResetEditorRuntimeState(Editor::EditorState& editorState);
        void CreateRuntimeScene(Scene::RuntimeMode runtimeMode);
        void DestroyRuntimeScene();

        void EnterSimulateMode();
        void StopSimulateMode();

        void EnterPlayMode();
        void StopPlayMode();

        [[nodiscard]] Renderer::Vulkan::CameraUniformData BuildActiveRenderCameraData(const Scene::Scene& scene) const;
        [[nodiscard]] glm::vec3 GetActiveRenderCameraPosition(const Scene::Scene& scene) const;

        bool UpdatePilotedEditorCamera(float deltaTime);

        [[nodiscard]] Scene::Scene& GetActiveScene();
        [[nodiscard]] const Scene::Scene& GetActiveScene() const;
        [[nodiscard]] const Scene::Actor* GetPreferredRenderCameraActor(const Scene::Scene& scene) const;

        bool RunEditorFrame(float deltaTime, Renderer::Vulkan::ImGuiLayer& uiLayer);
        bool RunSandboxFrame(float deltaTime, Renderer::Vulkan::ImGuiLayer& uiLayer);
        void FinalizePlatformWindows(bool frameCompleted) const;
        [[nodiscard]] std::function<void()> BuildEditorViewportOverlayCallback();
        [[nodiscard]] Renderer::Vulkan::ImGuiStats BuildEditorStats(float deltaTime, const Scene::Scene& scene) const;

    private:
        AppSpec m_Spec;
        bool m_Running = true;
        bool m_MouseCaptureEnabled = true;
        bool m_MouseCaptureBeforePlay = false;

        ProjectContext m_ProjectContext;

        std::unique_ptr<Platform::Window> m_Window;
        std::unique_ptr<Input::Input> m_Input;
        std::unique_ptr<Scene::FreeCamera> m_FreeCamera;
        std::unique_ptr<Scene::Scene> m_Scene;
        std::unique_ptr<Scene::Scene> m_RuntimeScene;
        std::unique_ptr<Renderer::Vulkan::VulkanRenderer> m_Renderer;
        std::unique_ptr<Editor::EditorController> m_EditorController;
    };
}