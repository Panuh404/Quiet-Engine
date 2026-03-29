#pragma once

#include <vector>

#include <vulkan/vulkan.h>

namespace Quiet::Renderer::Vulkan
{
    class PhysicalDevice;
    class LogicalDevice;
    class Swapchain;

    class GraphicsCommands
    {
    public:
        GraphicsCommands(const PhysicalDevice& physicalDevice,
                         const LogicalDevice& logicalDevice,
                         const Swapchain& swapchain);
        ~GraphicsCommands();

        GraphicsCommands(const GraphicsCommands&) = delete;
        GraphicsCommands& operator=(const GraphicsCommands&) = delete;
        GraphicsCommands(GraphicsCommands&&) = delete;
        GraphicsCommands& operator=(GraphicsCommands&&) = delete;

        [[nodiscard]] VkCommandPool GetCommandPool() const { return m_CommandPool; }
        [[nodiscard]] const std::vector<VkCommandBuffer>& GetCommandBuffers() const { return m_CommandBuffers; }

    private:
        VkDevice m_Device = VK_NULL_HANDLE;
        VkCommandPool m_CommandPool = VK_NULL_HANDLE;
        std::vector<VkCommandBuffer> m_CommandBuffers;
    };
}