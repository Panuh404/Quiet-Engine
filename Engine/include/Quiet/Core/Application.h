#pragma once

#include <cstdint>
#include <memory>
#include <string>

namespace Quiet::Renderer::Vulkan {
    class VulkanInstance;
    class VulkanSurface;
    class PhysicalDevice;
    class LogicalDevice;
}

namespace Quiet::Platform
{
    class Window;
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
        AppSpec m_Spec;
        std::unique_ptr<Quiet::Platform::Window> m_Window;
        std::unique_ptr<Quiet::Renderer::Vulkan::VulkanInstance> m_VulkanInstance;
        std::unique_ptr<Quiet::Renderer::Vulkan::VulkanSurface> m_VulkanSurface;
        std::unique_ptr<Quiet::Renderer::Vulkan::PhysicalDevice> m_PhysicalDevice;
        std::unique_ptr<Quiet::Renderer::Vulkan::LogicalDevice> m_LogicalDevice;
        bool m_Running = true;
    };
}