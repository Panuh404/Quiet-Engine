#include "Quiet/Project/ProjectAssetLoader.h"

#include <filesystem>

#include "Quiet/Core/Log.h"
#include "Quiet/Project/ProjectContext.h"
#include "Renderer/Vulkan/MeshImporter.h"
#include "Renderer/Vulkan/MeshLibrary.h"


namespace Quiet::Project
{
    ProjectAssetLoader::ProjectAssetLoader(ProjectContext &projectContext,
                                           Renderer::Vulkan::MeshLibrary &meshLibrary,
                                           const Renderer::Vulkan::PhysicalDevice &physicalDevice,
                                           const Renderer::Vulkan::LogicalDevice &logicalDevice,
                                           const Renderer::Vulkan::GraphicsCommands &graphicsCommands)
                                               : m_ProjectContext(projectContext)
                                               , m_MeshLibrary(meshLibrary)
                                               , m_PhysicalDevice(physicalDevice)
                                               , m_LogicalDevice(logicalDevice)
                                               , m_GraphicsCommands(graphicsCommands) {}

    bool ProjectAssetLoader::ReloadMeshAssets()
    {
        const auto meshAssets =
            AssetRegistryIO::GetAssetsByType(m_ProjectContext.GetAssetRegistry(), AssetType::Mesh);

        bool allSupportedAssetsLoaded = true;
        for (const AssetRecord* record : meshAssets)
        {
            if (!record) continue;
            if (record->BuiltIn)
            {
                if (!LoadBuiltInMesh(*record)) allSupportedAssetsLoaded = false;
                continue;
            }
            if (!LoadExternalMesh(*record))
            {
                allSupportedAssetsLoaded = false;
            }
        }
        return allSupportedAssetsLoaded;
    }

    bool ProjectAssetLoader::LoadBuiltInMesh(const AssetRecord &record)
    {
        if (m_MeshLibrary.Has(record.ID)) return true;

        if (record.ID == "Cube")
        {
            m_MeshLibrary.CreateCube(
                record.ID,
                m_PhysicalDevice,
                m_LogicalDevice,
                m_GraphicsCommands
            );
            return true;
        }

        if (record.ID == "Quad")
        {
            m_MeshLibrary.CreateQuad(
                record.ID,
                m_PhysicalDevice,
                m_LogicalDevice,
                m_GraphicsCommands
            );
            return true;
        }

        QUIET_CORE_WARN(
            "Built-in mesh asset '{}' is registered but has no loader implementation.",
            record.ID
        );
        return false;
    }

    bool ProjectAssetLoader::LoadExternalMesh(const Project::AssetRecord &record)
    {
        if (m_MeshLibrary.Has(record.ID)) return true;

        if (record.Path.empty())
        {
            QUIET_CORE_WARN(
                "Mesh asset '{}' is registered without a source path.",
                record.ID
            );
            return false;
        }

        const std::filesystem::path assetPath =
            (m_ProjectContext.GetRootPath() / record.Path).lexically_normal();

        if (!std::filesystem::exists(assetPath))
        {
            QUIET_CORE_WARN(
                "Mesh asset '{}' points to missing file '{}'.",
                record.ID,
                assetPath.string()
            );
            return false;
        }

        std::vector<Renderer::Vulkan::Vertex> vertices;
        std::vector<std::uint16_t> indices;
        std::string error;

        if (!Renderer::Vulkan::MeshImporter::LoadFromFile(assetPath, vertices, indices, error))
        {
            QUIET_CORE_WARN(
                "Failed to import mesh asset '{}' from '{}': {}",
                record.ID,
                assetPath.string(),
                error
            );
            return false;
        }

        try
        {
            m_MeshLibrary.CreateStaticMesh(
                record.ID,
                m_PhysicalDevice,
                m_LogicalDevice,
                m_GraphicsCommands,
                vertices,
                indices
            );
        }
        catch (const std::exception& e)
        {
            QUIET_CORE_ERROR(
                "Failed to create runtime mesh resource '{}' from '{}': {}",
                record.ID,
                assetPath.string(),
                e.what()
            );
            return false;
        }

        QUIET_CORE_INFO(
            "Loaded mesh asset '{}' from '{}'. vertices={} indices={}",
            record.ID,
            assetPath.string(),
            vertices.size(),
            indices.size()
        );

        return true;
    }
}
