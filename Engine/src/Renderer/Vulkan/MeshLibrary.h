#pragma once

#include <memory>
#include <unordered_map>

namespace Quiet::Renderer::Vulkan
{
    class PhysicalDevice;
    class LogicalDevice;
    class GraphicsCommands;
    class StaticMesh;

    class MeshLibrary
    {
    public:
        MeshLibrary() = default;

        void CreateQuad(const std::string &name,
                        const PhysicalDevice &physicalDevice,
                        const LogicalDevice &logicalDevice,
                        const GraphicsCommands& graphicsCommands);

        std::shared_ptr<StaticMesh> Get(const std::string& name) const;
        bool Has(const std::string& name) const;

    private:
        std::unordered_map<std::string, std::shared_ptr<StaticMesh>> m_Meshes;
    };
}
