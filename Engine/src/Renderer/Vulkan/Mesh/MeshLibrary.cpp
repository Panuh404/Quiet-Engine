#include "MeshLibrary.h"

#include <algorithm>
#include <stdexcept>

#include "Quiet/Core/Log.h"

namespace Quiet::Renderer::Vulkan
{
    void MeshLibrary::CreateStaticMesh(const std::string& name,
                                       const PhysicalDevice& physicalDevice,
                                       const LogicalDevice& logicalDevice,
                                       const GraphicsCommands& graphicsCommands,
                                       const std::vector<Vertex>& vertices,
                                       const std::vector<Index>& indices,
                                       MeshStats stats)
    {
        if (Has(name))
        {
            QUIET_CORE_ERROR("Mesh '{}' is already registered.", name);
            throw std::runtime_error("Mesh already exists in MeshLibrary: " + name);
        }

        if (vertices.empty() || indices.empty())
        {
            QUIET_CORE_ERROR("Mesh '{}' cannot be created with empty geometry.", name);
            throw std::runtime_error("Static mesh has empty geometry: " + name);
        }

        stats.VertexCount = static_cast<std::uint32_t>(vertices.size());
        stats.IndexCount = static_cast<std::uint32_t>(indices.size());

        m_Meshes[name] = std::make_shared<StaticMesh>(
            physicalDevice,
            logicalDevice,
            graphicsCommands,
            vertices,
            indices
        );
        m_Stats[name] = std::move(stats);
        QUIET_CORE_INFO("Registered mesh resource '{}'.", name);
    }

    void MeshLibrary::ReplaceStaticMesh(const std::string& name,
                                        const PhysicalDevice& physicalDevice,
                                        const LogicalDevice& logicalDevice,
                                        const GraphicsCommands& graphicsCommands,
                                        const std::vector<Vertex>& vertices,
                                        const std::vector<Index>& indices,
                                        MeshStats stats)
    {
        if (vertices.empty() || indices.empty())
        {
            QUIET_CORE_ERROR("Mesh '{}' cannot be replaced with empty geometry.", name);
            throw std::runtime_error("Static mesh has empty geometry: " + name);
        }

        stats.VertexCount = static_cast<std::uint32_t>(vertices.size());
        stats.IndexCount = static_cast<std::uint32_t>(indices.size());

        m_Meshes[name] = std::make_shared<StaticMesh>(
            physicalDevice,
            logicalDevice,
            graphicsCommands,
            vertices,
            indices
        );
        m_Stats[name] = std::move(stats);
        QUIET_CORE_INFO("Replaced mesh resource '{}'.", name);
    }

    std::shared_ptr<StaticMesh> MeshLibrary::Get(const std::string& name) const
    {
        if (auto mesh = TryGet(name)) return mesh;
        throw std::runtime_error("Mesh not found in MeshLibrary: " + name);
    }

    std::shared_ptr<StaticMesh> MeshLibrary::TryGet(const std::string &name) const
    {
        const auto it = m_Meshes.find(name);
        if (it == m_Meshes.end())
        {
            return nullptr;
        }
        return it->second;
    }

    bool MeshLibrary::Has(const std::string& name) const
    {
        return m_Meshes.find(name) != m_Meshes.end();
    }

    std::vector<std::string> MeshLibrary::GetNames() const
    {
        std::vector<std::string> names;
        names.reserve(m_Meshes.size());

        for (const auto& [name, mesh] : m_Meshes)
        {
            (void)mesh;
            names.push_back(name);
        }

        std::sort(names.begin(), names.end());
        return names;
    }

    std::optional<MeshStats> MeshLibrary::GetStats(const std::string &name) const
    {
        const auto it = m_Stats.find(name);
        if (it != m_Stats.end()) return it->second;

        const auto mesh = TryGet(name);
        if (!mesh) return std::nullopt;

        MeshStats fallback{};
        fallback.VertexCount = mesh->GetVertexCount();
        fallback.IndexCount = mesh->GetIndexCount();
        return fallback;
    }
}
