#pragma once

#include <vulkan/Vulkan.h>

namespace Quiet::Platform
{
    class Window;
}

namespace Quiet::Renderer::Vulkan
{
    class VulkanSurface
    {
    public:
        VulkanSurface(VkInstance instance, const Quiet::Platform::Window& window);
        ~VulkanSurface();

        VulkanSurface(const VulkanSurface&) = delete;
        VulkanSurface& operator=(const VulkanSurface&) = delete;
        VulkanSurface(VulkanSurface&&) = delete;
        VulkanSurface& operator=(VulkanSurface&&) = delete;

        [[nodiscard]] VkSurfaceKHR GetHandle() const { return m_Surface; }

    private:
        VkInstance m_Instance = VK_NULL_HANDLE;
        VkSurfaceKHR m_Surface = VK_NULL_HANDLE;
    };
}

