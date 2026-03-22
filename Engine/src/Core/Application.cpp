#include "Quiet/Core/Application.h"
#include "Quiet/Platform/Window.h"

#include <vulkan/vulkan.h>

#include <iostream>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace
{
    std::string FormatVulkanVersion(std::uint32_t version)
    {
        std::ostringstream out;
        out << VK_API_VERSION_MAJOR(version) << '.'
            << VK_API_VERSION_MINOR(version) << '.'
            << VK_API_VERSION_PATCH(version);
        return out.str();
    }

    std::uint32_t QueryLoaderInstanceVersion()
    {
        std::uint32_t version = VK_API_VERSION_1_0;

        auto enumerateInstanceVersion =
            reinterpret_cast<PFN_vkEnumerateInstanceVersion>(
                vkGetInstanceProcAddr(VK_NULL_HANDLE, "vkEnumerateInstanceVersion")
            );

        if (enumerateInstanceVersion != nullptr)
        {
            if (enumerateInstanceVersion(&version) != VK_SUCCESS)
                throw std::runtime_error("Failed to query Vulkan instance version.");
        }
        return version;
    }
}

namespace Quiet::Core
{
    Application::Application(AppSpec spec)
        : m_Spec(std::move(spec))
    {
        Quiet::Platform::WindowDesc windowDesc{};
        windowDesc.Width = m_Spec.Width;
        windowDesc.Height = m_Spec.Height;
        windowDesc.Title = m_Spec.Name;

        m_Window = std::make_unique<Quiet::Platform::Window>(windowDesc);
    }

    Application::~Application() = default;

    int Application::Run()
    {
        std::cout << "[Quiet] Starting application: " << m_Spec.Name << '\n';

        if (!m_Window->IsVulkanSupported())
        {
            throw std::runtime_error(
                "GLFW reports that Vulkan is not available. Check your Vulkan loader, drivers, or SDK."
            );
        }

        const std::uint32_t apiVersion = QueryLoaderInstanceVersion();
        std::cout << "[Quiet] Vulkan loader instance API version: "
                  << FormatVulkanVersion(apiVersion) << '\n';

        const auto extensions = m_Window->GetRequiredVulkanInstanceExtensions();
        std::cout << "[Quiet] Required Vulkan instance extensions:\n";
        for (const char* extension : extensions)
        {
            std::cout << "  - " << extension << '\n';
        }

        while (m_Running && !m_Window->ShouldClose())
        {
            m_Window->PollEvents();
        }

        std::cout << "[Quiet] Shutting down.\n";
        return 0;
    }
}