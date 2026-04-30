#include "MeshLibrary.h"

#include "Quiet/Core/Log.h"
#include "Renderer/Vulkan/StaticMesh.h"

namespace Quiet::Renderer::Vulkan
{
    void MeshLibrary::CreateQuad(const std::string &name,
                                 const PhysicalDevice &physicalDevice,
                                 const LogicalDevice &logicalDevice,
                                 const GraphicsCommands& graphicsCommands)
    {
        CreateStaticMesh(
            name,
            physicalDevice,
            logicalDevice,
            graphicsCommands,
            BuildQuadVertices(),
            BuildQuadIndices()
        );
    }

    void MeshLibrary::CreateCube(const std::string& name,
                                 const PhysicalDevice& physicalDevice,
                                 const LogicalDevice& logicalDevice,
                                 const GraphicsCommands& graphicsCommands)
    {
        CreateStaticMesh(
            name,
            physicalDevice,
            logicalDevice,
            graphicsCommands,
            BuildCubeVertices(),
            BuildCubeIndices()
        );
    }

    void MeshLibrary::CreateStaticMesh(const std::string& name,
                                       const PhysicalDevice& physicalDevice,
                                       const LogicalDevice& logicalDevice,
                                       const GraphicsCommands& graphicsCommands,
                                       const std::vector<Vertex>& vertices,
                                       const std::vector<std::uint16_t>& indices)
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
        m_Meshes[name] = std::make_shared<StaticMesh>(
            physicalDevice,
            logicalDevice,
            graphicsCommands,
            vertices,
            indices
        );
        QUIET_CORE_INFO("Registered mesh resource '{}'.", name);
    }

    void MeshLibrary::ReplaceStaticMesh(const std::string& name,
                                        const PhysicalDevice& physicalDevice,
                                        const LogicalDevice& logicalDevice,
                                        const GraphicsCommands& graphicsCommands,
                                        const std::vector<Vertex>& vertices,
                                        const std::vector<std::uint16_t>& indices)
    {
        if (vertices.empty() || indices.empty())
        {
            QUIET_CORE_ERROR("Mesh '{}' cannot be replaced with empty geometry.", name);
            throw std::runtime_error("Static mesh has empty geometry: " + name);
        }

        m_Meshes[name] = std::make_shared<StaticMesh>(
            physicalDevice,
            logicalDevice,
            graphicsCommands,
            vertices,
            indices
        );

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
        const auto mesh = TryGet(name);
        if (!mesh) return std::nullopt;

        return MeshStats{ mesh->GetVertexCount(), mesh->GetIndexCount() };
    }

    std::vector<Vertex> MeshLibrary::BuildQuadVertices()
    {
        return {{
            {{ -0.5f, -0.5f, 0.0f }, { 1.0f, 0.2f, 0.2f }},
            {{  0.5f, -0.5f, 0.0f }, { 0.2f, 1.0f, 0.2f }},
            {{  0.5f,  0.5f, 0.0f }, { 0.2f, 0.4f, 1.0f }},
            {{ -0.5f,  0.5f, 0.0f }, { 1.0f, 1.0f, 0.2f }}
        }};
    }

    std::vector<std::uint16_t> MeshLibrary::BuildQuadIndices()
    {
        return {{ 0, 1, 2, 2, 3, 0 }};
    }

    std::vector<Vertex> MeshLibrary::BuildCubeVertices()
    {
        return {
            {{-0.5f, -0.5f, -0.5f}, {1.0f, 0.2f, 0.2f}},
            {{ 0.5f, -0.5f, -0.5f}, {0.2f, 1.0f, 0.2f}},
            {{ 0.5f,  0.5f, -0.5f}, {0.2f, 0.4f, 1.0f}},
            {{-0.5f,  0.5f, -0.5f}, {1.0f, 1.0f, 0.2f}},
            {{-0.5f, -0.5f,  0.5f}, {1.0f, 0.2f, 1.0f}},
            {{ 0.5f, -0.5f,  0.5f}, {0.2f, 1.0f, 1.0f}},
            {{ 0.5f,  0.5f,  0.5f}, {1.0f, 1.0f, 1.0f}},
            {{-0.5f,  0.5f,  0.5f}, {0.4f, 0.4f, 0.4f}}
        };
    }

    std::vector<std::uint16_t> MeshLibrary::BuildCubeIndices()
    {
        return {
            // back
            0, 1, 2, 2, 3, 0,
            // front
            4, 5, 6, 6, 7, 4,
            // left
            0, 4, 7, 7, 3, 0,
            // right
            1, 5, 6, 6, 2, 1,
            // bottom
            0, 1, 5, 5, 4, 0,
            // top
            3, 2, 6, 6, 7, 3
        };
    }
}
