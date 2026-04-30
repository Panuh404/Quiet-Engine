#include "StaticMesh.h"

#include <cstring>
#include <glm/glm.hpp>
#include <glm/common.hpp>

#include "Quiet/Core/Log.h"
#include "Renderer/Vulkan/GraphicsCommands.h"
#include "Renderer/Vulkan/LogicalDevice.h"
#include "Renderer/Vulkan/VulkanBuffer.h"

namespace
{

    void CopyBufferOneShot(VkDevice device,
                           VkCommandPool commandPool,
                           VkQueue queue,
                           VkBuffer srcBuffer,
                           VkBuffer dstBuffer,
                           VkDeviceSize size)
    {
        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = commandPool;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = 1;

        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        if (vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer) != VK_SUCCESS)
            throw std::runtime_error("Failed to allocate one-shot copy command buffer.");

        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS)
        {
            vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
            throw std::runtime_error("Failed to begin one-shot copy command buffer.");
        }

        VkBufferCopy copyRegion{};
        copyRegion.size = size;
        vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

        if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS)
        {
            vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
            throw std::runtime_error("Failed to end one-shot copy command buffer.");
        }

        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffer;

        if (vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE) != VK_SUCCESS)
        {
            vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
            throw std::runtime_error("Failed to submit one-shot copy command buffer.");
        }

        vkQueueWaitIdle(queue);
        vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
    }
}

namespace Quiet::Renderer::Vulkan
{
    VkVertexInputBindingDescription Vertex::GetBindingDescription()
    {
        VkVertexInputBindingDescription binding{};
        binding.binding = 0;
        binding.stride = sizeof(Vertex);
        binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        return binding;
    }

    std::array<VkVertexInputAttributeDescription, 2> Vertex::GetAttributeDescriptions()
    {
        std::array<VkVertexInputAttributeDescription, 2> attributes{};

        attributes[0].location = 0;
        attributes[0].binding = 0;
        attributes[0].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributes[0].offset = offsetof(Vertex, Position);

        attributes[1].location = 1;
        attributes[1].binding = 0;
        attributes[1].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributes[1].offset = offsetof(Vertex, Color);

        return attributes;
    }

    StaticMesh::StaticMesh(const PhysicalDevice& physicalDevice,
                           const LogicalDevice& logicalDevice,
                           const GraphicsCommands& graphicsCommands,
                           const std::vector<Vertex>& vertices,
                           const std::vector<Index>& indices)
    {
        const VkDeviceSize vertexBufferSize = vertices.size() * sizeof(Vertex);
        const VkDeviceSize indexBufferSize = indices.size() * sizeof(Index);

        {
            VulkanBuffer stagingVertexBuffer(
                physicalDevice,
                logicalDevice,
                vertexBufferSize,
                VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

            void* mapped = stagingVertexBuffer.Map();
            std::memcpy(mapped, vertices.data(), static_cast<std::size_t>(vertexBufferSize));
            stagingVertexBuffer.Unmap();

            m_VertexBuffer = std::make_unique<VulkanBuffer>(
                physicalDevice,
                logicalDevice,
                vertexBufferSize,
                VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

            CopyBufferOneShot(
                logicalDevice.GetHandle(),
                graphicsCommands.GetCommandPool(),
                logicalDevice.GetGraphicsQueue(),
                stagingVertexBuffer.GetHandle(),
                m_VertexBuffer->GetHandle(),
                vertexBufferSize);
        }
        m_VertexCount = static_cast<std::uint32_t>(vertices.size());
        if (vertices.empty())
        {
            throw std::runtime_error("StaticMesh cannot be created with no vertices.");
        }

        m_LocalBoundsMin = vertices[0].Position;
        m_LocalBoundsMax = vertices[0].Position;

        for (const auto& vertex : vertices)
        {
            m_LocalBoundsMin = glm::min(m_LocalBoundsMin, vertex.Position);
            m_LocalBoundsMax = glm::max(m_LocalBoundsMax, vertex.Position);
        }

        {
            VulkanBuffer stagingIndexBuffer(
                physicalDevice,
                logicalDevice,
                indexBufferSize,
                VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

            void* mapped = stagingIndexBuffer.Map();
            std::memcpy(mapped, indices.data(), static_cast<std::size_t>(indexBufferSize));
            stagingIndexBuffer.Unmap();

            m_IndexBuffer =std::make_unique<VulkanBuffer>(
                physicalDevice,
                logicalDevice,
                indexBufferSize,
                VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

            CopyBufferOneShot(
                logicalDevice.GetHandle(),
                graphicsCommands.GetCommandPool(),
                logicalDevice.GetGraphicsQueue(),
                stagingIndexBuffer.GetHandle(),
                m_IndexBuffer->GetHandle(),
                indexBufferSize);
        }
        m_IndexCount = static_cast<std::uint32_t>(indices.size());

        QUIET_CORE_INFO(
            "Created static mesh. vertices={} indices={}",
            m_VertexCount,
            m_IndexCount
        );
    }

    StaticMesh::~StaticMesh() = default;

    VkBuffer StaticMesh::GetVertexBuffer() const
    {
        return m_VertexBuffer->GetHandle();
    }

    VkBuffer StaticMesh::GetIndexBuffer() const
    {
        return m_IndexBuffer->GetHandle();
    }
}
