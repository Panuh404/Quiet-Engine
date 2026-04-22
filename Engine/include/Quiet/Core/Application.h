#pragma once

#include <functional>
#include <memory>

#include "AppSpec.h"
#include "glm/vec3.hpp"
#include "Quiet/Project/ProjectContext.h"
#include "Quiet/Project/ProjectRegistry.h"

#pragma region Forward declarations

namespace Quiet::Platform
{
    class Window;
}

namespace Quiet::Editor
{
    class EditorController;
    class EditorSceneWorkflow;
    class EditorRuntimeController;
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

namespace Quiet::Project
{
    class ProjectContext;
    class ProjectWorkflow;
}

namespace Quiet::Renderer::UI
{
    struct ImGuiStats;
    class ImGuiLayer;
}

namespace Quiet::Renderer::Vulkan
{
    struct CameraUniformData;
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

        void InitializeCommonStartup();
        void InitializeEditorStartup();
        void InitializeSandboxStartup();
        void ShutdownCommon();

        void ProcessGlobalShortcuts();
        void ProcessSandboxViewportResize();
        void ProcessEditorViewportResize(const Editor::EditorState& editorState);
        void ProcessEditorRequests(Editor::EditorState& editorState);

        [[nodiscard]] Renderer::Vulkan::CameraUniformData BuildActiveRenderCameraData(const Scene::Scene& scene) const;
        [[nodiscard]] glm::vec3 GetActiveRenderCameraPosition(const Scene::Scene& scene) const;

        void UpdateEditorSceneForFrame(Scene::Scene& activeScene,
                               Editor::EditorState& editorState,
                               float deltaTime);
        void BuildAndProcessEditorUI(Renderer::UI::ImGuiLayer& uiLayer,
                                     Scene::Scene& activeScene,
                                     Editor::EditorState& editorState,
                                     float deltaTime);

        void UpdateSandboxSceneForFrame(Scene::Scene& activeScene, float deltaTime);

        void BuildSandboxUI(Renderer::UI::ImGuiLayer& uiLayer);
        bool UpdatePilotedEditorCamera(float deltaTime);

        [[nodiscard]] Scene::Scene& GetActiveScene();
        [[nodiscard]] const Scene::Scene& GetActiveScene() const;
        [[nodiscard]] const Scene::Actor* GetPreferredRenderCameraActor(const Scene::Scene& scene) const;
        [[nodiscard]] float CalculateDeltaTime(double& previousTime) const;

        struct FrameContext
        {
            float DeltaTime = 0.0f;
            Renderer::UI::ImGuiLayer* UILayer = nullptr;
        };

        [[nodiscard]] FrameContext BeginFrame(double& previousTime);
        bool RunEditorFrame(const FrameContext& frame);
        bool RunSandboxFrame(const FrameContext& frame);
        void FinalizePlatformWindows(bool frameCompleted) const;
        [[nodiscard]] std::function<void()> BuildEditorViewportOverlayCallback();
        [[nodiscard]] Renderer::UI::ImGuiStats BuildEditorStats(float deltaTime, const Scene::Scene& scene) const;

    private:
        AppSpec m_Spec;
        bool m_Running = true;
        bool m_MouseCaptureEnabled = true;

        Project::ProjectContext m_ProjectContext;
        Project::ProjectRegistry m_ProjectRegistry{};
        std::unique_ptr<Project::ProjectWorkflow> m_ProjectWorkflow;

        std::unique_ptr<Editor::EditorController> m_EditorController;
        std::unique_ptr<Editor::EditorSceneWorkflow> m_EditorSceneWorkflow;
        std::unique_ptr<Editor::EditorRuntimeController> m_EditorRuntimeController;

        std::unique_ptr<Platform::Window> m_Window;
        std::unique_ptr<Input::Input> m_Input;
        std::unique_ptr<Scene::FreeCamera> m_FreeCamera;
        std::unique_ptr<Scene::Scene> m_Scene;
        std::unique_ptr<Scene::Scene> m_RuntimeScene;
        std::unique_ptr<Renderer::Vulkan::VulkanRenderer> m_Renderer;
    };
}