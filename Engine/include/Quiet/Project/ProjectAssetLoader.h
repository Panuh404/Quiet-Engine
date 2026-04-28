#pragma once

#include "Quiet/Project/AssetRegistry.h"

namespace Quiet::Project
{
    class ProjectContext;
}

namespace Quiet::Renderer::Vulkan
{
    class MeshLibrary;
    class PhysicalDevice;
    class LogicalDevice;
    class GraphicsCommands;
}

namespace Quiet::Project
{
    class ProjectAssetLoader
    {
    public:
        ProjectAssetLoader(ProjectContext& projectContext,
                           Renderer::Vulkan::MeshLibrary& meshLibrary,
                           const Renderer::Vulkan::PhysicalDevice& physicalDevice,
                           const Renderer::Vulkan::LogicalDevice& logicalDevice,
                           const Renderer::Vulkan::GraphicsCommands& graphicsCommands);

        bool ReloadMeshAssets();

    private:
        bool LoadBuiltInMesh(const AssetRecord& record);
        bool LoadExternalMesh(const Project::AssetRecord& record);

    private:
        ProjectContext& m_ProjectContext;
        Renderer::Vulkan::MeshLibrary& m_MeshLibrary;
        const Renderer::Vulkan::PhysicalDevice& m_PhysicalDevice;
        const Renderer::Vulkan::LogicalDevice& m_LogicalDevice;
        const Renderer::Vulkan::GraphicsCommands& m_GraphicsCommands;
    };
}