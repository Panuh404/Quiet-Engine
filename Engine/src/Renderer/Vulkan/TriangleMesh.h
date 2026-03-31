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

    class TriangleMesh
    {
    public:
        TriangleMesh(const PhysicalDevice& physicalDevice,
                     const LogicalDevice& logicalDevice,
                     const GraphicsCommands& graphicsCommands);
        ~TriangleMesh();

        TriangleMesh(const TriangleMesh&) = delete;
        TriangleMesh& operator=(const TriangleMesh&) = delete;
        TriangleMesh(TriangleMesh&&) = delete;
        TriangleMesh& operator=(TriangleMesh&&) = delete;

        [[nodiscard]] VkBuffer GetVertexBuffer() const;
        [[nodiscard]] std::uint32_t GetVertexCount() const { return m_VertexCount; }

    private:
        std::unique_ptr<VulkanBuffer> m_VertexBuffer;
        std::uint32_t m_VertexCount = 0;
    };
}
