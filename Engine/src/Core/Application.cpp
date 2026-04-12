#include "Quiet/Core/Log.h"
#include "Quiet/Core/Application.h"
#include "Quiet/Platform/Window.h"
#include "Scene/FreeCamera.h"

#include <stdexcept>
#include <utility>
#include <filesystem>

#include "imgui.h"
#include "GLFW/glfw3.h"
#include "Input/Input.h"
#include "Quiet/Editor/EditorController.h"
#include "Quiet/Scene/SceneDocument.h"
#include "Renderer/Vulkan/MeshLibrary.h"
#include "Renderer/Vulkan/VulkanRenderer.h"
#include "Scene/Scene.h"


namespace Quiet::Core
{
    Application::Application(AppSpec spec) : m_Spec(std::move(spec))
    {
        Log::Init();
        m_ProjectContext.SetRootPath(std::filesystem::current_path());

        Platform::WindowDesc windowDesc{};
        windowDesc.Width = m_Spec.Width;
        windowDesc.Height = m_Spec.Height;
        windowDesc.Title = m_Spec.Name;
        windowDesc.StartMaximized = m_Spec.StartMaximized;

        m_Window = std::make_unique<Platform::Window>(windowDesc);
        if (!m_Window->IsVulkanSupported())
        {
            QUIET_CORE_ERROR("GLFW reports that Vulkan is not available.");
            throw std::runtime_error(
                "GLFW reports that Vulkan is not available. Check your Vulkan loader, drivers, or SDK."
            );
        }
        m_Input = std::make_unique<Input::Input>(*m_Window);
        SetMouseCaptureEnabled(false);

        m_FreeCamera = std::make_unique<Scene::FreeCamera>();
        m_Scene = std::make_unique<Scene::Scene>();
        m_Renderer = std::make_unique<Renderer::Vulkan::VulkanRenderer>(m_Spec, *m_Window);
        m_EditorController = std::make_unique<Editor::EditorController>();

        if (!Scene::SceneDocument::Load(*m_Scene, m_Renderer->GetMeshLibrary(), m_ProjectContext.GetDefaultScenePath()))
        {
            NewScene();
        }
        else
        {
            auto& editorState = m_EditorController->GetState();
            editorState.SceneDirty = false;
            editorState.SceneDisplayName = m_ProjectContext.GetDefaultScenePath().filename().string();

            m_EditorController->ResetSelection();
            QUIET_CORE_INFO("Loaded startup scene.");
        }
    }

    Application::~Application()
    {
        m_EditorController.reset();
        m_Scene.reset();
        m_FreeCamera.reset();
        m_Renderer.reset();
        m_Input.reset();
        m_Window.reset();
        Log::Shutdown();
    }

    int Application::Run()
    {
        QUIET_CORE_INFO("Starting application: {}", m_Spec.Name);
        double previousTime = glfwGetTime();

        while (m_Running && !m_Window->ShouldClose())
        {
            m_Window->PollEvents();
            m_Input->Update();

            const bool shiftDown =
                m_Input->IsKeyDown(Input::KeyCode::LeftShift) ||
                m_Input->IsKeyDown(Input::KeyCode::RightShift);

            if (shiftDown && m_Input->IsKeyPressed(Input::KeyCode::F1))
            {
                ToggleMouseCapture();
            }

            const double currentTime = glfwGetTime();
            const float deltaTime = static_cast<float>(currentTime - previousTime);
            previousTime = currentTime;

            if (m_MouseCaptureEnabled)
            {
                m_FreeCamera->Update(*m_Input, deltaTime);
            }

            m_Scene->Update(deltaTime, true);
            m_Renderer->ApplyPendingViewportResize();

            auto& editorState = m_EditorController->GetState();
            auto& uiLayer = m_Renderer->GetImGuiLayer();

            uiLayer.BeginFrame();
            {
                Renderer::Vulkan::ImGuiStats stats{};
                stats.FrameTimeMs = deltaTime * 1000.0f;
                stats.FPS = (deltaTime > 0.0f) ? (1.0f / deltaTime) : 0.0f;
                stats.ActorCount = static_cast<std::uint32_t>(m_Scene->GetActors().size());
                stats.MeshCount = static_cast<std::uint32_t>(m_Renderer->GetMeshLibrary().GetCount());
                stats.SwapchainExtent = m_Renderer->GetSwapchainExtent();
                stats.MouseCaptured = m_MouseCaptureEnabled;
                stats.CameraPosition = m_FreeCamera->GetPosition();

                uiLayer.BuildEditorUI(
                    stats,
                    *m_Scene,
                    editorState,
                    m_Renderer->GetMeshLibrary(),
                    m_Renderer->GetSceneViewportTextureID(),
                    [this]()
                    {
                        auto& editorStateInner = m_EditorController->GetState();
                        m_EditorController->TrySelectActorFromViewport(
                            *m_Input,
                            *m_Scene,
                            *m_FreeCamera,
                            *m_Renderer,
                            m_MouseCaptureEnabled
                        );

                        m_EditorController->HandleShortcuts(m_MouseCaptureEnabled);
                        if (editorStateInner.RequestFocusSelected)
                        {
                            m_EditorController->FocusSelectedActor(*m_Scene, *m_FreeCamera);
                            editorStateInner.RequestFocusSelected = false;
                        }

                        m_EditorController->BuildSelectedActorGizmo(
                            *m_Scene,
                            *m_FreeCamera,
                            *m_Renderer,
                            m_MouseCaptureEnabled
                        );
                    }
                );

                if (editorState.RequestNewScene)
                {
                    NewScene();
                    editorState.RequestNewScene = false;
                }

                if (editorState.RequestSaveScene)
                {
                    SaveScene();
                    editorState.RequestSaveScene = false;
                }

                if (editorState.RequestLoadScene)
                {
                    LoadScene();
                    editorState.RequestLoadScene = false;
                }

                if (editorState.ViewportWidth > 0 && editorState.ViewportHeight > 0)
                {
                    const VkExtent2D currentViewportExtent = m_Renderer->GetSceneViewportExtent();

                    if (currentViewportExtent.width != editorState.ViewportWidth ||
                        currentViewportExtent.height != editorState.ViewportHeight)
                    {
                        m_Renderer->RequestViewportResize(
                            editorState.ViewportWidth,
                            editorState.ViewportHeight
                        );
                    }
                }
            }
            uiLayer.EndFrame();

            const bool frameCompleted = m_Renderer->DrawFrame(
                *m_Scene,
                *m_FreeCamera,
                uiLayer,
                editorState
            );

            if (frameCompleted &&
                (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable))
            {
                ImGui::UpdatePlatformWindows();
                ImGui::RenderPlatformWindowsDefault();
            }
        }

        QUIET_CORE_INFO("Shutting down.");
        return 0;
    }

    void Application::SetMouseCaptureEnabled(bool enabled)
    {
        m_MouseCaptureEnabled = enabled;

        m_Input->SetCursorDisabled(enabled);
        m_Input->EnableRawMouseMotionIfSupported(enabled);
        m_Input->ResetMouseDelta();

        QUIET_CORE_TRACE(
            "Mouse capture: {}. Toggle with Shift+F1.",
            enabled ? "enabled" : "disabled"
        );
    }

    void Application::ToggleMouseCapture()
    {
        SetMouseCaptureEnabled(!m_MouseCaptureEnabled);
    }

    void Application::NewScene()
    {
        Scene::SceneDocument::CreateDefault(
            *m_Scene,
            m_Renderer->GetMeshLibrary()
        );
        auto& editorState = m_EditorController->GetState();
        editorState.SceneDirty = false;
        editorState.SceneDisplayName = "DefaultScene.json";

        m_EditorController->ResetSelection();
        QUIET_CORE_INFO("Created new scene.");
    }

    void Application::SaveScene()
    {
        if (!m_ProjectContext.EnsureDirectoriesExist())
        {
            QUIET_CORE_ERROR("Failed to create project scene directories.");
            return;
        }

        const std::filesystem::path scenePath = m_ProjectContext.GetDefaultScenePath();
        if (!Scene::SceneDocument::Save(*m_Scene, scenePath))
        {
            QUIET_CORE_ERROR("Failed to save scene to '{}'.", scenePath.string());
            return;
        }

        auto& editorState = m_EditorController->GetState();
        editorState.SceneDirty = false;
        editorState.SceneDisplayName = scenePath.filename().string();

        QUIET_CORE_INFO("Saved scene to '{}'.", scenePath.string());
    }

    void Application::LoadScene()
    {
        const std::filesystem::path scenePath = m_ProjectContext.GetDefaultScenePath();

        if (!Scene::SceneDocument::Load(*m_Scene, m_Renderer->GetMeshLibrary(), scenePath))
        {
            QUIET_CORE_WARN("Failed to load scene from '{}'.", scenePath.string());
            return;
        }

        auto& editorState = m_EditorController->GetState();
        editorState.SceneDirty = false;
        editorState.SceneDisplayName = scenePath.filename().string();

        m_EditorController->ResetSelection();
        QUIET_CORE_INFO("Loaded scene from '{}'.", scenePath.string());
    }
}
