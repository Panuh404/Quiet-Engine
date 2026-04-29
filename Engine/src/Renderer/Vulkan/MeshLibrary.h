#pragma once

#include <cstdint>
#include <memory>
#include <unordered_map>
#include <vector>
#include <optional>

namespace Quiet::Renderer::Vulkan
{
    struct Vertex;
    class PhysicalDevice;
    class LogicalDevice;
    class GraphicsCommands;
    class StaticMesh;

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
                              const std::vector<std::uint16_t>& indices);

        std::shared_ptr<StaticMesh> Get(const std::string& name) const;

        std::shared_ptr<StaticMesh> TryGet(const std::string &name) const;
        bool Has(const std::string& name) const;

        [[nodiscard]] std::vector<std::string> GetNames() const;
        [[nodiscard]] std::optional<MeshStats> GetStats(const std::string& name) const;

        static std::vector<Vertex> BuildQuadVertices();
        static std::vector<std::uint16_t> BuildQuadIndices();

        static std::vector<Vertex> BuildCubeVertices();
        static std::vector<std::uint16_t> BuildCubeIndices();

        [[nodiscard]] std::size_t GetCount() const { return m_Meshes.size(); }

    private:
        std::unordered_map<std::string, std::shared_ptr<StaticMesh>> m_Meshes;
    };
}
