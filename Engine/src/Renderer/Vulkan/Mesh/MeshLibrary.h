#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>
#include <utility>

#include <glm/glm.hpp>

#include "Quiet/Core/UUID.h"
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
        std::uint32_t SubmeshCount = 0;
        std::uint32_t MaterialCount = 0;
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

        void CreateStaticMesh(Core::UUID assetID,
                              const PhysicalDevice& physicalDevice,
                              const LogicalDevice& logicalDevice,
                              const GraphicsCommands& graphicsCommands,
                              const std::vector<Vertex>& vertices,
                              const std::vector<Index>& indices,
                              std::vector<StaticSubmesh> submeshes,
                              std::vector<StaticMeshMaterialSlot> importedMaterialSlots,
                              MeshStats stats);

        void ReplaceStaticMesh(Core::UUID assetID,
                               const PhysicalDevice& physicalDevice,
                               const LogicalDevice& logicalDevice,
                               const GraphicsCommands& graphicsCommands,
                               const std::vector<Vertex>& vertices,
                               const std::vector<Index>& indices,
                               std::vector<StaticSubmesh> submeshes,
                               std::vector<StaticMeshMaterialSlot> importedMaterialSlots,
                               MeshStats stats);

        std::shared_ptr<StaticMesh> Get(Core::UUID assetID) const;
        std::shared_ptr<StaticMesh> TryGet(Core::UUID assetID) const;
        bool Has(Core::UUID assetID) const;

        [[nodiscard]] std::vector<Core::UUID> GetAssetIDs() const;
        [[nodiscard]] std::optional<MeshStats> GetStats(Core::UUID assetID) const;

        [[nodiscard]] std::size_t GetCount() const { return m_Meshes.size(); }

    private:
        std::unordered_map<Core::UUID, std::shared_ptr<StaticMesh>> m_Meshes;
        std::unordered_map<Core::UUID, MeshStats> m_Stats;
    };
}
