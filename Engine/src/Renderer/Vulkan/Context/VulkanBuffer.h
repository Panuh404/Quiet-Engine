#pragma once

#include <cstdint>
#include <vulkan/vulkan.h>

namespace Quiet::Renderer::Vulkan
{
    class PhysicalDevice;
    class LogicalDevice;
    
    class VulkanBuffer
    {
    public:
        VulkanBuffer(const PhysicalDevice& physicalDevice,
                     const LogicalDevice& logicalDevice,
                     VkDeviceSize size,
                     VkBufferUsageFlags usage,
                     VkMemoryPropertyFlags memoryProperties);
        ~VulkanBuffer();

        VulkanBuffer(const VulkanBuffer&) = delete;
        VulkanBuffer& operator=(const VulkanBuffer&) = delete;
        VulkanBuffer(VulkanBuffer&&) = delete;
        VulkanBuffer& operator=(VulkanBuffer&&) = delete;

        VkBuffer GetHandle() const { return m_Buffer; }
        VkDeviceMemory GetMemory() const { return m_Memory; }
        VkDeviceSize GetSize() const { return m_Size; }

        void* Map();
        void Unmap();

    private:
        VkDevice m_Device = VK_NULL_HANDLE;
        VkBuffer m_Buffer = VK_NULL_HANDLE;
        VkDeviceMemory m_Memory = VK_NULL_HANDLE;
        VkDeviceSize m_Size = 0;
    };
}