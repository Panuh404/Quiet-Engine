#include "Quiet/Core/Log.h"
#include "Quiet/Core/Application.h"
#include "Quiet/Platform/Window.h"
#include "Scene/Camera/FreeCamera.h"

#include <stdexcept>
#include <utility>
#include <filesystem>
#include <algorithm>
#include <cctype>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <system_error>

#include "imgui.h"
#include "GLFW/glfw3.h"
#include "Quiet/Renderer/RenderConventions.h"
#include "Quiet/Input/Input.h"
#include "Quiet/Editor/EditorController.h"
#include "Quiet/Editor/EditorRuntimeController.h"
#include "Quiet/Editor/EditorSceneWorkflow.h"
#include "Quiet/Project/ProjectAssetLoader.h"
#include "Quiet/Project/ProjectWorkflow.h"
#include "Renderer/UI/ImGuiLayer.h"
#include "Renderer/Vulkan/Mesh/MeshLibrary.h"
#include "Renderer/Vulkan/VulkanRenderer.h"
#include "Scene/Scene.h"
#include "Scene/Camera/CameraController.h"
#include "Scene/Camera/CameraPilotUtils.h"

namespace
{
    const char* ToString(const Quiet::Core::AppMode mode)
    {
        switch (mode)
        {
            case Quiet::Core::AppMode::Sandbox: return "Sandbox";
            case Quiet::Core::AppMode::Editor:  return "Editor";
            default:                            return "Unknown";
        }
    }

    Quiet::Renderer::Vulkan::CameraUniformData BuildSceneCameraUniformData(const Quiet::Scene::Actor& cameraActor,
                                                                           float aspect)
    {
        using namespace Quiet;

        const auto& transform = cameraActor.Transform.WorldTransform;
        const auto& camera = cameraActor.Camera.value();

        const float safeAspect = (aspect > 0.0f) ? aspect : 1.0f;
        const float safeFov = std::clamp(camera.FieldOfViewDegrees, 1.0f, 179.0f);
        const float safeNear = std::max(camera.NearClip, 0.001f);
        const float safeFar = std::max(camera.FarClip, safeNear + 0.001f);

        glm::mat4 rotation(1.0f);
        rotation = glm::rotate(rotation, glm::radians(transform.RotationDegrees.z), glm::vec3(0.0f, 0.0f, 1.0f));
        rotation = glm::rotate(rotation, glm::radians(transform.RotationDegrees.y), glm::vec3(0.0f, 1.0f, 0.0f));
        rotation = glm::rotate(rotation, glm::radians(transform.RotationDegrees.x), glm::vec3(1.0f, 0.0f, 0.0f));

        const glm::vec3 position = transform.Position;
        const glm::vec3 forward = glm::normalize(glm::vec3(rotation * glm::vec4(0.0f, 0.0f, -1.0f, 0.0f)));
        const glm::vec3 up = glm::normalize(glm::vec3(rotation * glm::vec4(0.0f, 1.0f, 0.0f, 0.0f)));

        Renderer::Vulkan::CameraUniformData data{};
        data.View = glm::lookAt(position, position + forward, up);
        data.Projection = glm::perspective(glm::radians(safeFov), safeAspect, safeNear, safeFar);

        if (Renderer::UseRenderProjectionYFlip)
        {
            data.Projection[1][1] *= -1.0f;
        }

        return data;
    }
}

namespace Quiet::Core
{
    Application::Application(AppSpec spec) : m_Spec(std::move(spec))
    {
        Log::Init();
        QUIET_CORE_INFO("Configured application mode: {}", ToString(m_Spec.Mode));

        InitializeCommonStartup();

        if (m_Spec.IsEditor())
        {
            InitializeEditorStartup();
        }
        else
        {
            InitializeSandboxStartup();
        }
    }

    Application::~Application()
    {
        ShutdownCommon();
        Log::Shutdown();
    }

    int Application::Run()
    {
        QUIET_CORE_INFO("Starting application: {} [{}]", m_Spec.Name, ToString(m_Spec.Mode));
        double previousTime = glfwGetTime();

        while (m_Running && !m_Window->ShouldClose())
        {
            const FrameContext frame = BeginFrame(previousTime);
            const bool frameCompleted = m_Spec.IsEditor()
                ? RunEditorFrame(frame)
                : RunSandboxFrame(frame);

            FinalizePlatformWindows(frameCompleted);
        }

        QUIET_CORE_INFO("Shutting down.");
        return 0;
    }

    void Application::ToggleMouseCapture()
    {
        SetMouseCaptureEnabled(!m_MouseCaptureEnabled);
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

    void Application::InitializeCommonStartup()
    {
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
        SetMouseCaptureEnabled(m_Spec.IsSandbox());

        m_FreeCamera = std::make_unique<Scene::FreeCamera>();
        m_Scene = std::make_unique<Scene::Scene>();
        m_Renderer = std::make_unique<Renderer::Vulkan::VulkanRenderer>(m_Spec, *m_Window);

        m_ProjectAssetLoader = std::make_unique<Project::ProjectAssetLoader>(
            m_ProjectContext,
            m_Renderer->GetMeshLibrary(),
            m_Renderer->GetPhysicalDevice(),
            m_Renderer->GetLogicalDevice(),
            m_Renderer->GetGraphicsCommands()
        );

        m_ProjectWorkflow = std::make_unique<Project::ProjectWorkflow>(
            m_ProjectContext,
            m_ProjectRegistry,
            *m_ProjectAssetLoader,
            *m_Scene,
            m_Renderer->GetMeshLibrary()
        );

        if (!m_ProjectWorkflow->InitializeLastOpenedOrDefaultProject())
        {
            throw std::runtime_error("Failed to initialize project workflow.");
        }
    }

    void Application::InitializeEditorStartup()
    {
        m_EditorController = std::make_unique<Editor::EditorController>();
        m_EditorSceneWorkflow = std::make_unique<Editor::EditorSceneWorkflow>(
            *m_Scene,
            m_ProjectContext,
            m_Renderer->GetMeshLibrary(),
            *m_EditorController
        );
        m_EditorRuntimeController = std::make_unique<Editor::EditorRuntimeController>(
            *m_EditorController,
            *m_Scene,
            m_RuntimeScene,
            [this](const bool enabled) { SetMouseCaptureEnabled(enabled); },
            [this]() { return m_MouseCaptureEnabled; }
        );

        auto& editorState = m_EditorController->GetState();
        editorState.Scene.Mode = Editor::SceneMode::Edit;
        m_EditorSceneWorkflow->ResetSceneSession();
    }

    void Application::InitializeSandboxStartup()
    {
        const VkExtent2D initialRuntimeExtent = m_Renderer->GetSwapchainExtent();
        if (initialRuntimeExtent.width > 0 && initialRuntimeExtent.height > 0)
        {
            m_Renderer->RequestViewportResize(
                initialRuntimeExtent.width,
                initialRuntimeExtent.height
            );
        }

        m_Scene->BeginRuntime(Scene::RuntimeMode::Play);
        QUIET_CORE_INFO("Sandbox bootstrap entered play runtime.");
    }

    void Application::ShutdownCommon()
    {
        if (m_EditorRuntimeController)
            m_EditorRuntimeController->Shutdown();

        if (m_Scene && m_Scene->IsRuntimeActive())
            m_Scene->EndRuntime();

        m_EditorRuntimeController.reset();
        m_EditorSceneWorkflow.reset();
        m_ProjectWorkflow.reset();
        m_ProjectAssetLoader.reset();

        m_EditorController.reset();
        m_RuntimeScene.reset();
        m_Scene.reset();
        m_FreeCamera.reset();
        m_Renderer.reset();
        m_Input.reset();
        m_Window.reset();
    }

    void Application::ProcessGlobalShortcuts()
    {
        const bool shiftDown =
            m_Input->IsKeyDown(Input::KeyCode::LeftShift) ||
            m_Input->IsKeyDown(Input::KeyCode::RightShift);

        if (shiftDown && m_Input->IsKeyPressed(Input::KeyCode::F1))
        {
            ToggleMouseCapture();
        }
    }

    void Application::ProcessSandboxViewportResize()
    {
        const VkExtent2D swapchainExtent = m_Renderer->GetSwapchainExtent();
        const VkExtent2D currentViewportExtent = m_Renderer->GetSceneViewportExtent();

        if (swapchainExtent.width == 0 || swapchainExtent.height == 0) return;
        if (currentViewportExtent.width == swapchainExtent.width &&
            currentViewportExtent.height == swapchainExtent.height)
            return;

        m_Renderer->RequestViewportResize(
            swapchainExtent.width,
            swapchainExtent.height
        );
        m_Renderer->ApplyPendingViewportResize();
    }

    void Application::ProcessEditorViewportResize(const Editor::EditorState &editorState)
    {
        if (editorState.Viewport.Width == 0 || editorState.Viewport.Height == 0)
            return;

        const VkExtent2D currentViewportExtent = m_Renderer->GetSceneViewportExtent();

        if (currentViewportExtent.width == editorState.Viewport.Width &&
            currentViewportExtent.height == editorState.Viewport.Height)
            return;

        m_Renderer->RequestViewportResize(
            editorState.Viewport.Width,
            editorState.Viewport.Height
        );
    }

    void Application::ProcessEditorRequests(Editor::EditorState &editorState)
    {
        while (const std::optional<Editor::EditorRequest> request = editorState.PopNextRequest())
        {
            switch (request->Type)
            {
                case Editor::EditorRequestType::FocusSelected:
                    m_EditorController->FocusSelectedActor(GetActiveScene(), *m_FreeCamera);
                    break;
                case Editor::EditorRequestType::NewScene:
                    m_EditorSceneWorkflow->NewScene();
                    break;
                case Editor::EditorRequestType::SaveScene:
                    m_EditorSceneWorkflow->SaveScene();
                    break;
                case Editor::EditorRequestType::LoadScene:
                    m_EditorSceneWorkflow->LoadScene();
                    break;
                case Editor::EditorRequestType::EnterSimulate:
                    m_EditorRuntimeController->EnterSimulateMode();
                    break;
                case Editor::EditorRequestType::StopSimulate:
                    m_EditorRuntimeController->StopSimulateMode();
                    break;
                case Editor::EditorRequestType::EnterPlay:
                    m_EditorRuntimeController->EnterPlayMode();
                    break;
                case Editor::EditorRequestType::StopPlay:
                    m_EditorRuntimeController->StopPlayMode();
                    break;
                case Editor::EditorRequestType::Undo:
                    m_EditorController->Undo(*m_Scene);
                    break;
                case Editor::EditorRequestType::Redo:
                    m_EditorController->Redo(*m_Scene);
                    break;
                case Editor::EditorRequestType::OpenProject:
                    if (!request->Argument.empty() &&
                        m_EditorSceneWorkflow->EnsureEditMode("open project") &&
                        m_ProjectWorkflow->OpenOrCreateProject(request->Argument))
                    {
                        m_EditorSceneWorkflow->ResetSceneSession();
                    }
                    break;
                case Editor::EditorRequestType::CreateProject:
                    if (!request->Argument.empty() &&
                        m_EditorSceneWorkflow->EnsureEditMode("create project") &&
                        m_ProjectWorkflow->CreateProject(request->Argument))
                    {
                        m_EditorSceneWorkflow->ResetSceneSession();
                    }
                    break;
                case Editor::EditorRequestType::OpenProjectScene:
                    if (!request->Argument.empty())
                    {
                        m_EditorSceneWorkflow->OpenSceneInProject(request->Argument);
                    }
                    break;
                case Editor::EditorRequestType::SetStartupScene:
                    if (!request->Argument.empty())
                    {
                        m_ProjectWorkflow->SetStartupScene(request->Argument);
                    }
                    break;
                case Editor::EditorRequestType::ReloadAssets:
                    if (!m_ProjectAssetLoader)
                    {
                        QUIET_CORE_WARN("Cannot reload assets because the project asset loader is not available.");
                        break;
                    }

                    m_Renderer->WaitIdle();
                    const bool allAssetsLoaded = m_ProjectAssetLoader->ReloadMeshAssets(true);
                    if (!allAssetsLoaded)
                    {
                        QUIET_CORE_WARN("Some registered mesh assets could not be loaded.");
                    }

                    RelinkSceneStaticMeshes(*m_Scene);
                    if (m_RuntimeScene)
                    {
                        RelinkSceneStaticMeshes(*m_RuntimeScene);
                    }
                    break;
            }
        }
    }

    Scene::Scene& Application::GetActiveScene()
    {
        if (!m_Spec.IsEditor() || !m_EditorRuntimeController) return *m_Scene;
        return m_EditorRuntimeController->GetActiveScene();
    }

    const Scene::Scene& Application::GetActiveScene() const
    {
        if (!m_Spec.IsEditor() || !m_EditorRuntimeController) return *m_Scene;
        return m_EditorRuntimeController->GetActiveScene();
    }

    const Scene::Actor * Application::GetPreferredRenderCameraActor(const Scene::Scene &scene) const
    {
        const Scene::Actor* cameraActor = scene.FindPrimaryCameraActor();
        if (!cameraActor || !cameraActor->Camera.has_value()) return nullptr;

        if (!m_Spec.IsEditor()) return cameraActor;
        if (!m_EditorController) return nullptr;

        const auto& editorState = m_EditorController->GetState();
        return (editorState.Scene.Mode == Editor::SceneMode::Play) ? cameraActor : nullptr;
    }

    float Application::CalculateDeltaTime(double &previousTime) const
    {
        const double currentTime = glfwGetTime();
        const float deltaTime = static_cast<float>(currentTime - previousTime);
        previousTime = currentTime;
        return deltaTime;
    }

    Application::FrameContext Application::BeginFrame(double &previousTime)
    {
        m_Window->PollEvents();
        m_Input->Update();

        ProcessGlobalShortcuts();

        const float deltaTime = CalculateDeltaTime(previousTime);

        const bool deferViewportResize =
            m_Spec.IsEditor() &&
            m_Input->IsMouseButtonDown(Input::MouseCode::ButtonLeft);

        if (!deferViewportResize)
            m_Renderer->ApplyPendingViewportResize();

        auto& uiLayer = m_Renderer->GetImGuiLayer();

        FrameContext frame{};
        frame.DeltaTime = deltaTime;
        frame.UILayer = &uiLayer;
        return frame;
    }

    bool Application::RunEditorFrame(const FrameContext& frame)
    {
        if (!m_EditorController)
        {
            QUIET_CORE_ERROR("Editor frame requested without an editor controller.");
            return false;
        }

        if (!frame.UILayer)
        {
            QUIET_CORE_ERROR("Editor frame requested without a valid UI layer.");
            return false;
        }

        auto& uiLayer = *frame.UILayer;
        const float deltaTime = frame.DeltaTime;
        auto& editorState = m_EditorController->GetState();
        Scene::Scene& activeScene = GetActiveScene();

        UpdateEditorSceneForFrame(activeScene, editorState, deltaTime);

        uiLayer.BeginFrame();
        {
            BuildAndProcessEditorUI(uiLayer, activeScene, editorState, deltaTime);
        }
        uiLayer.EndFrame();

        Scene::Scene& renderScene = GetActiveScene();
        const auto activeCameraData = BuildActiveRenderCameraData(renderScene);
        return m_Renderer->DrawFrame(renderScene, activeCameraData, uiLayer);
    }

    bool Application::RunSandboxFrame(const FrameContext& frame)
    {
        if (!frame.UILayer)
        {
            QUIET_CORE_ERROR("Sandbox frame requested without a valid UI layer.");
            return false;
        }

        auto& uiLayer = *frame.UILayer;
        const float deltaTime = frame.DeltaTime;
        Scene::Scene& activeScene = *m_Scene;

        UpdateSandboxSceneForFrame(activeScene, deltaTime);

        uiLayer.BeginFrame();
        {
            BuildSandboxUI(uiLayer);
        }
        uiLayer.EndFrame();

        const auto activeCameraData = BuildActiveRenderCameraData(activeScene);
        return m_Renderer->DrawFrame(activeScene, activeCameraData, uiLayer);
    }

    void Application::FinalizePlatformWindows(const bool frameCompleted) const
    {
        if (frameCompleted && (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable))
        {
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
        }
    }

    std::function<void()> Application::BuildEditorViewportOverlayCallback()
    {
        return [this]()
        {
            auto& overlayScene = GetActiveScene();

            m_EditorController->TrySelectActorFromViewport(
                *m_Input,
                overlayScene,
                *m_FreeCamera,
                *m_Renderer,
                m_MouseCaptureEnabled
            );

            m_EditorController->HandleShortcuts(*m_Input, m_MouseCaptureEnabled);
            m_EditorController->BuildSelectedActorGizmo(
                overlayScene,
                *m_FreeCamera,
                *m_Renderer,
                m_MouseCaptureEnabled
            );
        };
    }

    Renderer::UI::ImGuiStats Application::BuildEditorStats(float deltaTime, const Scene::Scene &scene) const
    {
        Renderer::UI::ImGuiStats stats{};
        stats.FrameTimeMs = deltaTime * 1000.0f;
        stats.FPS = (deltaTime > 0.0f) ? (1.0f / deltaTime) : 0.0f;
        stats.ActorCount = static_cast<std::uint32_t>(scene.GetActors().size());
        stats.MeshCount = static_cast<std::uint32_t>(m_Renderer->GetMeshLibrary().GetCount());
        stats.SwapchainExtent = m_Renderer->GetSwapchainExtent();
        stats.MouseCaptured = m_MouseCaptureEnabled;
        stats.CameraPosition = GetActiveRenderCameraPosition(scene);
        return stats;
    }

    Renderer::Vulkan::CameraUniformData Application::BuildActiveRenderCameraData(const Scene::Scene &scene) const
    {
        const VkExtent2D sceneExtent = m_Renderer->GetSceneViewportExtent();

        const float safeWidth = (sceneExtent.width > 0) ? static_cast<float>(sceneExtent.width) : 1.0f;
        const float safeHeight = (sceneExtent.height > 0) ? static_cast<float>(sceneExtent.height) : 1.0f;
        const float aspect = safeWidth / safeHeight;

        if (const Scene::Actor* cameraActor = GetPreferredRenderCameraActor(scene))
        {
            return BuildSceneCameraUniformData(*cameraActor, aspect);
        }

        return m_FreeCamera->BuildUniformData(aspect, Renderer::UseRenderProjectionYFlip);
    }

    glm::vec3 Application::GetActiveRenderCameraPosition(const Scene::Scene &scene) const
    {
        if (const Scene::Actor* cameraActor = GetPreferredRenderCameraActor(scene))
        {
            return cameraActor->Transform.WorldTransform.Position;
        }

        return m_FreeCamera->GetPosition();
    }

    void Application::UpdateEditorSceneForFrame(Scene::Scene &activeScene,
                                                Editor::EditorState &editorState,
                                                float deltaTime)
    {
        const bool isEditMode = (editorState.Scene.Mode == Editor::SceneMode::Edit);
        const bool isSimulateMode = (editorState.Scene.Mode == Editor::SceneMode::Simulate);
        const bool isPlayMode = (editorState.Scene.Mode == Editor::SceneMode::Play);

        const bool isPilotingEditCamera = isEditMode && UpdatePilotedEditorCamera(deltaTime);
        const bool isUsingEditorFlyControls =
            (isEditMode || isSimulateMode) &&
            m_Input->IsMouseButtonDown(Input::MouseCode::ButtonRight);

        if (!isPilotingEditCamera && isUsingEditorFlyControls)
        {
            m_FreeCamera->Update(*m_Input, deltaTime);
        }

        if (isSimulateMode || isPlayMode)
        {
            activeScene.HandleRuntimeInput(*m_Input, deltaTime);
        }
        activeScene.Update(deltaTime);
    }

    void Application::BuildAndProcessEditorUI(Renderer::UI::ImGuiLayer &uiLayer,
                                              Scene::Scene &activeScene,
                                              Editor::EditorState &editorState,
                                              float deltaTime)
    {
        const Renderer::UI::ImGuiStats stats = BuildEditorStats(deltaTime, activeScene);
        uiLayer.BuildEditorUI(
            stats,
            *m_Input,
            activeScene,
            *m_EditorController,
            editorState,
            m_ProjectContext,
            m_ProjectRegistry,
            m_Renderer->GetMeshLibrary(),
            m_Renderer->GetSceneViewportTextureID(),
            BuildEditorViewportOverlayCallback());

        ProcessEditorRequests(editorState);
        ProcessEditorViewportResize(editorState);
    }

    void Application::UpdateSandboxSceneForFrame(Scene::Scene &activeScene, float deltaTime)
    {
        if (activeScene.IsRuntimeActive())
            activeScene.HandleRuntimeInput(*m_Input, deltaTime);

        activeScene.Update(deltaTime);
        ProcessSandboxViewportResize();
    }

    void Application::BuildSandboxUI(Renderer::UI::ImGuiLayer &uiLayer)
    {
        std::uint32_t runtimeViewportWidth = 0;
        std::uint32_t runtimeViewportHeight = 0;

        uiLayer.BuildRuntimeUI(
            runtimeViewportWidth,
            runtimeViewportHeight,
            m_Renderer->GetSceneViewportTextureID());
    }

    bool Application::UpdatePilotedEditorCamera(float deltaTime)
    {
        if (!m_Spec.IsEditor() || !m_EditorController) return false;

        auto& editorState = m_EditorController->GetState();

        if (editorState.Scene.Mode != Editor::SceneMode::Edit ||
            !editorState.Selection.PilotedCameraActorID.has_value())
        {
            return false;
        }

        Scene::Actor* actor = m_Scene->FindActorByID(editorState.Selection.PilotedCameraActorID.value());
        if (!actor || !actor->Camera.has_value())
        {
            editorState.Selection.PilotedCameraActorID.reset();
            return false;
        }

        Scene::CameraPilotState state = Scene::BuildPilotStateFromCameraTransform(actor->Transform.WorldTransform);

        state.MoveSpeed = m_FreeCamera->GetMoveSpeed();
        state.LookSensitivity = m_FreeCamera->GetLookSensitivity();

        const bool isUsingEditorFlyControls = m_Input->IsMouseButtonDown(Input::MouseCode::ButtonRight);
        if (isUsingEditorFlyControls)
        {
            Scene::CameraController::UpdateFromInput(state, *m_Input, deltaTime);
            Scene::ApplyPilotStateToCameraTransform(state, actor->Transform.WorldTransform);
        }

        m_FreeCamera->SetPose(state.Position, state.YawDegrees, state.PitchDegrees);
        return true;
    }

    void Application::RelinkSceneStaticMeshes(Scene::Scene &scene)
    {
        auto& meshLibrary = m_Renderer->GetMeshLibrary();

        for (auto& actor : scene.GetActors())
        {
            if (!actor.StaticMesh.has_value()) continue;
            const std::string& meshID = actor.StaticMesh->MeshResourceID;
            if (auto mesh = meshLibrary.TryGet(meshID))
            {
                actor.StaticMesh->Mesh = mesh;
            }
            else
            {
                QUIET_CORE_WARN(
                    "Could not relink actor '{}' to missing mesh resource '{}'.",
                    actor.Name,
                    meshID
                );
            }
        }
    }
}
