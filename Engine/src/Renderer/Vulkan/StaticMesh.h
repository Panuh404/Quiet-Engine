#pragma once

#include <array>
#include <cstdint>
#include <memory>

#include <vulkan/vulkan.h>

#include "VulkanBuffer.h"

namespace Quiet::Renderer::Vulkan
{
    class PhysicalDevice;
    class LogicalDevice;
    class GraphicsCommands;
    class VulkanBuffer;

    struct Vertex
    {
        float Position[3];
        float Color[3];

        static VkVertexInputBindingDescription GetBindingDescription();
        static std::array<VkVertexInputAttributeDescription, 2> GetAttributeDescriptions();
    };

    class StaticMesh
    {
    public:
        StaticMesh(const PhysicalDevice& physicalDevice,
                     const LogicalDevice& logicalDevice,
                     const GraphicsCommands& graphicsCommands);
        ~StaticMesh();

        StaticMesh(const StaticMesh&) = delete;
        StaticMesh& operator=(const StaticMesh&) = delete;
        StaticMesh(StaticMesh&&) = delete;
        StaticMesh& operator=(StaticMesh&&) = delete;

        [[nodiscard]] VkBuffer GetVertexBuffer() const;
        [[nodiscard]] VkBuffer GetIndexBuffer() const;
        [[nodiscard]] std::uint32_t GetIndexCount() const { return m_IndexCount; }

    private:
        std::unique_ptr<VulkanBuffer> m_VertexBuffer;
        std::unique_ptr<VulkanBuffer> m_IndexBuffer;
        std::uint32_t m_IndexCount = 0;
    };
}
