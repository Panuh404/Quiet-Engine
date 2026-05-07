#include "Renderer/Vulkan/Context/GraphicsCommands.h"

#include <stdexcept>

#include "Renderer/Vulkan/Context/LogicalDevice.h"
#include "Renderer/Vulkan/Context/PhysicalDevice.h"
#include "Renderer/Vulkan/Frame/Swapchain.h"
#include "Quiet/Core/Log.h"

namespace Quiet::Renderer::Vulkan
{
    GraphicsCommands::GraphicsCommands(const PhysicalDevice &physicalDevice,
        const LogicalDevice &logicalDevice,
        const Swapchain &swapchain)
            : m_Device(logicalDevice.GetHandle())
    {
        const auto& queueFamilies = physicalDevice.GetQueueFamilies();

        VkCommandPoolCreateInfo poolCreateInfo{};
        poolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolCreateInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        poolCreateInfo.queueFamilyIndex = queueFamilies.GraphicsFamily.value();

        if (vkCreateCommandPool(m_Device, &poolCreateInfo, nullptr, &m_CommandPool) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create graphics command pool.");
        }

        m_CommandBuffers.resize(swapchain.GetImageViews().size());

        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = m_CommandPool;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = static_cast<std::uint32_t>(m_CommandBuffers.size());

        if (!m_CommandBuffers.empty())
        {
            if (vkAllocateCommandBuffers(m_Device, &allocInfo, m_CommandBuffers.data()) != VK_SUCCESS)
            {
                vkDestroyCommandPool(m_Device, m_CommandPool, nullptr);
                m_CommandPool = VK_NULL_HANDLE;
                throw std::runtime_error("Failed to allocate graphics command buffers.");
            }
        }

        QUIET_CORE_INFO(
            "Created graphics command pool and allocated {} primary command buffers.",
            m_CommandBuffers.size()
        );
    }

    GraphicsCommands::~GraphicsCommands()
    {
        if (!m_CommandBuffers.empty() && m_CommandPool != VK_NULL_HANDLE)
        {
            vkFreeCommandBuffers(
                m_Device,
                m_CommandPool,
                static_cast<std::uint32_t>(m_CommandBuffers.size()),
                m_CommandBuffers.data()
            );
            m_CommandBuffers.clear();
        }

        if (m_CommandPool != VK_NULL_HANDLE)
        {
            vkDestroyCommandPool(m_Device, m_CommandPool, nullptr);
            m_CommandPool = VK_NULL_HANDLE;
        }
    }
}
