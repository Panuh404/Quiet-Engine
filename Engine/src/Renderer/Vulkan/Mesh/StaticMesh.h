#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include <vulkan/vulkan.h>

#include "glm/vec3.hpp"
#include "glm/vec2.hpp"
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
        glm::vec3 Normal;
        glm::vec2 TexCoord;

        static VkVertexInputBindingDescription GetBindingDescription();
        static std::array<VkVertexInputAttributeDescription, 4> GetAttributeDescriptions();
    };
    using Index = std::uint32_t;

    struct StaticSubmesh
    {
        std::string Name;

        std::uint32_t BaseVertex = 0;
        std::uint32_t VertexCount = 0;

        std::uint32_t BaseIndex = 0;
        std::uint32_t IndexCount = 0;

        std::uint32_t MaterialSlot = 0;
    };

    class StaticMesh
    {
    public:
        StaticMesh(const PhysicalDevice& physicalDevice,
                   const LogicalDevice& logicalDevice,
                   const GraphicsCommands& graphicsCommands,
                   const std::vector<Vertex>& vertices,
                   const std::vector<Index>& indices,
                   std::vector<StaticSubmesh> submeshes = {});
        ~StaticMesh();

        StaticMesh(const StaticMesh&) = delete;
        StaticMesh& operator=(const StaticMesh&) = delete;
        StaticMesh(StaticMesh&&) = delete;
        StaticMesh& operator=(StaticMesh&&) = delete;

        [[nodiscard]] VkBuffer GetVertexBuffer() const;
        [[nodiscard]] VkBuffer GetIndexBuffer() const;
        [[nodiscard]] std::uint32_t GetIndexCount() const { return m_IndexCount; }
        [[nodiscard]] std::uint32_t GetVertexCount() const { return m_VertexCount; }

        [[nodiscard]] const std::vector<StaticSubmesh>& GetSubmeshes() const { return m_Submeshes; }

        [[nodiscard]] const glm::vec3& GetLocalBoundsMin() const { return m_LocalBoundsMin; }
        [[nodiscard]] const glm::vec3& GetLocalBoundsMax() const { return m_LocalBoundsMax; }
        [[nodiscard]] Math::AABB GetLocalBounds() const { return { m_LocalBoundsMin, m_LocalBoundsMax }; }

    private:
        std::unique_ptr<VulkanBuffer> m_VertexBuffer;
        std::unique_ptr<VulkanBuffer> m_IndexBuffer;
        std::vector<StaticSubmesh> m_Submeshes;

        std::uint32_t m_VertexCount = 0;
        std::uint32_t m_IndexCount = 0;

        glm::vec3 m_LocalBoundsMin{ 0.0f };
        glm::vec3 m_LocalBoundsMax{ 0.0f };
    };
}
