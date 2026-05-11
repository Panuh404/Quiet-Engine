#include "Quiet/Project/ProjectAssetLoader.h"

#include <cstdint>
#include <exception>
#include <filesystem>
#include <string>
#include <utility>

#include "Quiet/Core/Log.h"
#include "Quiet/Project/ProjectContext.h"
#include "Renderer/Vulkan/Mesh/MeshImporter.h"
#include "Renderer/Vulkan/Mesh/MeshLibrary.h"
#include "Renderer/Vulkan/Mesh/MeshPrimitives.h"
#include "Renderer/Vulkan/Texture/TextureImporter.h"
#include "Renderer/Vulkan/Texture/TextureImportResult.h"
#include "Renderer/Vulkan/Texture/TextureLibrary.h"
#include "Renderer/Vulkan/Texture/TexturePixelData.h"

namespace
{
    Quiet::Renderer::Vulkan::MeshStats MakeMeshStatsFromImportResult(
        const Quiet::Renderer::Vulkan::MeshImportResult& importResult)
    {
        Quiet::Renderer::Vulkan::MeshStats stats{};
        stats.VertexCount = static_cast<std::uint32_t>(importResult.Vertices.size());
        stats.IndexCount = static_cast<std::uint32_t>(importResult.Indices.size());
        stats.SubmeshCount = static_cast<std::uint32_t>(importResult.Submeshes.size());
        stats.ImportedMeshCount = importResult.ImportedMeshCount;
        stats.BoundsValid = importResult.Bounds.Valid;
        stats.BoundsMin = importResult.Bounds.Min;
        stats.BoundsMax = importResult.Bounds.Max;
        stats.ImporterName = importResult.ImporterName;
        stats.WarningCount = static_cast<std::uint32_t>(importResult.Warnings.size());
        stats.Warnings = importResult.Warnings;
        return stats;
    }

    Quiet::Renderer::Vulkan::TextureStats MakeTextureStatsFromImportResult(
        const Quiet::Renderer::Vulkan::TextureImportResult& importResult)
    {
        Quiet::Renderer::Vulkan::TextureStats stats{};

        stats.Format = importResult.Format;
        stats.Width = importResult.Width;
        stats.Height = importResult.Height;
        stats.DimensionsValid = importResult.DimensionsValid;
        stats.FileSizeBytes = importResult.FileSizeBytes;
        stats.WarningCount = static_cast<std::uint32_t>(importResult.Warnings.size());
        stats.Warnings = importResult.Warnings;

        return stats;
    }

    std::vector<Quiet::Renderer::Vulkan::StaticSubmesh> MakeStaticSubmeshesFromImportResult(
        const Quiet::Renderer::Vulkan::MeshImportResult& importResult)
    {
        std::vector<Quiet::Renderer::Vulkan::StaticSubmesh> submeshes;
        submeshes.reserve(importResult.Submeshes.size());

        for (const Quiet::Renderer::Vulkan::ImportedSubmesh& imported : importResult.Submeshes)
        {
            Quiet::Renderer::Vulkan::StaticSubmesh submesh{};
            submesh.Name = imported.Name;
            submesh.BaseVertex = imported.BaseVertex;
            submesh.VertexCount = imported.VertexCount;
            submesh.BaseIndex = imported.BaseIndex;
            submesh.IndexCount = imported.IndexCount;
            submesh.MaterialSlot = imported.MaterialIndex;

            submeshes.push_back(std::move(submesh));
        }

        return submeshes;
    }
}

namespace Quiet::Project
{
    ProjectAssetLoader::ProjectAssetLoader(ProjectContext& projectContext,
                                           Renderer::Vulkan::MeshLibrary& meshLibrary,
                                           Renderer::Vulkan::TextureLibrary& textureLibrary,
                                           const Renderer::Vulkan::PhysicalDevice& physicalDevice,
                                           const Renderer::Vulkan::LogicalDevice& logicalDevice,
                                           const Renderer::Vulkan::GraphicsCommands& graphicsCommands)
                                               : m_ProjectContext(projectContext)
                                               , m_MeshLibrary(meshLibrary)
                                               , m_TextureLibrary(textureLibrary)
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
                allSupportedAssetsLoaded = false;
        }
        return allSupportedAssetsLoaded;
    }

    bool ProjectAssetLoader::ReloadTextureAssets(bool forceReimport)
    {
        const auto textureAssets =
            AssetRegistryIO::GetAssetsByType(m_ProjectContext.GetAssetRegistry(), AssetType::Texture);

        bool allSupportedAssetsLoaded = true;
        for (const AssetRecord* record : textureAssets)
        {
            if (!record) continue;
            if (record->BuiltIn) continue;

            if (!LoadExternalTexture(*record, forceReimport))
                allSupportedAssetsLoaded = false;
        }

        return allSupportedAssetsLoaded;
    }

    bool ProjectAssetLoader::ReloadAssets(bool forceReimport)
    {
        const bool meshesLoaded = ReloadMeshAssets(forceReimport);
        const bool texturesLoaded = ReloadTextureAssets(forceReimport);
        return meshesLoaded && texturesLoaded;
    }

    bool ProjectAssetLoader::LoadBuiltInMesh(const AssetRecord &record)
    {
        if (m_MeshLibrary.Has(record.ID)) { return true; }

        const auto primitive = Renderer::Vulkan::MeshPrimitives::Create(record.ID);
        if (!primitive.has_value() || !primitive->IsValid())
        {
            QUIET_CORE_WARN(
                "Built-in mesh asset '{}' is registered but has no primitive creator.",
                record.ID
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
                primitive->Vertices,
                primitive->Indices,
                std::vector<Renderer::Vulkan::StaticSubmesh>{},
                primitive->Stats
            );
        }
        catch (const std::exception& e)
        {
            QUIET_CORE_ERROR(
                "Failed to create built-in mesh asset '{}': {}",
                record.ID,
                e.what()
            );
            return false;
        }

        return true;
    }

    bool ProjectAssetLoader::LoadExternalMesh(const Project::AssetRecord& record, const bool forceReimport)
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
        std::vector<Renderer::Vulkan::StaticSubmesh> submeshes = MakeStaticSubmeshesFromImportResult(importResult);
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
                    submeshes,
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
                    std::move(submeshes),
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
        "{} mesh asset '{}' from '{}'. importer={} meshes={} submeshes={} vertices={} indices={} boundsValid={} warnings={}",
             alreadyLoaded ? "Reloaded" : "Loaded",
             record.ID,
             assetPath.string(),
             importResult.ImporterName,
             importResult.ImportedMeshCount,
             importResult.Submeshes.size(),
             importResult.Vertices.size(),
             importResult.Indices.size(),
             importResult.Bounds.Valid,
             importResult.Warnings.size()
         );

        for (const std::string& warning : importResult.Warnings)
        {
            QUIET_CORE_WARN("Mesh asset '{}': {}", record.ID, warning);
        }

        return true;
    }

    bool ProjectAssetLoader::LoadExternalTexture(const AssetRecord& record, const bool forceReimport)
    {
        const bool alreadyLoaded = m_TextureLibrary.Has(record.ID);
        if (alreadyLoaded && !forceReimport) return true;
        if (record.Path.empty())
        {
            QUIET_CORE_WARN("Texture asset '{}' is registered without a source path.", record.ID);
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
                "Texture asset '{}' points to missing file '{}'.",
                record.ID,
                assetPath.string()
            );
            return false;
        }

        const Renderer::Vulkan::TexturePixelData pixelData = Renderer::Vulkan::TextureImporter::LoadPixelsFromFile(assetPath);

        if (!pixelData.Metadata.Succeeded() || !pixelData.IsValid())
        {
            QUIET_CORE_WARN(
                "Failed to load texture asset '{}' from '{}': {}",
                record.ID,
                assetPath.string(),
                pixelData.Metadata.Error
            );
            return false;
        }

        Renderer::Vulkan::TextureStats stats = MakeTextureStatsFromImportResult(pixelData.Metadata);

        stats.PixelBytes = pixelData.GetByteSize();
        try
        {
            if (alreadyLoaded)
            {
                m_TextureLibrary.ReplaceTexture2D(
                    record.ID,
                    m_PhysicalDevice,
                    m_LogicalDevice,
                    m_GraphicsCommands,
                    pixelData,
                    stats
                );
            }
            else
            {
                m_TextureLibrary.CreateTexture2D(
                    record.ID,
                    m_PhysicalDevice,
                    m_LogicalDevice,
                    m_GraphicsCommands,
                    pixelData,
                    stats
                );
            }
        }
        catch (const std::exception& e)
        {
            QUIET_CORE_ERROR(
                "Failed to create runtime texture resource '{}' from '{}': {}",
                record.ID,
                assetPath.string(),
                e.what()
            );
            return false;
        }

        QUIET_CORE_INFO(
            "{} texture asset '{}' from '{}'. format={} dimensions={}x{} dimensionsValid={} pixels={} warnings={}",
            alreadyLoaded ? "Reloaded" : "Loaded",
            record.ID,
            assetPath.string(),
            pixelData.Metadata.Format,
            pixelData.Width,
            pixelData.Height,
            pixelData.Metadata.DimensionsValid,
            pixelData.GetByteSize(),
            pixelData.Metadata.Warnings.size()
        );

        for (const std::string& warning : pixelData.Metadata.Warnings)
        {
            QUIET_CORE_WARN("Texture asset '{}': {}", record.ID, warning);
        }

        return true;
    }
}
