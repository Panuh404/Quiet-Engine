#pragma once

#include <cstdint>
#include <memory>
#include <unordered_map>
#include <vector>
#include <optional>

#include "StaticMesh.h"


namespace Quiet::Renderer::Vulkan
{
    class PhysicalDevice;
    class LogicalDevice;
    class GraphicsCommands;

    struct MeshStats
    {
        std::uint32_t VertexCount = 0;
        std::uint32_t IndexCount = 0;
    };

    class MeshLibrary
    {
    public:
        MeshLibrary() = default;

        void CreateQuad(const std::string &name,
                        const PhysicalDevice &physicalDevice,
                        const LogicalDevice &logicalDevice,
                        const GraphicsCommands& graphicsCommands);

        void CreateCube(const std::string& name,
                        const PhysicalDevice& physicalDevice,
                        const LogicalDevice& logicalDevice,
                        const GraphicsCommands& graphicsCommands);

        void CreateStaticMesh(const std::string& name,
                              const PhysicalDevice& physicalDevice,
                              const LogicalDevice& logicalDevice,
                              const GraphicsCommands& graphicsCommands,
                              const std::vector<Vertex>& vertices,
                              const std::vector<Index>& indices);

        void ReplaceStaticMesh(const std::string& name,
                               const PhysicalDevice& physicalDevice,
                               const LogicalDevice& logicalDevice,
                               const GraphicsCommands& graphicsCommands,
                               const std::vector<Vertex>& vertices,
                               const std::vector<Index>& indices);

        std::shared_ptr<StaticMesh> Get(const std::string& name) const;

        std::shared_ptr<StaticMesh> TryGet(const std::string &name) const;
        bool Has(const std::string& name) const;

        [[nodiscard]] std::vector<std::string> GetNames() const;
        [[nodiscard]] std::optional<MeshStats> GetStats(const std::string& name) const;

        static std::vector<Vertex> BuildQuadVertices();
        static std::vector<Index> BuildQuadIndices();

        static std::vector<Vertex> BuildCubeVertices();
        static std::vector<Index> BuildCubeIndices();

        [[nodiscard]] std::size_t GetCount() const { return m_Meshes.size(); }

    private:
        std::unordered_map<std::string, std::shared_ptr<StaticMesh>> m_Meshes;
    };
}
