#include "Renderer/Vulkan/Mesh/MeshLibrary.h"

#include <algorithm>
#include <stdexcept>

#include "Quiet/Core/Log.h"

namespace Quiet::Renderer::Vulkan
{
    void MeshLibrary::CreateStaticMesh(Core::UUID assetID,
                                       const PhysicalDevice& physicalDevice,
                                       const LogicalDevice& logicalDevice,
                                       const GraphicsCommands& graphicsCommands,
                                       const std::vector<Vertex>& vertices,
                                       const std::vector<Index>& indices,
                                       std::vector<StaticSubmesh> submeshes,
                                       std::vector<StaticMeshMaterialSlot> importedMaterialSlots,
                                       MeshStats stats)
    {
        if (!assetID.IsValid()) throw std::invalid_argument("Mesh asset UUID is invalid.");
        if (Has(assetID))
        {
            QUIET_CORE_ERROR("Mesh '{}' is already registered.", Core::UUIDToString(assetID));
            throw std::runtime_error("Mesh already exists in MeshLibrary: " + Core::UUIDToString(assetID));
        }

        if (vertices.empty() || indices.empty())
        {
            QUIET_CORE_ERROR("Mesh '{}' cannot be created with empty geometry.", Core::UUIDToString(assetID));
            throw std::runtime_error("Static mesh has empty geometry: " + Core::UUIDToString(assetID));
        }

        stats.VertexCount = static_cast<std::uint32_t>(vertices.size());
        stats.IndexCount = static_cast<std::uint32_t>(indices.size());

        m_Meshes[assetID] = std::make_shared<StaticMesh>(
            physicalDevice,
            logicalDevice,
            graphicsCommands,
            vertices,
            indices,
            std::move(submeshes),
            std::move(importedMaterialSlots)
        );
        m_Stats[assetID] = std::move(stats);
        QUIET_CORE_INFO("Registered mesh resource '{}'.", Core::UUIDToString(assetID));
    }

    void MeshLibrary::ReplaceStaticMesh(Core::UUID assetID,
                                        const PhysicalDevice& physicalDevice,
                                        const LogicalDevice& logicalDevice,
                                        const GraphicsCommands& graphicsCommands,
                                        const std::vector<Vertex>& vertices,
                                        const std::vector<Index>& indices,
                                        std::vector<StaticSubmesh> submeshes,
                                        std::vector<StaticMeshMaterialSlot> importedMaterialSlots,
                                        MeshStats stats)
    {
        if (!assetID.IsValid()) throw std::invalid_argument("Mesh asset UUID is invalid.");
        if (vertices.empty() || indices.empty())
        {
            QUIET_CORE_ERROR("Mesh '{}' cannot be replaced with empty geometry.", Core::UUIDToString(assetID));
            throw std::runtime_error("Static mesh has empty geometry: " + Core::UUIDToString(assetID));
        }

        stats.VertexCount = static_cast<std::uint32_t>(vertices.size());
        stats.IndexCount = static_cast<std::uint32_t>(indices.size());

        m_Meshes[assetID] = std::make_shared<StaticMesh>(
            physicalDevice,
            logicalDevice,
            graphicsCommands,
            vertices,
            indices,
            std::move(submeshes),
            std::move(importedMaterialSlots)
        );
        m_Stats[assetID] = std::move(stats);
        QUIET_CORE_INFO("Replaced mesh resource '{}'.", Core::UUIDToString(assetID));
    }

    std::shared_ptr<StaticMesh> MeshLibrary::Get(Core::UUID assetID) const
    {
        if (auto mesh = TryGet(assetID)) return mesh;
        throw std::runtime_error("Mesh not found in MeshLibrary: " + Core::UUIDToString(assetID));
    }

    std::shared_ptr<StaticMesh> MeshLibrary::TryGet(Core::UUID assetID) const
    {
        const auto it = m_Meshes.find(assetID);
        if (it == m_Meshes.end())
        {
            return nullptr;
        }
        return it->second;
    }

    bool MeshLibrary::Has(Core::UUID assetID) const
    {
        return m_Meshes.find(assetID) != m_Meshes.end();
    }

    std::vector<Core::UUID> MeshLibrary::GetAssetIDs() const
    {
        std::vector<Core::UUID> assetIDs;
        assetIDs.reserve(m_Meshes.size());

        for (const auto& [assetID, mesh] : m_Meshes)
        {
            (void)mesh;
            assetIDs.push_back(assetID);
        }

        std::sort(assetIDs.begin(), assetIDs.end());
        return assetIDs;
    }

    std::optional<MeshStats> MeshLibrary::GetStats(Core::UUID assetID) const
    {
        const auto it = m_Stats.find(assetID);
        if (it != m_Stats.end()) return it->second;

        const auto mesh = TryGet(assetID);
        if (!mesh) return std::nullopt;

        MeshStats fallback{};
        fallback.VertexCount = mesh->GetVertexCount();
        fallback.IndexCount = mesh->GetIndexCount();
        return fallback;
    }
}
