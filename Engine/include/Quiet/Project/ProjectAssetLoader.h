#pragma once

#include <string_view>

#include "Quiet/Project/AssetRegistry.h"

namespace Quiet::Project
{
    class ProjectContext;
}

namespace Quiet::Renderer::Vulkan
{
    class MeshLibrary;
    class TextureLibrary;
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
                           Renderer::Vulkan::TextureLibrary& textureLibrary,
                           const Renderer::Vulkan::PhysicalDevice& physicalDevice,
                           const Renderer::Vulkan::LogicalDevice& logicalDevice,
                           const Renderer::Vulkan::GraphicsCommands& graphicsCommands);

        void BindRendererResources(const Renderer::Vulkan::PhysicalDevice& physicalDevice,
                                   const Renderer::Vulkan::LogicalDevice& logicalDevice,
                                   const Renderer::Vulkan::GraphicsCommands& graphicsCommands);

        bool ReloadMeshAssets(bool forceReimport = false);
        bool ReloadTextureAssets(bool forceReimport = false);
        bool ReloadAssets(bool forceReimport = false);
        bool ReloadAssetByID(std::string_view assetID, bool forceReimport = true);

    private:
        bool LoadBuiltInMesh(const AssetRecord& record);
        bool LoadExternalMesh(const Project::AssetRecord& record, bool forceReimport);
        bool LoadExternalTexture(const AssetRecord& record, bool forceReimport);

    private:
        ProjectContext& m_ProjectContext;
        Renderer::Vulkan::MeshLibrary& m_MeshLibrary;
        Renderer::Vulkan::TextureLibrary& m_TextureLibrary;
        const Renderer::Vulkan::PhysicalDevice* m_PhysicalDevice = nullptr;
        const Renderer::Vulkan::LogicalDevice* m_LogicalDevice = nullptr;
        const Renderer::Vulkan::GraphicsCommands* m_GraphicsCommands = nullptr;
    };
}