#pragma once

#include <array>
#include <memory>
#include <vector>

#include <vulkan/vulkan.h>

#include "VulkanBuffer.h"
#include "glm/vec3.hpp"
#include "Quiet/Math/Geometry.h"

namespace Quiet::Renderer::Vulkan
{
    class PhysicalDevice;
    class LogicalDevice;
    class GraphicsCommands;
    class VulkanBuffer;

    struct Vertex
    {
        glm::vec3 Position;
        glm::vec3 Color;

        static VkVertexInputBindingDescription GetBindingDescription();
        static std::array<VkVertexInputAttributeDescription, 2> GetAttributeDescriptions();
    };

    class StaticMesh
    {
    public:
        StaticMesh(const PhysicalDevice& physicalDevice,
                   const LogicalDevice& logicalDevice,
                   const GraphicsCommands& graphicsCommands,
                   const std::vector<Vertex>& vertices,
                   const std::vector<std::uint16_t>& indices);
        ~StaticMesh();

        StaticMesh(const StaticMesh&) = delete;
        StaticMesh& operator=(const StaticMesh&) = delete;
        StaticMesh(StaticMesh&&) = delete;
        StaticMesh& operator=(StaticMesh&&) = delete;

        [[nodiscard]] VkBuffer GetVertexBuffer() const;
        [[nodiscard]] VkBuffer GetIndexBuffer() const;
        [[nodiscard]] std::uint32_t GetIndexCount() const { return m_IndexCount; }

        [[nodiscard]] const glm::vec3& GetLocalBoundsMin() const { return m_LocalBoundsMin; }
        [[nodiscard]] const glm::vec3& GetLocalBoundsMax() const { return m_LocalBoundsMax; }
        [[nodiscard]] Math::AABB GetLocalBounds() const { return { m_LocalBoundsMin, m_LocalBoundsMax }; }

    private:
        std::unique_ptr<VulkanBuffer> m_VertexBuffer;
        std::unique_ptr<VulkanBuffer> m_IndexBuffer;
        std::uint32_t m_VertexCount = 0;
        std::uint32_t m_IndexCount = 0;
        glm::vec3 m_LocalBoundsMin{ 0.0f };
        glm::vec3 m_LocalBoundsMax{ 0.0f };
    };
}
