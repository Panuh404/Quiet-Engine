#include "VulkanSurface.h"
#include "Quiet/Platform/Window.h"
#include "Quiet/Core/Log.h"

#include <GLFW/glfw3.h>

#include <stdexcept>

namespace Quiet::Renderer::Vulkan
{
    VulkanSurface::VulkanSurface(VkInstance instance, const Quiet::Platform::Window &window)
        : m_Instance(instance)
    {
        auto* glfwWindow = static_cast<GLFWwindow*>(window.GetNativeHandle());
        const VkResult result = glfwCreateWindowSurface(m_Instance, glfwWindow, nullptr, &m_Surface);

        if (result != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create window surface!");
        }

        QUIET_CORE_INFO("Created Vulkan window surface.");
    }

    VulkanSurface::~VulkanSurface()
    {
        if (m_Surface != VK_NULL_HANDLE)
        {
            vkDestroySurfaceKHR(m_Instance, m_Surface, nullptr);
            m_Surface = VK_NULL_HANDLE;
        }
    }
}
