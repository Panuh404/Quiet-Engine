#include "VulkanBuffer.h"

#include <stdexcept>

#include "LogicalDevice.h"
#include "PhysicalDevice.h"

namespace Quiet::Renderer::Vulkan
{
    VulkanBuffer::VulkanBuffer(const PhysicalDevice &physicalDevice,
                               const LogicalDevice &logicalDevice,
                               VkDeviceSize size,
                               VkBufferUsageFlags usage,
                               VkMemoryPropertyFlags memoryProperties)
        : m_Device(logicalDevice.GetHandle())
        , m_Size(size)
    {
        VkBufferCreateInfo bufferInfo = {};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = size;
        bufferInfo.usage = usage;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        if (vkCreateBuffer(m_Device, &bufferInfo, nullptr, &m_Buffer) != VK_SUCCESS)
            throw std::runtime_error("Failed to create Vulkan buffer.");

        VkMemoryRequirements requirements{};
        vkGetBufferMemoryRequirements(m_Device, m_Buffer, &requirements);

        VkMemoryAllocateInfo allocInfo = {};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = requirements.size;
        allocInfo.memoryTypeIndex = physicalDevice.FindMemoryType(requirements.memoryTypeBits, memoryProperties);

        if (vkAllocateMemory(m_Device, &allocInfo, nullptr, &m_Memory) != VK_SUCCESS)
        {
            vkDestroyBuffer(m_Device, m_Buffer, nullptr);
            m_Buffer = VK_NULL_HANDLE;
            throw std::runtime_error("Failed to allocate Vulkan buffer memory.");
        }

        if (vkBindBufferMemory(m_Device, m_Buffer, m_Memory, 0) != VK_SUCCESS)
        {
            vkFreeMemory(m_Device, m_Memory, nullptr);
            vkDestroyBuffer(m_Device, m_Buffer, nullptr);
            m_Memory = VK_NULL_HANDLE;
            m_Buffer = VK_NULL_HANDLE;
            throw std::runtime_error("Failed to bind Vulkan buffer memory.");
        }
    }

    VulkanBuffer::~VulkanBuffer()
    {
        if (m_Buffer != VK_NULL_HANDLE)
        {
            vkDestroyBuffer(m_Device, m_Buffer, nullptr);
            m_Buffer = VK_NULL_HANDLE;
        }
        if (m_Memory != VK_NULL_HANDLE)
        {
            vkFreeMemory(m_Device, m_Memory, nullptr);
            m_Memory = VK_NULL_HANDLE;
        }
    }

    void * VulkanBuffer::Map()
    {
        void* data = nullptr;
        if (vkMapMemory(m_Device, m_Memory, 0, m_Size, 0, &data) != VK_SUCCESS)
            throw std::runtime_error("Failed to map Vulkan buffer memory.");

        return data;
    }

    void VulkanBuffer::Unmap()
    {
        vkUnmapMemory(m_Device, m_Memory);
    }
}
