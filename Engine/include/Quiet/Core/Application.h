#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <filesystem>
#include <string>

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
    enum class ViewportNavigationMode : std::uint8_t;
}

namespace Quiet::Input
{
    class Input;
}

namespace Quiet::Audio
{
    class AudioSubsystem;
}

namespace Quiet::Physics
{
    class PhysicsSubsystem;
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
    class ProjectAssetLoader;
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
    /**
     * @brief Owns and coordinates the Editor or Sandbox application systems.
     * @ingroup quiet_core
     *
     * Construction initializes shared systems and mode-specific startup.
     * Editor project bootstrapping continues during application frames.
     */
    class Application
    {
    public:
        /**
         * @brief Initializes shared systems and selects mode-specific startup.
         * @param spec Initial configuration stored by the Application.
         * @note Successful construction does not imply Editor project readiness.
         * @note Startup exceptions propagate to the caller.
         */
        explicit Application(AppSpec spec);

        /**
         * @brief Shuts down application resources and then logging.
         */
        ~Application();

        /**
         * @brief Executes application frames until the loop terminates.
         * @return Zero after ordinary loop termination.
         * @note Execution exceptions propagate to the caller.
         */
        int Run();

    private:
        void ToggleMouseCapture();
        void SetMouseCaptureEnabled(bool enabled);

        void BeginEditorViewportNavigationCapture(Scene::Scene& activeScene, Editor::EditorState& editorState, Editor::ViewportNavigationMode mode);
        void EndEditorViewportNavigationCapture(Editor::EditorState& editorState);
        void UpdateEditorViewportNavigationCapture(Scene::Scene& activeScene, Editor::EditorState& editorState);

        void InitializeCommonStartup();
        void InitializeEditorStartup();
        void InitializeSandboxStartup();
        void ShutdownCommon();

        void ProcessGlobalShortcuts();
        void ProcessSandboxViewportResize();
        void ProcessEditorViewportResize(const Editor::EditorState& editorState);
        void ProcessEditorViewportAssetDrop(Scene::Scene& activeScene, Editor::EditorState& editorState);
        void ProcessEditorRequests(Editor::EditorState& editorState);
        void RefreshProjectAssetLoaderRendererBindings();

        [[nodiscard]] Renderer::Vulkan::CameraUniformData BuildActiveRenderCameraData(const Scene::Scene& scene) const;
        [[nodiscard]] glm::vec3 GetActiveRenderCameraPosition(const Scene::Scene& scene) const;

        void UpdateEditorSceneForFrame(Scene::Scene& activeScene, Editor::EditorState& editorState, float deltaTime);
        void BuildAndProcessEditorUI(Renderer::UI::ImGuiLayer& uiLayer, Scene::Scene& activeScene, Editor::EditorState& editorState, float deltaTime);
        void UpdateSandboxSceneForFrame(Scene::Scene& activeScene, float deltaTime);

        void BuildSandboxUI(Renderer::UI::ImGuiLayer& uiLayer);
        bool UpdatePilotedEditorCamera(float deltaTime);
        void RelinkSceneStaticMeshes(Scene::Scene& scene);

        [[nodiscard]] Scene::Scene& GetActiveScene();
        [[nodiscard]] const Scene::Scene& GetActiveScene() const;
        [[nodiscard]] const Scene::Actor* GetPreferredRenderCameraActor(const Scene::Scene& scene) const;
        [[nodiscard]] float CalculateDeltaTime(double& previousTime) const;

        enum class EditorBootstrapStage : std::uint8_t
        {
            WaitingForFirstFrame,
            PreparingProject,
            LoadingAssets,
            LoadingScene,
            Ready,
            Failed
        };

        struct FrameContext
        {
            float DeltaTime = 0.0f;
            Renderer::UI::ImGuiLayer* UILayer = nullptr;
        };

        [[nodiscard]] FrameContext BeginFrame(double& previousTime);
        bool RunEditorFrame(const FrameContext& frame);
        bool RunEditorBootstrapFrame(const FrameContext& frame);
        void AdvanceEditorBootstrap();
        void BuildEditorBootstrapUI(Renderer::UI::ImGuiLayer& uiLayer);

        bool RunSandboxFrame(const FrameContext& frame);
        void FinalizePlatformWindows(bool frameCompleted) const;
        [[nodiscard]] std::function<void()> BuildEditorViewportOverlayCallback();
        [[nodiscard]] Renderer::UI::ImGuiStats BuildEditorStats(float deltaTime, const Scene::Scene& scene) const;

    private:
        AppSpec m_Spec;
        bool m_Running = true;
        bool m_MouseCaptureEnabled = true;

        EditorBootstrapStage m_EditorBootstrapStage = EditorBootstrapStage::Ready;
        std::filesystem::path m_EditorProjectFilePath;
        std::string m_EditorBootstrapError;

        Editor::ViewportNavigationMode m_ViewportNavigationMode{};
        glm::vec3 m_ViewportNavigationPivot{0.0f};

        bool m_MouseCaptureBeforeViewportNavigation = false;
        double m_ViewportNavigationCursorX = 0.0;
        double m_ViewportNavigationCursorY = 0.0;

        Project::ProjectContext m_ProjectContext;
        Project::ProjectRegistry m_ProjectRegistry{};
        std::unique_ptr<Project::ProjectWorkflow> m_ProjectWorkflow;
        std::unique_ptr<Project::ProjectAssetLoader> m_ProjectAssetLoader;

        std::unique_ptr<Editor::EditorController> m_EditorController;
        std::unique_ptr<Editor::EditorSceneWorkflow> m_EditorSceneWorkflow;
        std::unique_ptr<Editor::EditorRuntimeController> m_EditorRuntimeController;

        std::unique_ptr<Platform::Window> m_Window;
        std::unique_ptr<Input::Input> m_Input;
        std::unique_ptr<Physics::PhysicsSubsystem> m_PhysicsSubsystem;
        std::unique_ptr<Audio::AudioSubsystem> m_AudioSubsystem;
        std::unique_ptr<Scene::FreeCamera> m_FreeCamera;
        std::unique_ptr<Scene::Scene> m_Scene;
        std::unique_ptr<Scene::Scene> m_RuntimeScene;
        std::unique_ptr<Renderer::Vulkan::VulkanRenderer> m_Renderer;
    };
}