#pragma once

#include <cstdint>
#include <memory>
#include <string>

#include "EditorState.h"

#pragma region Forward declarations

namespace Quiet::Renderer::Vulkan
{
    class DepthResources;
    class Swapchain;
    class VulkanInstance;
    class VulkanSurface;
    class PhysicalDevice;
    class LogicalDevice;
    class GraphicsCommands;
    class FrameSync;
    class StaticMeshPipeline;
    class StaticMeshRenderSystem;
    class CameraResources;
    class StaticMesh;
    class MeshLibrary;
    class SceneViewportTarget;

    class ImGuiLayer;
    struct ImGuiStats;
}

namespace Quiet::Platform
{
    class Window;
}

namespace Quiet::Input
{
    class Input;
}

namespace Quiet::Scene
{
    class FreeCamera;
    class Scene;
}

#pragma endregion

namespace Quiet::Core
{
    struct AppSpec
    {
        std::string Name = "Quiet Application";
        std::uint32_t Width = 1280;
        std::uint32_t Height = 720;
    };

    class Application
    {
    public:
        explicit Application(AppSpec spec);
        ~Application();

        int Run();

    private:
        bool DrawFrame();
        void RecreateSwapchain();

        void SetMouseCaptureEnabled(bool enabled);
        void ToggleMouseCapture();

    private:
        bool m_Running = true;
        bool m_MouseCaptureEnabled = true;
        AppSpec m_Spec;
        EditorState m_EditorState{};

        std::uint32_t m_PendingViewportWidth = 0;
        std::uint32_t m_PendingViewportHeight = 0;
        bool m_ViewportResizePending = false;
        bool m_SceneViewportReady = false;

        void RecordFrameCommandBuffer(std::uint32_t imageIndex);

        std::unique_ptr<Quiet::Platform::Window> m_Window;
        std::unique_ptr<Quiet::Input::Input> m_Input;
        std::unique_ptr<Quiet::Scene::Scene> m_Scene;
        std::unique_ptr<Quiet::Scene::FreeCamera> m_FreeCamera;

        std::unique_ptr<Quiet::Renderer::Vulkan::MeshLibrary> m_MeshLibrary;
        std::unique_ptr<Quiet::Renderer::Vulkan::StaticMeshRenderSystem> m_StaticMeshRenderSystem;
        std::unique_ptr<Quiet::Renderer::Vulkan::StaticMeshPipeline> m_StaticMeshPipeline;
        std::unique_ptr<Quiet::Renderer::Vulkan::CameraResources> m_CameraResources;
        std::unique_ptr<Quiet::Renderer::Vulkan::SceneViewportTarget> m_SceneViewportTarget;

        std::unique_ptr<Quiet::Renderer::Vulkan::ImGuiLayer> m_ImGuiLayer;

        std::unique_ptr<Quiet::Renderer::Vulkan::VulkanInstance> m_VulkanInstance;
        std::unique_ptr<Quiet::Renderer::Vulkan::VulkanSurface> m_VulkanSurface;
        std::unique_ptr<Quiet::Renderer::Vulkan::PhysicalDevice> m_PhysicalDevice;
        std::unique_ptr<Quiet::Renderer::Vulkan::LogicalDevice> m_LogicalDevice;
        std::unique_ptr<Quiet::Renderer::Vulkan::Swapchain> m_Swapchain;
        std::unique_ptr<Quiet::Renderer::Vulkan::DepthResources> m_DepthResources;
        std::unique_ptr<Quiet::Renderer::Vulkan::GraphicsCommands> m_GraphicsCommands;
        std::unique_ptr<Quiet::Renderer::Vulkan::FrameSync> m_FrameSync;
    };
}