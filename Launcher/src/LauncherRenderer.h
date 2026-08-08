#pragma once

#include <cstdint>
#include <memory>

namespace Quiet::Platform
{
    class Window;
}

namespace Quiet::Renderer::UI
{
    class ImGuiLayer;
}

namespace Quiet::Renderer::Vulkan
{
    class FrameSync;
    class GraphicsCommands;
    class LogicalDevice;
    class PhysicalDevice;
    class Swapchain;
    class VulkanInstance;
    class VulkanSurface;
}

namespace Quiet::Launcher
{
    class LauncherRenderer
    {
    public:
        explicit LauncherRenderer(Platform::Window& window);
        ~LauncherRenderer();

        LauncherRenderer(const LauncherRenderer&) = delete;
        LauncherRenderer& operator=(const LauncherRenderer&) = delete;

        [[nodiscard]] Renderer::UI::ImGuiLayer& GetImGuiLayer();

        bool DrawFrame();
        void WaitIdle() const;

    private:
        void CreateResources();
        void DestroyResources();
        void RecreateSwapchain();

        void RecordCommandBuffer(std::uint32_t imageIndex);

    private:
        Platform::Window* m_Window = nullptr;

        std::unique_ptr<Renderer::Vulkan::VulkanInstance> m_VulkanInstance;
        std::unique_ptr<Renderer::Vulkan::VulkanSurface> m_VulkanSurface;
        std::unique_ptr<Renderer::Vulkan::PhysicalDevice> m_PhysicalDevice;
        std::unique_ptr<Renderer::Vulkan::LogicalDevice> m_LogicalDevice;
        std::unique_ptr<Renderer::Vulkan::Swapchain> m_Swapchain;
        std::unique_ptr<Renderer::Vulkan::GraphicsCommands> m_GraphicsCommands;
        std::unique_ptr<Renderer::Vulkan::FrameSync> m_FrameSync;
        std::unique_ptr<Renderer::UI::ImGuiLayer> m_ImGuiLayer;
    };
}