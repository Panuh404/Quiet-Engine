#include "Quiet/Project/ProjectAssetLoader.h"

#include <cstdint>
#include <exception>
#include <filesystem>
#include <string>
#include <utility>
#include <cctype>
#include <fstream>
#include <algorithm>
#include <iterator>
#include <unordered_set>

#include "Quiet/Core/Log.h"
#include "Quiet/Core/UUID.h"
#include "Quiet/Project/ProjectContext.h"
#include "Quiet/Assets/MeshImporter.h"
#include "Renderer/Vulkan/Mesh/MeshLibrary.h"
#include "Renderer/Vulkan/Mesh/MeshPrimitives.h"
#include "Quiet/Assets/TextureImporter.h"
#include "Quiet/Assets/TextureImportResult.h"
#include "Quiet/Assets/TexturePixelData.h"
#include "Renderer/Vulkan/Texture/TextureLibrary.h"
#include "Audio/AudioLibrary.h"


namespace
{
    bool HasBoundRendererResources(
        const Quiet::Renderer::Vulkan::PhysicalDevice* physicalDevice,
        const Quiet::Renderer::Vulkan::LogicalDevice* logicalDevice,
        const Quiet::Renderer::Vulkan::GraphicsCommands* graphicsCommands)
    {
        return physicalDevice != nullptr && logicalDevice != nullptr && graphicsCommands != nullptr;
    }

    Quiet::Renderer::Vulkan::MeshStats MakeMeshStatsFromImportResult(const Quiet::Assets::MeshImportResult& importResult)
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

    Quiet::Renderer::Vulkan::TextureStats MakeTextureStatsFromImportResult(const Quiet::Assets::TextureImportResult& importResult)
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

    std::vector<Quiet::Renderer::Vulkan::StaticSubmesh> MakeStaticSubmeshesFromImportResult(const Quiet::Assets::MeshImportResult& importResult)
    {
        std::vector<Quiet::Renderer::Vulkan::StaticSubmesh> submeshes;
        submeshes.reserve(importResult.Submeshes.size());

        for (const Quiet::Assets::ImportedSubmesh& imported : importResult.Submeshes)
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

    std::vector<Quiet::Renderer::Vulkan::StaticMeshMaterialSlot> MakeStaticMaterialSlotsFromImportResult(const Quiet::Assets::MeshImportResult& importResult)
    {
        std::vector<Quiet::Renderer::Vulkan::StaticMeshMaterialSlot> slots;
        slots.reserve(importResult.Materials.size());

        for (const Quiet::Assets::ImportedMaterial& imported : importResult.Materials)
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

    std::filesystem::path ResolveExternalMaterialTexturePath(const std::filesystem::path& meshAssetPath, const std::string& texturePath)
    {
        if (texturePath.empty()) return {};
        std::filesystem::path path(texturePath);

        if (path.is_absolute())
            return path.lexically_normal();

        return (meshAssetPath.parent_path() / path).lexically_normal();
    }

    const Quiet::Assets::ImportedEmbeddedTexture* FindEmbeddedTextureByIndex(const Quiet::Assets::MeshImportResult& importResult, const int embeddedTextureIndex)
    {
        for (const Quiet::Assets::ImportedEmbeddedTexture& texture : importResult.EmbeddedTextures)
        {
            if (texture.Index == embeddedTextureIndex)
                return &texture;
        }
        return nullptr;
    }

    bool WriteBinaryFileIfDifferent(const std::filesystem::path& path, const std::vector<std::uint8_t>& bytes)
    {
        if (path.empty() || bytes.empty()) return false;

        std::error_code ec;
        std::filesystem::create_directories(path.parent_path(), ec);
        if (ec) return false;

        {
            std::ifstream input(path, std::ios::binary);
            if (input && std::equal(bytes.begin(), bytes.end(), std::istreambuf_iterator<char>(input),
                std::istreambuf_iterator<char>{}, [](std::uint8_t expected, char actual)
                {
                    return expected == static_cast<std::uint8_t>(actual);
                }))
            {
                return true;
            }
        }

        std::ofstream output(path, std::ios::binary | std::ios::trunc);
        if (!output) return false;
        output.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
        output.close();
        return output.good();
    }

    void RegisterImportedMaterialTextures(Quiet::Project::ProjectContext& projectContext,
                                          const Quiet::Project::AssetRecord& meshRecord,
                                          const std::filesystem::path& meshAssetPath,
                                          Quiet::Assets::MeshImportResult& importResult)
    {
        namespace fs = std::filesystem;
        using namespace Quiet;
        using namespace Quiet::Project;

        if (std::none_of(importResult.Materials.begin(), importResult.Materials.end(),
            [](const Assets::ImportedMaterial& material) { return !material.BaseColorTexturePath.empty(); }))
        {
            return;
        }

        auto& liveRegistry = projectContext.GetAssetRegistry();
        AssetRegistry pendingRegistry = liveRegistry;
        AssetRecord updatedMesh = meshRecord;
        bool bindingsChanged = false;
        std::vector<fs::path> createdFiles;

        for (auto& material : importResult.Materials)
        {
            if (material.BaseColorTexturePath.empty()) continue;

            const auto* embedded = material.BaseColorTextureEmbedded ? FindEmbeddedTextureByIndex(importResult, material.EmbeddedTextureIndex) : nullptr;
            if (material.BaseColorTextureEmbedded && (!embedded || !embedded->IsValid()))
            {
                importResult.Warnings.push_back("Missing embedded texture for material '" + material.Name + "'.");
                continue;
            }

            const std::string sourceKey = embedded ? "*" + std::to_string(embedded->Index) : fs::path(material.BaseColorTexturePath).lexically_normal().generic_string();
            const auto binding = updatedMesh.MeshTextureBindings.find(sourceKey);

            const AssetRecord* texture = binding == updatedMesh.MeshTextureBindings.end() ? nullptr : AssetRegistryIO::FindByID(pendingRegistry, binding->second);
            if (texture && (texture->Type != AssetType::Texture || texture->BuiltIn || texture->Path.empty()))
                texture = nullptr;

            fs::path texturePath;
            if (texture)
            {
                texturePath = texture->Path;
                if (texturePath.is_relative()) texturePath = projectContext.GetRootPath() / texturePath;
            }
            else if (!embedded)
            {
                texturePath = ResolveExternalMaterialTexturePath(meshAssetPath, material.BaseColorTexturePath);
                const std::string normalizedPath = projectContext.NormalizeAssetPath(texturePath).generic_string();
                const auto existing = std::find_if(pendingRegistry.Records.begin(), pendingRegistry.Records.end(),
                    [&](const AssetRecord& record)
                    {
                        return record.Type == AssetType::Texture && !record.BuiltIn && record.Path == normalizedPath;
                    });
                if (existing != pendingRegistry.Records.end()) texture = &*existing;
            }

            Core::UUID textureID{};
            if (texture)
            {
                textureID = texture->ID;
            }
            else
            {
                do { textureID = Core::UUIDGenerator::New(); }
                while (AssetRegistryIO::FindByID(pendingRegistry, textureID));

                if (embedded)
                {
                    texturePath = projectContext.GetAssetsPath() / "Textures" /
                        Core::UUIDToString(meshRecord.ID) /
                        (Core::UUIDToString(textureID) + embedded->Extension);
                }
            }

            std::error_code ec;
            if (embedded)
            {
                const auto assetsRoot = fs::weakly_canonical(projectContext.GetAssetsPath(), ec);
                if (ec) continue;
                const auto resolvedPath = fs::weakly_canonical(texturePath, ec);
                const auto relativePath = resolvedPath.lexically_relative(assetsRoot);

                if (ec || relativePath.empty() || relativePath == "." || *relativePath.begin() == "..")
                {
                    importResult.Warnings.push_back("Embedded texture path is outside the project Assets folder.");
                    continue;
                }

                if (!texture)
                {
                    if (fs::exists(texturePath, ec) || ec) continue;
                    createdFiles.push_back(texturePath);
                }
                if (!WriteBinaryFileIfDifferent(texturePath, embedded->Bytes))
                {
                    importResult.Warnings.push_back("Failed to extract embedded texture for material '" + material.Name + "'.");
                    if (!texture) fs::remove(texturePath, ec);
                    continue;
                }
            }
            else if (!IsSupportedTextureExtension(texturePath) || !fs::is_regular_file(texturePath, ec) || ec)
            {
                importResult.Warnings.push_back("Missing or unsupported material texture '" + texturePath.string() + "'.");
                continue;
            }

            if (!texture)
            {
                const std::string name = embedded
                    ? meshRecord.Name + " Texture " + std::to_string(embedded->Index + 1LL)
                    : texturePath.stem().string();

                AssetRecord record{ textureID, name, AssetType::Texture, projectContext.NormalizeAssetPath(texturePath).generic_string(), false };
                if (!AssetRegistryIO::AddOrUpdate(pendingRegistry, std::move(record)))
                {
                    importResult.Warnings.push_back("Could not register material texture '" + name + "'.");
                    if (embedded) fs::remove(texturePath, ec);
                    continue;
                }
            }

            if (binding == updatedMesh.MeshTextureBindings.end() || binding->second != textureID)
            {
                updatedMesh.MeshTextureBindings[sourceKey] = textureID;
                bindingsChanged = true;
            }
            material.BaseColorTextureAssetID = textureID;
        }

        if (!bindingsChanged) return;
        if (AssetRegistryIO::AddOrUpdate(pendingRegistry, std::move(updatedMesh)) &&
            AssetRegistryIO::SaveToFile(projectContext.GetAssetRegistryPath(), pendingRegistry))
        {
            liveRegistry = std::move(pendingRegistry);
            return;
        }

        for (const auto& path : createdFiles)
        {
            std::error_code ec;
            fs::remove(path, ec);
        }
        for (auto& material : importResult.Materials)
        {
            if (!AssetRegistryIO::FindByID(liveRegistry, material.BaseColorTextureAssetID))
                material.BaseColorTextureAssetID = {};
        }
        importResult.Warnings.push_back("Failed to save imported material texture bindings for '" + meshRecord.Name + "'.");
    }
}

namespace Quiet::Project
{
    ProjectAssetLoader::ProjectAssetLoader(ProjectContext& projectContext,
                                           Renderer::Vulkan::MeshLibrary& meshLibrary,
                                           Renderer::Vulkan::TextureLibrary& textureLibrary,
                                           Audio::AudioLibrary& audioLibrary,
                                           const Renderer::Vulkan::PhysicalDevice& physicalDevice,
                                           const Renderer::Vulkan::LogicalDevice& logicalDevice,
                                           const Renderer::Vulkan::GraphicsCommands& graphicsCommands)
                                               : m_ProjectContext(projectContext)
                                               , m_MeshLibrary(meshLibrary)
                                               , m_TextureLibrary(textureLibrary)
                                               , m_AudioLibrary(audioLibrary)
    {
        BindRendererResources(physicalDevice, logicalDevice, graphicsCommands);
    }

    void ProjectAssetLoader::BindRendererResources(const Renderer::Vulkan::PhysicalDevice &physicalDevice,
                                                   const Renderer::Vulkan::LogicalDevice &logicalDevice,
                                                   const Renderer::Vulkan::GraphicsCommands &graphicsCommands)
    {
        m_PhysicalDevice = &physicalDevice;
        m_LogicalDevice = &logicalDevice;
        m_GraphicsCommands = &graphicsCommands;
    }

    bool ProjectAssetLoader::ReloadMeshAssets(const bool forceReimport)
    {
        const auto meshAssetPointers = AssetRegistryIO::GetAssetsByType(m_ProjectContext.GetAssetRegistry(), AssetType::Mesh);
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

    bool ProjectAssetLoader::ReloadBuiltInMeshAssets()
    {
        const auto meshAssetPointers = AssetRegistryIO::GetAssetsByType(m_ProjectContext.GetAssetRegistry(), AssetType::Mesh);
        bool allLoaded = true;

        for (const AssetRecord* record : meshAssetPointers)
        {
            if (!record || !record->BuiltIn) continue;
            if (!LoadBuiltInMesh(*record)) allLoaded = false;
        }
        return allLoaded;
    }

    bool ProjectAssetLoader::ReloadTextureAssets(const bool forceReimport)
    {
        const auto textureAssetPointers = AssetRegistryIO::GetAssetsByType(m_ProjectContext.GetAssetRegistry(), AssetType::Texture);

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

    bool ProjectAssetLoader::ReloadAudioAssets(bool forceReimport)
    {
        const auto assetPointers = AssetRegistryIO::GetAssetsByType(m_ProjectContext.GetAssetRegistry(), AssetType::Audio);
        std::vector<AssetRecord> assets;
        assets.reserve(assetPointers.size());

        for (const AssetRecord* record : assetPointers)
        {
            if (!record || record->BuiltIn) continue;
            assets.push_back(*record);
        }

        bool allLoaded = true;

        for (const AssetRecord& record : assets)
        {
            if (!LoadExternalAudio(record, forceReimport))
            {
                allLoaded = false;
            }
        }

        return allLoaded;
    }

    bool ProjectAssetLoader::ReloadAssets(bool forceReimport)
    {
        const bool meshesLoaded = ReloadMeshAssets(forceReimport);
        const bool texturesLoaded = ReloadTextureAssets(forceReimport);
        const bool audioLoaded = ReloadAudioAssets(forceReimport);
        return meshesLoaded && texturesLoaded && audioLoaded;
    }

    bool ProjectAssetLoader::ReloadRuntimeSupportAssets()
    {
        return ReloadBuiltInMeshAssets();
    }

    bool ProjectAssetLoader::ReloadAssetByID(Core::UUID assetID, bool forceReimport)
    {
        const AssetRecord* registered = AssetRegistryIO::FindByID(m_ProjectContext.GetAssetRegistry(), assetID);
        if (!registered)
        {
            QUIET_CORE_WARN("Cannot reimport asset '{}': asset is not registered.", Core::UUIDToString(assetID));
            return false;
        }

        const AssetRecord record = *registered;
        switch (record.Type)
        {
            case AssetType::Mesh:
            {
                if (record.BuiltIn) return LoadBuiltInMesh(record);
                if (!LoadExternalMesh(record, forceReimport)) return false;

                const auto mesh = m_MeshLibrary.TryGet(assetID);
                if (!mesh) return false;
                std::unordered_set<Core::UUID> reloadedTextures;
                bool loaded = true;

                for (const auto& slot : mesh->GetImportedMaterialSlots())
                {
                    const auto textureID = slot.BaseColorTextureAssetID;
                    if (!textureID.IsValid() || !reloadedTextures.insert(textureID).second) continue;
                    const auto* texture = AssetRegistryIO::FindByID(m_ProjectContext.GetAssetRegistry(), textureID);
                    if (!texture || !LoadExternalTexture(*texture, forceReimport)) loaded = false;
                }
                return loaded;
            }
            case AssetType::Texture:
                return LoadExternalTexture(record, forceReimport);

            case AssetType::Audio:
                return LoadExternalAudio(record, forceReimport);

            case AssetType::Script:
                QUIET_CORE_WARN("Script asset '{}' is source-only and does not require project reimport.", Core::UUIDToString(assetID));
                return false;

            case AssetType::Unknown:
            default:
                QUIET_CORE_WARN("Cannot reimport asset '{}': unsupported asset type.", Core::UUIDToString(assetID));
                return false;
        }
    }

    void ProjectAssetLoader::BeginIncrementalReload(bool forceReimport)
    {
        m_IncrementalForceReimport = forceReimport;
        m_IncrementalReloadProgress = {};
        m_IncrementalReloadProgress.Stage = AssetReloadStage::Meshes;
        m_AudioLibrary.Clear();
        PrepareIncrementalMeshAssets();
    }

    void ProjectAssetLoader::AdvanceIncrementalReload()
    {
        if (m_IncrementalReloadProgress.Stage == AssetReloadStage::Idle ||
            m_IncrementalReloadProgress.Stage == AssetReloadStage::Complete)
        {
            return;
        }

        if (m_IncrementalReloadIndex >= m_IncrementalReloadAssets.size())
        {
            if (m_IncrementalReloadProgress.Stage == AssetReloadStage::Meshes)
            {
                PrepareIncrementalTextureAssets();
            }
            else if (m_IncrementalReloadProgress.Stage == AssetReloadStage::Textures)
            {
                PrepareIncrementalAudioAssets();
            }
            else
            {
                m_IncrementalReloadProgress.Stage = AssetReloadStage::Complete;
                m_IncrementalReloadProgress.CurrentAssetID = {};
            }
            return;
        }

        const AssetRecord record = m_IncrementalReloadAssets[m_IncrementalReloadIndex];
        bool loaded = true;
        switch (m_IncrementalReloadProgress.Stage)
        {
            case AssetReloadStage::Meshes:
            {
                loaded = record.BuiltIn ? LoadBuiltInMesh(record) : LoadExternalMesh(record, m_IncrementalForceReimport);
                break;
            }

            case AssetReloadStage::Textures:
            {
                loaded = LoadExternalTexture(record, m_IncrementalForceReimport);
                break;
            }

            case AssetReloadStage::Audio:
            {
                loaded = LoadExternalAudio(record, m_IncrementalForceReimport);
                break;
            }

            case AssetReloadStage::Idle:
            case AssetReloadStage::Complete:
            default:
                return;
        }

        if (!loaded)
        {
            m_IncrementalReloadProgress.HadFailures = true;
        }

        ++m_IncrementalReloadIndex;
        m_IncrementalReloadProgress.Completed = m_IncrementalReloadIndex;
        if (m_IncrementalReloadIndex >= m_IncrementalReloadAssets.size())
        {
            if (m_IncrementalReloadProgress.Stage == AssetReloadStage::Meshes)
            {
                PrepareIncrementalTextureAssets();
            }
            else if (m_IncrementalReloadProgress.Stage == AssetReloadStage::Textures)
            {
                PrepareIncrementalAudioAssets();
            }
            else
            {
                m_IncrementalReloadProgress.Stage = AssetReloadStage::Complete;
                m_IncrementalReloadProgress.CurrentAssetID = {};
            }
            return;
        }

        m_IncrementalReloadProgress.CurrentAssetID = m_IncrementalReloadAssets[m_IncrementalReloadIndex].ID;
    }

    bool ProjectAssetLoader::LoadBuiltInMesh(const AssetRecord &record)
    {
        if (!HasBoundRendererResources(m_PhysicalDevice, m_LogicalDevice, m_GraphicsCommands))
        {
            QUIET_CORE_WARN("Cannot load asset because renderer upload resources are not bound.");
            return false;
        }
        if (m_MeshLibrary.Has(record.ID)) { return true; }

        if (!record.ID.IsValid())
        {
            QUIET_CORE_ERROR("Built-in mesh asset ID is not a valid UUID: '{}'.", Core::UUIDToString(record.ID));
            return false;
        }

        const auto primitive = Renderer::Vulkan::MeshPrimitives::Create(record.ID);
        if (!primitive.has_value() || !primitive->IsValid())
        {
            QUIET_CORE_WARN("Built-in mesh asset '{}' is registered but has no primitive creator.", Core::UUIDToString(record.ID));
            return false;
        }

        try
        {
            m_MeshLibrary.CreateStaticMesh(
                record.ID,
                *m_PhysicalDevice,
                *m_LogicalDevice,
                *m_GraphicsCommands,
                primitive->Vertices,
                primitive->Indices,
                std::vector<Renderer::Vulkan::StaticSubmesh>{},
                std::vector<Renderer::Vulkan::StaticMeshMaterialSlot>{},
                primitive->Stats
            );
        }
        catch (const std::exception& e)
        {
            QUIET_CORE_ERROR("Failed to create built-in mesh asset '{}': {}", Core::UUIDToString(record.ID), e.what());
            return false;
        }

        return true;
    }

    bool ProjectAssetLoader::LoadExternalMesh(const AssetRecord& record, const bool forceReimport)
    {
        if (!HasBoundRendererResources(m_PhysicalDevice, m_LogicalDevice, m_GraphicsCommands))
        {
            QUIET_CORE_WARN("Cannot load asset because renderer upload resources are not bound.");
            return false;
        }

        const bool alreadyLoaded = m_MeshLibrary.Has(record.ID);
        if (alreadyLoaded && !forceReimport) return true;
        if (record.Path.empty())
        {
            QUIET_CORE_WARN("Mesh asset '{}' is registered without a source path.", Core::UUIDToString(record.ID));
            return false;
        }

        const std::filesystem::path rawAssetPath = record.Path;
        const std::filesystem::path assetPath = rawAssetPath.is_absolute()
            ? rawAssetPath.lexically_normal()
            : (m_ProjectContext.GetRootPath() / rawAssetPath).lexically_normal();

        if (!std::filesystem::exists(assetPath))
        {
            QUIET_CORE_WARN("Mesh asset '{}' points to missing file '{}'.", Core::UUIDToString(record.ID), assetPath.string());
            return false;
        }

        Assets::MeshImportResult importResult = Assets::MeshImporter::LoadFromFile(assetPath);
        if (!importResult.Succeeded())
        {
            QUIET_CORE_WARN("Failed to import mesh asset '{}' from '{}': {}", Core::UUIDToString(record.ID), assetPath.string(), importResult.Error);
            return false;
        }

        RegisterImportedMaterialTextures(m_ProjectContext, record, assetPath, importResult);

        const Renderer::Vulkan::MeshStats stats = MakeMeshStatsFromImportResult(importResult);
        std::vector<Renderer::Vulkan::StaticSubmesh> submeshes = MakeStaticSubmeshesFromImportResult(importResult);
        std::vector<Renderer::Vulkan::StaticMeshMaterialSlot> materialSlots = MakeStaticMaterialSlotsFromImportResult(importResult);
        try
        {
            if (alreadyLoaded)
            {
                m_MeshLibrary.ReplaceStaticMesh(
                    record.ID,
                    *m_PhysicalDevice,
                    *m_LogicalDevice,
                    *m_GraphicsCommands,
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
                    *m_PhysicalDevice,
                    *m_LogicalDevice,
                    *m_GraphicsCommands,
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
            QUIET_CORE_ERROR("Failed to create runtime mesh resource '{}' from '{}': {}", Core::UUIDToString(record.ID), assetPath.string(), e.what());
            return false;
        }

        QUIET_CORE_INFO(
            "{} mesh asset '{}' from '{}'. importer={} meshes={} submeshes={} materials={} embeddedTextures={} vertices={} indices={} boundsValid={} warnings={}",
            alreadyLoaded ? "Reloaded" : "Loaded",
            Core::UUIDToString(record.ID),
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
            QUIET_CORE_WARN("Mesh asset '{}': {}", Core::UUIDToString(record.ID), warning);
        }

        return true;
    }

    bool ProjectAssetLoader::LoadExternalTexture(const AssetRecord& record, const bool forceReimport)
    {
        if (!HasBoundRendererResources(m_PhysicalDevice, m_LogicalDevice, m_GraphicsCommands))
        {
            QUIET_CORE_WARN("Cannot load asset because renderer upload resources are not bound.");
            return false;
        }

        const bool alreadyLoaded = m_TextureLibrary.Has(record.ID);
        if (alreadyLoaded && !forceReimport) return true;
        if (record.Path.empty())
        {
            QUIET_CORE_WARN("Texture asset '{}' is registered without a source path.", Core::UUIDToString(record.ID));
            return false;
        }

        const std::filesystem::path rawAssetPath = record.Path;
        const std::filesystem::path assetPath =
            rawAssetPath.is_absolute()
                ? rawAssetPath.lexically_normal()
                : (m_ProjectContext.GetRootPath() / rawAssetPath).lexically_normal();

        if (!std::filesystem::exists(assetPath))
        {
            QUIET_CORE_WARN("Texture asset '{}' points to missing file '{}'.", Core::UUIDToString(record.ID), assetPath.string());
            return false;
        }

        const Assets::TexturePixelData pixelData = Assets::TextureImporter::LoadPixelsFromFile(assetPath);

        if (!pixelData.Metadata.Succeeded() || !pixelData.IsValid())
        {
            QUIET_CORE_WARN("Failed to load texture asset '{}' from '{}': {}", Core::UUIDToString(record.ID), assetPath.string(), pixelData.Metadata.Error);
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
                    *m_PhysicalDevice,
                    *m_LogicalDevice,
                    *m_GraphicsCommands,
                    pixelData,
                    stats
                );
            }
            else
            {
                m_TextureLibrary.CreateTexture2D(
                    record.ID,
                    *m_PhysicalDevice,
                    *m_LogicalDevice,
                    *m_GraphicsCommands,
                    pixelData,
                    stats
                );
            }
        }
        catch (const std::exception& e)
        {
            QUIET_CORE_ERROR("Failed to create runtime texture resource '{}' from '{}': {}", Core::UUIDToString(record.ID), assetPath.string(), e.what());
            return false;
        }

        QUIET_CORE_INFO(
            "{} texture asset '{}' from '{}'. format={} dimensions={}x{} dimensionsValid={} pixels={} warnings={}",
            alreadyLoaded ? "Reloaded" : "Loaded",
            Core::UUIDToString(record.ID),
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
            QUIET_CORE_WARN("Texture asset '{}': {}", Core::UUIDToString(record.ID), warning);
        }

        return true;
    }

    bool ProjectAssetLoader::LoadExternalAudio(const AssetRecord &record, bool forceReimport)
    {
        if (record.Path.empty())
        {
            QUIET_CORE_WARN("Audio asset '{}' is registered without a source path.", Core::UUIDToString(record.ID));
            return false;
        }

        const std::filesystem::path rawPath = record.Path;
        const std::filesystem::path assetPath = rawPath.is_absolute()
            ? rawPath.lexically_normal()
            : (m_ProjectContext.GetRootPath() / rawPath).lexically_normal();

        if (!std::filesystem::exists(assetPath))
        {
            QUIET_CORE_WARN("Audio asset '{}' points to missing file '{}'.", Core::UUIDToString(record.ID), assetPath.string());
            return false;
        }

        return m_AudioLibrary.LoadFromFile(record.ID, assetPath, forceReimport);
    }

    void ProjectAssetLoader::PrepareIncrementalMeshAssets()
    {
        m_IncrementalReloadAssets.clear();
        m_IncrementalReloadIndex = 0;

        const auto assetPointers = AssetRegistryIO::GetAssetsByType(m_ProjectContext.GetAssetRegistry(), AssetType::Mesh);
        m_IncrementalReloadAssets.reserve(assetPointers.size());
        for (const AssetRecord* record : assetPointers)
        {
            if (record)
            {
                m_IncrementalReloadAssets.push_back(*record);
            }
        }

        m_IncrementalReloadProgress.Stage = AssetReloadStage::Meshes;
        m_IncrementalReloadProgress.Completed = 0;
        m_IncrementalReloadProgress.Total = m_IncrementalReloadAssets.size();
        m_IncrementalReloadProgress.CurrentAssetID = m_IncrementalReloadAssets.empty() ? Core::UUID{} : m_IncrementalReloadAssets.front().ID;

        if (m_IncrementalReloadAssets.empty())
        {
            PrepareIncrementalTextureAssets();
        }
    }

    void ProjectAssetLoader::PrepareIncrementalTextureAssets()
    {
        m_IncrementalReloadAssets.clear();
        m_IncrementalReloadIndex = 0;

        const auto assetPointers = AssetRegistryIO::GetAssetsByType(m_ProjectContext.GetAssetRegistry(), AssetType::Texture);
        m_IncrementalReloadAssets.reserve(assetPointers.size());
        for (const AssetRecord* record : assetPointers)
        {
            if (!record || record->BuiltIn) continue;
            m_IncrementalReloadAssets.push_back(*record);
        }

        m_IncrementalReloadProgress.Stage = AssetReloadStage::Textures;
        m_IncrementalReloadProgress.Completed = 0;
        m_IncrementalReloadProgress.Total = m_IncrementalReloadAssets.size();
        m_IncrementalReloadProgress.CurrentAssetID = m_IncrementalReloadAssets.empty() ? Core::UUID{} : m_IncrementalReloadAssets.front().ID;

        if (m_IncrementalReloadAssets.empty())
        {
            PrepareIncrementalAudioAssets();
        }
    }

    void ProjectAssetLoader::PrepareIncrementalAudioAssets()
    {
        m_IncrementalReloadAssets.clear();
        m_IncrementalReloadIndex = 0;

        const auto assetPointers = AssetRegistryIO::GetAssetsByType(m_ProjectContext.GetAssetRegistry(), AssetType::Audio);
        m_IncrementalReloadAssets.reserve(assetPointers.size());

        for (const AssetRecord* record : assetPointers)
        {
            if (!record || record->BuiltIn) continue;
            m_IncrementalReloadAssets.push_back(*record);
        }

        m_IncrementalReloadProgress.Stage = AssetReloadStage::Audio;
        m_IncrementalReloadProgress.Completed = 0;
        m_IncrementalReloadProgress.Total = m_IncrementalReloadAssets.size();
        m_IncrementalReloadProgress.CurrentAssetID = m_IncrementalReloadAssets.empty() ? Core::UUID{} : m_IncrementalReloadAssets.front().ID;

        if (m_IncrementalReloadAssets.empty())
        {
            m_IncrementalReloadProgress.Stage = AssetReloadStage::Complete;
            m_IncrementalReloadProgress.CurrentAssetID = {};
        }
    }
}
