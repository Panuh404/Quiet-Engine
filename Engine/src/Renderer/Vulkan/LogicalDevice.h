#pragma once

#include <vulkan/vulkan.h>

namespace Quiet::Renderer::Vulkan
{
    class PhysicalDevice;

    class LogicalDevice
    {
    public:
        explicit LogicalDevice(const PhysicalDevice& physicalDevice);
        ~LogicalDevice();

        LogicalDevice(const LogicalDevice&) = delete;
        LogicalDevice& operator=(const LogicalDevice&) = delete;
        LogicalDevice(LogicalDevice&&) = delete;
        LogicalDevice& operator=(LogicalDevice&&) = delete;

        VkDevice GetHandle() const { return m_Device; }
        VkQueue GetGraphicsQueue() const { return m_GraphicsQueue; }
        VkQueue GetPresentQueue() const { return m_PresentQueue; }

    private:
        VkDevice m_Device = VK_NULL_HANDLE;
        VkQueue m_GraphicsQueue = VK_NULL_HANDLE;
        VkQueue m_PresentQueue = VK_NULL_HANDLE;
    };
}