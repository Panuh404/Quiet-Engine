#pragma once

#include <cstdint>
#include <memory>
#include <string>

namespace Quiet::Renderer::Vulkan {
    class VulkanInstance;
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
        bool m_Running = true;
    };
}