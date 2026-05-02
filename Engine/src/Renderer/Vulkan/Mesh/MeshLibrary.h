#pragma once

#include <cstdint>
#include <memory>
#include <unordered_map>
#include <vector>
#include <optional>

#include <glm/glm.hpp>

#include "Renderer/Vulkan/Mesh/StaticMesh.h"

namespace Quiet::Renderer::Vulkan
{
    class PhysicalDevice;
    class LogicalDevice;
    class GraphicsCommands;

    struct MeshStats
    {
        std::uint32_t VertexCount = 0;
        std::uint32_t IndexCount = 0;
        std::uint32_t ImportedMeshCount = 0;

        glm::vec3 BoundsMin{ 0.0f };
        glm::vec3 BoundsMax{ 0.0f };
        bool BoundsValid = false;

        std::string ImporterName;
        std::uint32_t WarningCount = 0;
        std::vector<std::string> Warnings;
    };

    class MeshLibrary
    {
    public:
        MeshLibrary() = default;

        void CreateStaticMesh(const std::string& name,
                              const PhysicalDevice& physicalDevice,
                              const LogicalDevice& logicalDevice,
                              const GraphicsCommands& graphicsCommands,
                              const std::vector<Vertex>& vertices,
                              const std::vector<Index>& indices,
                              MeshStats stats);

        void ReplaceStaticMesh(const std::string& name,
                               const PhysicalDevice& physicalDevice,
                               const LogicalDevice& logicalDevice,
                               const GraphicsCommands& graphicsCommands,
                               const std::vector<Vertex>& vertices,
                               const std::vector<Index>& indices,
                               MeshStats stats);

        std::shared_ptr<StaticMesh> Get(const std::string& name) const;
        std::shared_ptr<StaticMesh> TryGet(const std::string &name) const;
        bool Has(const std::string& name) const;

        [[nodiscard]] std::vector<std::string> GetNames() const;
        [[nodiscard]] std::optional<MeshStats> GetStats(const std::string& name) const;

        [[nodiscard]] std::size_t GetCount() const { return m_Meshes.size(); }

    private:
        std::unordered_map<std::string, std::shared_ptr<StaticMesh>> m_Meshes;
        std::unordered_map<std::string, MeshStats> m_Stats;
    };
}
