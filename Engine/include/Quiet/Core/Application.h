#pragma once

#include <cstdint>
#include <memory>
#include <string>

namespace Quiet::Renderer::Vulkan {
    class DepthResources;
    class Swapchain;
    class VulkanInstance;
    class VulkanSurface;
    class PhysicalDevice;
    class LogicalDevice;
    class GraphicsCommands;
    class FrameSync;

    class TrianglePipeline;
    class CameraResources;
    class StaticMesh;
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
        void DrawFrame();
        void RecreateSwapchain();

    private:
        bool m_Running = true;
        AppSpec m_Spec;

        void RecordTriangleCommandBuffer(std::uint32_t imageIndex);

        std::unique_ptr<Quiet::Input::Input> m_Input;
        std::unique_ptr<Quiet::Scene::Scene> m_Scene;

        std::shared_ptr<Quiet::Renderer::Vulkan::StaticMesh> m_StaticMesh;
        std::unique_ptr<Quiet::Renderer::Vulkan::TrianglePipeline> m_TrianglePipeline;
        std::unique_ptr<Quiet::Renderer::Vulkan::CameraResources> m_CameraResources;

        std::unique_ptr<Quiet::Scene::FreeCamera> m_FreeCamera;

        std::unique_ptr<Quiet::Platform::Window> m_Window;
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