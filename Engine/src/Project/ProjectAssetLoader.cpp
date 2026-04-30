#include "Quiet/Project/ProjectAssetLoader.h"

#include <filesystem>

#include "Quiet/Core/Log.h"
#include "Quiet/Project/ProjectContext.h"
#include "Renderer/Vulkan/Mesh/MeshImporter.h"
#include "Renderer/Vulkan/Mesh/MeshLibrary.h"

namespace
{
    Quiet::Renderer::Vulkan::MeshStats MakeMeshStatsFromImportResult(
        const Quiet::Renderer::Vulkan::MeshImportResult& importResult)
    {
        Quiet::Renderer::Vulkan::MeshStats stats{};
        stats.VertexCount = static_cast<std::uint32_t>(importResult.Vertices.size());
        stats.IndexCount = static_cast<std::uint32_t>(importResult.Indices.size());
        stats.ImportedMeshCount = importResult.ImportedMeshCount;
        stats.BoundsValid = importResult.Bounds.Valid;
        stats.BoundsMin = importResult.Bounds.Min;
        stats.BoundsMax = importResult.Bounds.Max;
        stats.ImporterName = importResult.ImporterName;
        stats.WarningCount = static_cast<std::uint32_t>(importResult.Warnings.size());
        return stats;
    }
}

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

    bool ProjectAssetLoader::ReloadMeshAssets(const bool forceReimport)
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
            if (!LoadExternalMesh(*record, forceReimport))
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

    bool ProjectAssetLoader::LoadExternalMesh(const Project::AssetRecord& record,
                                          const bool forceReimport)
    {
        const bool alreadyLoaded = m_MeshLibrary.Has(record.ID);
        if (alreadyLoaded && !forceReimport) return true;
        if (record.Path.empty())
        {
            QUIET_CORE_WARN("Mesh asset '{}' is registered without a source path.", record.ID);
            return false;
        }

        const std::filesystem::path rawAssetPath = record.Path;
        const std::filesystem::path assetPath =
            rawAssetPath.is_absolute()
                ? rawAssetPath.lexically_normal()
                : (m_ProjectContext.GetRootPath() / rawAssetPath).lexically_normal();

        if (!std::filesystem::exists(assetPath))
        {
            QUIET_CORE_WARN(
                "Mesh asset '{}' points to missing file '{}'.",
                record.ID,
                assetPath.string()
            );
            return false;
        }

        Renderer::Vulkan::MeshImportResult importResult = Renderer::Vulkan::MeshImporter::LoadFromFile(assetPath);
        if (!importResult.Succeeded())
        {
            QUIET_CORE_WARN(
                "Failed to import mesh asset '{}' from '{}': {}",
                record.ID,
                assetPath.string(),
                importResult.Error
            );
            return false;
        }

        const Renderer::Vulkan::MeshStats stats = MakeMeshStatsFromImportResult(importResult);
        try
        {
            if (alreadyLoaded)
            {
                m_MeshLibrary.ReplaceStaticMesh(
                    record.ID,
                    m_PhysicalDevice,
                    m_LogicalDevice,
                    m_GraphicsCommands,
                    importResult.Vertices,
                    importResult.Indices,
                    stats
                );
            }
            else
            {
                m_MeshLibrary.CreateStaticMesh(
                    record.ID,
                    m_PhysicalDevice,
                    m_LogicalDevice,
                    m_GraphicsCommands,
                    importResult.Vertices,
                    importResult.Indices,
                    stats
                );
            }
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
            "{} mesh asset '{}' from '{}'. meshes={} vertices={} indices={} boundsValid={}",
            alreadyLoaded ? "Reloaded" : "Loaded",
            record.ID,
            assetPath.string(),
            importResult.ImportedMeshCount,
            importResult.Vertices.size(),
            importResult.Indices.size(),
            importResult.Bounds.Valid
        );

        return true;
    }
}
