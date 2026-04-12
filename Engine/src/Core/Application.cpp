#include "Quiet/Core/Log.h"
#include "Quiet/Core/Application.h"
#include "Quiet/Platform/Window.h"
#include "Scene/FreeCamera.h"

#include <stdexcept>
#include <utility>
#include <limits>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/ext/matrix_projection.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "imgui.h"
#include "ImGuizmo.h"
#include "GLFW/glfw3.h"
#include "Input/Input.h"
#include "Quiet/Math/Geometry.h"
#include "Quiet/Scene/SceneDocument.h"
#include "Renderer/Vulkan/StaticMesh.h"

#include "Renderer/Vulkan/VulkanRenderer.h"
#include "Scene/Scene.h"

namespace
{
    ImGuizmo::OPERATION ToImGuizmoOperation(Quiet::Core::GizmoOperation operation)
    {
        switch (operation)
        {
            case Quiet::Core::GizmoOperation::Translate: return ImGuizmo::TRANSLATE;
            case Quiet::Core::GizmoOperation::Rotate:    return ImGuizmo::ROTATE;
            case Quiet::Core::GizmoOperation::Scale:     return ImGuizmo::SCALE;
            default:                                     return ImGuizmo::TRANSLATE;
        }
    }

    ImGuizmo::MODE ToImGuizmoMode(Quiet::Core::GizmoMode mode)
    {
        switch (mode)
        {
            case Quiet::Core::GizmoMode::Local: return ImGuizmo::LOCAL;
            case Quiet::Core::GizmoMode::World: return ImGuizmo::WORLD;
            default:                            return ImGuizmo::WORLD;
        }
    }
}

namespace Quiet::Core
{
    Application::Application(AppSpec spec) : m_Spec(std::move(spec))
    {
        Log::Init();

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

        m_Renderer = std::make_unique<Renderer::Vulkan::VulkanRenderer>(
            m_Spec,
            *m_Window);

        NewScene();
    }

    Application::~Application()
    {
        m_EditorState.SelectedActorID.reset();
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

            m_Scene->Update(deltaTime);
            m_Renderer->ApplyPendingViewportResize();

            auto& uiLayer = m_Renderer->GetImGuiLayer();
            uiLayer.BeginFrame();
            {
                Renderer::Vulkan::ImGuiStats stats{};
                stats.FrameTimeMs = deltaTime * 1000.0f;
                stats.FPS = (deltaTime > 0.0f) ? (1.0f / deltaTime) : 0.0f;
                stats.ActorCount = static_cast<std::uint32_t>(m_Scene->GetActors().size());
                stats.MeshCount = 2;
                stats.SwapchainExtent = m_Renderer->GetSwapchainExtent();
                stats.MouseCaptured = m_MouseCaptureEnabled;
                stats.CameraPosition = m_FreeCamera->GetPosition();

                uiLayer.BuildEditorUI(
                    stats,
                    *m_Scene,
                    m_EditorState,
                    m_Renderer->GetSceneViewportTextureID(),
                    [this]()
                    {
                        TrySelectActorFromViewport();
                        HandleEditorShortcuts();

                        if (m_EditorState.RequestFocusSelected)
                        {
                            FocusSelectedActor();
                            m_EditorState.RequestFocusSelected = false;
                        }

                        BuildSelectedActorGizmo();
                    }
                );

                if (m_EditorState.RequestNewScene)
                {
                    NewScene();
                    m_EditorState.RequestNewScene = false;
                }

                if (m_EditorState.RequestSaveScene)
                {
                    SaveScene();
                    m_EditorState.RequestSaveScene = false;
                }

                if (m_EditorState.RequestLoadScene)
                {
                    LoadScene();
                    m_EditorState.RequestLoadScene = false;
                }

                if (m_EditorState.ViewportWidth > 0 && m_EditorState.ViewportHeight > 0)
                {
                    const VkExtent2D currentViewportExtent = m_Renderer->GetSceneViewportExtent();

                    if (currentViewportExtent.width != m_EditorState.ViewportWidth ||
                        currentViewportExtent.height != m_EditorState.ViewportHeight)
                    {
                        m_Renderer->RequestViewportResize(
                            m_EditorState.ViewportWidth,
                            m_EditorState.ViewportHeight
                        );
                    }
                }
            }
            uiLayer.EndFrame();

            const bool frameCompleted = m_Renderer->DrawFrame(
                *m_Scene,
                *m_FreeCamera,
                uiLayer,
                m_EditorState
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

    void Application::HandleEditorShortcuts()
    {
        if (m_MouseCaptureEnabled)
        {
            return;
        }

        const ImGuiIO& io = ImGui::GetIO();
        if (io.WantTextInput)
        {
            return;
        }

        if (ImGui::IsKeyPressed(ImGuiKey_T, false)) { m_EditorState.CurrentGizmoOperation = GizmoOperation::Translate; }
        if (ImGui::IsKeyPressed(ImGuiKey_R, false)) { m_EditorState.CurrentGizmoOperation = GizmoOperation::Rotate; }
        if (ImGui::IsKeyPressed(ImGuiKey_S, false)) { m_EditorState.CurrentGizmoOperation = GizmoOperation::Scale; }
        if (ImGui::IsKeyPressed(ImGuiKey_L, false))
        {
            if (m_EditorState.CurrentGizmoOperation != GizmoOperation::Scale)
            {
                m_EditorState.CurrentGizmoMode =
                    (m_EditorState.CurrentGizmoMode == GizmoMode::World)
                        ? GizmoMode::Local
                        : GizmoMode::World;
            }
        }
        if (ImGui::IsKeyPressed(ImGuiKey_G, false)) { m_EditorState.GizmoSnapEnabled = !m_EditorState.GizmoSnapEnabled; }
        if (ImGui::IsKeyPressed(ImGuiKey_F, false)) { m_EditorState.RequestFocusSelected = true; }
    }

    void Application::FocusSelectedActor()
    {
        if (!m_EditorState.SelectedActorID.has_value()) return;

        Scene::Actor* actor = m_Scene->FindActorByID(m_EditorState.SelectedActorID.value());
        if (!actor)
        {
            m_EditorState.SelectedActorID.reset();
            return;
        }

        glm::vec3 target = actor->Transform.WorldTransform.Position;
        float radius = 1.0f;

        if (actor->StaticMesh.has_value() &&
            actor->StaticMesh->IsValid() &&
            actor->StaticMesh->Visible)
        {
            const auto& mesh = actor->StaticMesh->Mesh;
            const auto localBounds = mesh->GetLocalBounds();

            const Quiet::Math::AABB worldBounds = Quiet::Math::TransformAABB(
                actor->Transform.WorldTransform.ToMatrix(),
                localBounds.Min,
                localBounds.Max
            );

            target = (worldBounds.Min + worldBounds.Max) * 0.5f;
            radius = glm::max(0.5f, glm::length(worldBounds.Max - worldBounds.Min) * 0.5f);
        }

        const float fovRadians = glm::radians(m_FreeCamera->GetFovDegrees());
        float distance = radius / std::tan(fovRadians * 0.5f);
        distance = glm::max(distance * 1.5f, 1.5f);

        const glm::vec3 forward = m_FreeCamera->GetForwardVector();
        const glm::vec3 newPosition = target - forward * distance;

        m_FreeCamera->SetPosition(newPosition);

        QUIET_CORE_TRACE(
            "Focused selected actor at ({:.2f}, {:.2f}, {:.2f}).",
            target.x,
            target.y,
            target.z
        );
    }

    void Application::BuildSelectedActorGizmo()
    {
        if (m_MouseCaptureEnabled) return;
        if (!m_Renderer->IsSceneViewportReady()) return;
        if (!m_EditorState.SelectedActorID.has_value()) return;
        if (m_EditorState.ViewportWidth == 0 || m_EditorState.ViewportHeight == 0) return;
        if (!m_EditorState.ViewportImageHovered && !ImGuizmo::IsUsing()) return;

        const VkExtent2D sceneExtent = m_Renderer->GetSceneViewportExtent();
        const float aspect =
            static_cast<float>(sceneExtent.width) /
            static_cast<float>(sceneExtent.height);

        const auto cameraData = m_FreeCamera->BuildUniformData(aspect, false);

        Scene::Actor* actor = m_Scene->FindActorByID(m_EditorState.SelectedActorID.value());
        if (!actor)
        {
            m_EditorState.SelectedActorID.reset();
            return;
        }

        auto& transform = actor->Transform.WorldTransform;

        float translation[3] =
        {
            transform.Position.x,
            transform.Position.y,
            transform.Position.z
        };
        float rotation[3] =
        {
            transform.RotationDegrees.x,
            transform.RotationDegrees.y,
            transform.RotationDegrees.z
        };
        float scale[3] =
        {
            transform.Scale.x,
            transform.Scale.y,
            transform.Scale.z
        };

        float modelArray[16]{};
        ImGuizmo::RecomposeMatrixFromComponents(
            translation,
            rotation,
            scale,
            modelArray
        );

        glm::mat4 model = glm::make_mat4(modelArray);
        ImGuizmo::SetOrthographic(false);
        ImGuizmo::SetDrawlist();
        ImGuizmo::SetRect(
            m_EditorState.ViewportScreenX,
            m_EditorState.ViewportScreenY,
            static_cast<float>(m_EditorState.ViewportWidth),
            static_cast<float>(m_EditorState.ViewportHeight)
        );

        const ImGuizmo::OPERATION operation = ToImGuizmoOperation(m_EditorState.CurrentGizmoOperation);
        const ImGuizmo::MODE mode = ToImGuizmoMode(m_EditorState.CurrentGizmoMode);

        float snapValues[3] = { 0.0f, 0.0f, 0.0f };
        const float* snapPointer = nullptr;

        if (m_EditorState.GizmoSnapEnabled)
        {
            switch (m_EditorState.CurrentGizmoOperation)
            {
            case GizmoOperation::Translate:
                snapValues[0] = m_EditorState.TranslateSnap[0];
                snapValues[1] = m_EditorState.TranslateSnap[1];
                snapValues[2] = m_EditorState.TranslateSnap[2];
                snapPointer = snapValues;
                break;

            case GizmoOperation::Rotate:
                snapValues[0] = m_EditorState.RotateSnap;
                snapPointer = snapValues;
                break;

            case GizmoOperation::Scale:
                snapValues[0] = m_EditorState.ScaleSnap;
                snapPointer = snapValues;
                break;
            }
        }

        const bool changed = ImGuizmo::Manipulate(
            glm::value_ptr(cameraData.View),
            glm::value_ptr(cameraData.Projection),
            operation,
            mode,
            glm::value_ptr(model),
            nullptr,
            snapPointer
        );

        if (changed || ImGuizmo::IsUsing())
        {
            float outTranslation[3]{};
            float outRotation[3]{};
            float outScale[3]{};

            ImGuizmo::DecomposeMatrixToComponents(
                glm::value_ptr(model),
                outTranslation,
                outRotation,
                outScale
            );

            transform.Position = { outTranslation[0], outTranslation[1], outTranslation[2] };
            transform.RotationDegrees = { outRotation[0], outRotation[1], outRotation[2] };
            transform.Scale = { outScale[0], outScale[1], outScale[2] };
        }
    }

    void Application::NewScene()
    {
        m_EditorState.SelectedActorID.reset();
        if (m_Scene) m_Scene->Clear();

        Scene::SceneDocument::CreateDefault(
            *m_Scene,
            m_Renderer->GetMeshLibrary()
        );

        QUIET_CORE_INFO("Created new scene.");
    }

    void Application::SaveScene()
    {
        const std::filesystem::path scenePath = "Assets/Scenes/DefaultScene.json";

        if (!Scene::SceneDocument::Save(*m_Scene, scenePath))
        {
            QUIET_CORE_ERROR("Failed to save scene to '{}'.", scenePath.string());
        }
    }

    void Application::LoadScene()
    {
        const std::filesystem::path scenePath = "Assets/Scenes/DefaultScene.json";
        m_EditorState.SelectedActorID.reset();
        if (m_Scene) m_Scene->Clear();

        if (!Scene::SceneDocument::Load(*m_Scene, m_Renderer->GetMeshLibrary(), scenePath))
        {
            QUIET_CORE_WARN("Failed to load scene from '{}'.", scenePath.string());
            return;
        }
        QUIET_CORE_INFO("Loaded scene from '{}'.", scenePath.string());
    }

    void Application::TrySelectActorFromViewport()
    {
        if (m_MouseCaptureEnabled) return;
        if (!m_EditorState.ViewportImageHovered) return;
        if (!m_Input->IsMouseButtonPressed(Quiet::Input::MouseCode::ButtonLeft)) return;
        if (ImGuizmo::IsOver() || ImGuizmo::IsUsing()) return;

        Math::Ray ray{};
        if (!BuildViewportPickRay(ray)) return;

        auto& actors = m_Scene->GetActors();
        float closestHit = std::numeric_limits<float>::max();
        std::optional<UUID> bestHitID;

        for (std::size_t i = 0; i < actors.size(); ++i)
        {
            auto& actor = actors[i];

            if (!actor.StaticMesh.has_value() ||
                !actor.StaticMesh->IsValid() ||
                !actor.StaticMesh->Visible)
                continue;

            const auto& mesh = actor.StaticMesh->Mesh;
            const glm::mat4 model = actor.Transform.WorldTransform.ToMatrix();

            const Math::AABB worldBounds = Math::TransformAABB(
                model,
                mesh->GetLocalBoundsMin(),
                mesh->GetLocalBoundsMax()
            );

            float hitDistance = 0.0f;
            if (Math::IntersectRayAABB(ray, worldBounds, hitDistance))
            {
                if (hitDistance < closestHit)
                {
                    closestHit = hitDistance;
                    bestHitID = actor.ID;
                }
            }
        }

        if (bestHitID.has_value())
        {
            m_EditorState.SelectedActorID = bestHitID;
            QUIET_CORE_TRACE("Viewport pick selected actor.");
        }
        else
        {
            m_EditorState.SelectedActorID.reset();
        }
    }

    bool Application::BuildViewportPickRay(Math::Ray& outRay) const
    {
        const VkExtent2D sceneExtent = m_Renderer->GetSceneViewportExtent();
        if (sceneExtent.width == 0 || sceneExtent.height == 0)
            return false;

        if (m_EditorState.ViewportWidth == 0 || m_EditorState.ViewportHeight == 0)
            return false;

        const auto [mouseX, mouseY] = m_Input->GetMousePosition();

        const float localX = static_cast<float>(mouseX) - m_EditorState.ViewportScreenX;
        const float localY = static_cast<float>(mouseY) - m_EditorState.ViewportScreenY;

        if (localX < 0.0f || localY < 0.0f ||
            localX > static_cast<float>(m_EditorState.ViewportWidth) ||
            localY > static_cast<float>(m_EditorState.ViewportHeight))
            return false;

        const float viewportHeight = static_cast<float>(m_EditorState.ViewportHeight);
        const float flippedY = viewportHeight - localY;

        const float aspect =
            static_cast<float>(sceneExtent.width) /
            static_cast<float>(sceneExtent.height);

        const auto cameraData = m_FreeCamera->BuildUniformData(aspect, false);

        const glm::vec4 viewport(
            0.0f,
            0.0f,
            static_cast<float>(m_EditorState.ViewportWidth),
            static_cast<float>(m_EditorState.ViewportHeight)
        );

        const glm::vec3 nearPoint = glm::unProjectZO(
            glm::vec3(localX, flippedY, 0.0f),
            cameraData.View,
            cameraData.Projection,
            viewport
        );

        const glm::vec3 farPoint = glm::unProjectZO(
            glm::vec3(localX, flippedY, 1.0f),
            cameraData.View,
            cameraData.Projection,
            viewport
        );

        outRay.Origin = nearPoint;
        outRay.Direction = glm::normalize(farPoint - nearPoint);
        return true;
    }
}
