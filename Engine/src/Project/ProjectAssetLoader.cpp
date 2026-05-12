#include "Quiet/Project/ProjectAssetLoader.h"

#include <cstdint>
#include <exception>
#include <filesystem>
#include <string>
#include <utility>
#include <cctype>
#include <fstream>
#include <unordered_map>

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
        stats.MaterialCount = static_cast<std::uint32_t>(importResult.Materials.size());
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

    std::vector<Quiet::Renderer::Vulkan::StaticMeshMaterialSlot> MakeStaticMaterialSlotsFromImportResult(
        const Quiet::Renderer::Vulkan::MeshImportResult& importResult)
    {
        std::vector<Quiet::Renderer::Vulkan::StaticMeshMaterialSlot> slots;
        slots.reserve(importResult.Materials.size());

        for (const Quiet::Renderer::Vulkan::ImportedMaterial& imported : importResult.Materials)
        {
            Quiet::Renderer::Vulkan::StaticMeshMaterialSlot slot{};
            slot.Name = imported.Name;
            slot.BaseColor = imported.BaseColor;
            slot.BaseColorTexturePath = imported.BaseColorTexturePath;
            slot.BaseColorTextureAssetID = imported.BaseColorTextureAssetID;
            slot.BaseColorTextureEmbedded = imported.BaseColorTextureEmbedded;
            slot.EmbeddedTextureIndex = imported.EmbeddedTextureIndex;

            slots.push_back(std::move(slot));
        }
        return slots;
    }

    std::string SanitizeAssetIDPart(std::string value)
    {
        for (char& c : value)
        {
            const auto uc = static_cast<unsigned char>(c);
            if (!std::isalnum(uc) && c != '_' && c != '-') c = '_';
        }

        while (!value.empty() && value.front() == '_')
        {
            value.erase(value.begin());
        }
        while (!value.empty() && value.back() == '_')
        {
            value.pop_back();
        }

        if (value.empty()) return "Texture";
        return value;
    }

    std::string MakeImportedMaterialTextureAssetID(const std::string& meshAssetID,
                                                   const std::string& materialName,
                                                   const std::size_t materialIndex)
    {
        return SanitizeAssetIDPart(meshAssetID) +
               "_" +
               SanitizeAssetIDPart(materialName.empty()
                   ? "Material_" + std::to_string(materialIndex)
                   : materialName) +
               "_BaseColor";
    }

    bool IsSupportedTextureExtension(const std::filesystem::path& path)
    {
        std::string ext = path.extension().string();
        std::transform(
            ext.begin(),
            ext.end(),
            ext.begin(),
            [](const unsigned char c)
            {
                return static_cast<char>(std::tolower(c));
            }
        );

        return ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".tga" || ext == ".bmp";
    }

    std::filesystem::path ResolveExternalMaterialTexturePath(
        const std::filesystem::path& meshAssetPath,
        const std::string& texturePath)
    {
        if (texturePath.empty()) return {};
        std::filesystem::path path(texturePath);

        if (path.is_absolute())
            return path.lexically_normal();

        return (meshAssetPath.parent_path() / path).lexically_normal();
    }

    const Quiet::Renderer::Vulkan::ImportedEmbeddedTexture* FindEmbeddedTextureByIndex(
        const Quiet::Renderer::Vulkan::MeshImportResult& importResult,
        const int embeddedTextureIndex)
    {
        for (const Quiet::Renderer::Vulkan::ImportedEmbeddedTexture& texture : importResult.EmbeddedTextures)
        {
            if (texture.Index == embeddedTextureIndex)
                return &texture;
        }
        return nullptr;
    }

    bool WriteBinaryFileIfDifferent(const std::filesystem::path& path,
                                    const std::vector<std::uint8_t>& bytes)
    {
        if (path.empty() || bytes.empty()) return false;

        std::error_code ec;
        std::filesystem::create_directories(path.parent_path(), ec);

        if (ec) return false;
        if (std::filesystem::exists(path, ec) && std::filesystem::file_size(path, ec) == bytes.size())
            return true;

        std::ofstream output(path, std::ios::binary | std::ios::trunc);
        if (!output.is_open()) return false;

        output.write(
            reinterpret_cast<const char*>(bytes.data()),
            static_cast<std::streamsize>(bytes.size())
        );

        return output.good();
    }

    void RegisterImportedMaterialTextures(
        Quiet::Project::ProjectContext& projectContext,
        const std::string& meshAssetID,
        const std::filesystem::path& meshAssetPath,
        Quiet::Renderer::Vulkan::MeshImportResult& importResult)
    {
        for (std::size_t materialIndex = 0;
             materialIndex < importResult.Materials.size();
             ++materialIndex)
        {
            Quiet::Renderer::Vulkan::ImportedMaterial& material = importResult.Materials[materialIndex];
            if (material.BaseColorTexturePath.empty()) continue;

            const std::string textureAssetID =
                MakeImportedMaterialTextureAssetID(
                    meshAssetID,
                    material.Name,
                    materialIndex
                );

            if (material.BaseColorTextureEmbedded)
            {
                const Quiet::Renderer::Vulkan::ImportedEmbeddedTexture* embeddedTexture =
                    FindEmbeddedTextureByIndex(importResult, material.EmbeddedTextureIndex);

                if (!embeddedTexture || !embeddedTexture->IsValid())
                {
                    importResult.Warnings.push_back(
                        "Material '" + material.Name +
                        "' references missing embedded base color texture '" +
                        material.BaseColorTexturePath + "'."
                    );
                    continue;
                }

                const std::filesystem::path outputPath =
                    projectContext.GetAssetsPath() /
                    "Textures" /
                    meshAssetID /
                    (textureAssetID + embeddedTexture->Extension);

                if (!WriteBinaryFileIfDifferent(outputPath, embeddedTexture->Bytes))
                {
                    importResult.Warnings.push_back(
                        "Failed to extract embedded base color texture for material '" +
                        material.Name + "'."
                    );
                    continue;
                }

                if (projectContext.RegisterTextureAsset(textureAssetID, outputPath, false))
                {
                    material.BaseColorTextureAssetID = textureAssetID;
                    continue;
                }

                importResult.Warnings.push_back(
                    "Failed to register extracted texture asset '" +
                    textureAssetID + "'."
                );

                continue;
            }

            const std::filesystem::path externalTexturePath =
                ResolveExternalMaterialTexturePath(
                    meshAssetPath,
                    material.BaseColorTexturePath
                );

            if (!std::filesystem::exists(externalTexturePath))
            {
                importResult.Warnings.push_back(
                    "Material '" + material.Name +
                    "' references missing base color texture '" +
                    externalTexturePath.string() + "'."
                );
                continue;
            }

            if (!IsSupportedTextureExtension(externalTexturePath))
            {
                importResult.Warnings.push_back(
                    "Material '" + material.Name +
                    "' references unsupported base color texture '" +
                    externalTexturePath.string() + "'."
                );
                continue;
            }

            if (projectContext.RegisterTextureAsset(textureAssetID, externalTexturePath, false))
            {
                material.BaseColorTextureAssetID = textureAssetID;
                continue;
            }

            importResult.Warnings.push_back(
                "Failed to register material texture asset '" +
                textureAssetID + "'."
            );
        }
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
        const auto meshAssetPointers =
            AssetRegistryIO::GetAssetsByType(
                m_ProjectContext.GetAssetRegistry(),
                AssetType::Mesh
            );

        std::vector<AssetRecord> meshAssets;
        meshAssets.reserve(meshAssetPointers.size());

        for (const AssetRecord* record : meshAssetPointers)
        {
            if (record) meshAssets.push_back(*record);
        }

        bool allSupportedAssetsLoaded = true;
        for (const AssetRecord& record : meshAssets)
        {
            if (record.BuiltIn)
            {
                if (!LoadBuiltInMesh(record)) allSupportedAssetsLoaded = false;
                continue;
            }

            if (!LoadExternalMesh(record, forceReimport))
                allSupportedAssetsLoaded = false;
        }
        return allSupportedAssetsLoaded;
    }

    bool ProjectAssetLoader::ReloadTextureAssets(const bool forceReimport)
    {
        const auto textureAssetPointers =
            AssetRegistryIO::GetAssetsByType(
                m_ProjectContext.GetAssetRegistry(),
                AssetType::Texture
            );

        std::vector<AssetRecord> textureAssets;
        textureAssets.reserve(textureAssetPointers.size());

        for (const AssetRecord* record : textureAssetPointers)
        {
            if (record) textureAssets.push_back(*record);
        }

        bool allSupportedAssetsLoaded = true;
        for (const AssetRecord& record : textureAssets)
        {
            if (record.BuiltIn) continue;
            if (!LoadExternalTexture(record, forceReimport))
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
                std::vector<Renderer::Vulkan::StaticMeshMaterialSlot>{},
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

        RegisterImportedMaterialTextures(
            m_ProjectContext,
            record.ID,
            assetPath,
            importResult
        );

        const Renderer::Vulkan::MeshStats stats = MakeMeshStatsFromImportResult(importResult);
        std::vector<Renderer::Vulkan::StaticSubmesh> submeshes = MakeStaticSubmeshesFromImportResult(importResult);
        std::vector<Renderer::Vulkan::StaticMeshMaterialSlot> materialSlots = MakeStaticMaterialSlotsFromImportResult(importResult);
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
                    materialSlots,
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
                    std::move(materialSlots),
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
            "{} mesh asset '{}' from '{}'. importer={} meshes={} submeshes={} materials={} embeddedTextures={} vertices={} indices={} boundsValid={} warnings={}",
            alreadyLoaded ? "Reloaded" : "Loaded",
            record.ID,
            assetPath.string(),
            importResult.ImporterName,
            importResult.ImportedMeshCount,
            importResult.Submeshes.size(),
            importResult.Materials.size(),
            importResult.EmbeddedTextures.size(),
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
