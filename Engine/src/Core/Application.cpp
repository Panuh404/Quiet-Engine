#include "Quiet/Core/Log.h"
#include "Quiet/Core/Application.h"
#include "Quiet/Platform/Window.h"
#include "Renderer/Vulkan/VulkanInstance.h"
#include "Renderer/Vulkan/VulkanSurface.h"
#include "Renderer/Vulkan/PhysicalDevice.h"

#include <stdexcept>
#include <utility>

namespace Quiet::Core
{
    Application::Application(AppSpec spec)
        : m_Spec(std::move(spec))
    {
        Log::Init();

        Quiet::Platform::WindowDesc windowDesc{};
        windowDesc.Width = m_Spec.Width;
        windowDesc.Height = m_Spec.Height;
        windowDesc.Title = m_Spec.Name;

        m_Window = std::make_unique<Quiet::Platform::Window>(windowDesc);
        if (!m_Window->IsVulkanSupported())
        {
            QUIET_CORE_ERROR("GLFW reports that Vulkan is not available.");
            throw std::runtime_error(
                "GLFW reports that Vulkan is not available. Check your Vulkan loader, drivers, or SDK."
            );
        }

        m_VulkanInstance = std::make_unique<Quiet::Renderer::Vulkan::VulkanInstance>(
            m_Spec.Name,
            m_Window->GetRequiredVulkanInstanceExtensions()
        );

        m_VulkanSurface = std::make_unique<Quiet::Renderer::Vulkan::VulkanSurface>(
            m_VulkanInstance->GetHandle(),
            *m_Window
        );

        m_PhysicalDevice = std::make_unique<Quiet::Renderer::Vulkan::PhysicalDevice>(
            m_VulkanInstance->GetHandle(),
            m_VulkanSurface->GetHandle(),
            m_VulkanInstance->GetApiVersion()
        );
    }

    Application::~Application()
    {
        m_PhysicalDevice.reset();
        m_VulkanSurface.reset();
        m_VulkanInstance.reset();
        m_Window.reset();
        Log::Shutdown();
    }

    int Application::Run()
    {
        QUIET_CORE_INFO("Starting application: {}", m_Spec.Name);

        while (m_Running && !m_Window->ShouldClose())
        {
            m_Window->PollEvents();
        }

        QUIET_CORE_INFO("Shutting down.");
        return 0;
    }
}