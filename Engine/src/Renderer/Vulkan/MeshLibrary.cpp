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
        if (Has(name))
        {
            QUIET_CORE_ERROR("Mesh '{}' is already registered.", name);
            throw std::runtime_error("Mesh already exists in MeshLibrary: " + name);
        }

        m_Meshes[name] = std::make_shared<StaticMesh>(physicalDevice, logicalDevice, graphicsCommands);
        QUIET_CORE_INFO("Registered mesh resource '{}'.", name);
    }

    std::shared_ptr<StaticMesh> MeshLibrary::Get(const std::string& name) const
    {
        const auto it = m_Meshes.find(name);
        if (it == m_Meshes.end())
            throw std::runtime_error("Mesh not found in MeshLibrary: " + name);
        return it->second;
    }

    bool MeshLibrary::Has(const std::string& name) const
    {
        return m_Meshes.find(name) != m_Meshes.end();
    }
}
