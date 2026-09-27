#include "Quiet/Project/RuntimeAssetLoader.h"

#include <algorithm>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <string>
#include <utility>
#include <vector>

#include "Quiet/Assets/QMeshReader.h"
#include "Quiet/Core/Log.h"

#include "Renderer/Vulkan/Mesh/MeshLibrary.h"
#include "Renderer/Vulkan/Mesh/StaticMesh.h"

namespace Quiet::Project
{
    RuntimeAssetLoader::RuntimeAssetLoader(
        Renderer::Vulkan::MeshLibrary &meshLibrary,
        const Renderer::Vulkan::PhysicalDevice &physicalDevice,
        const Renderer::Vulkan::LogicalDevice &logicalDevice,
        const Renderer::Vulkan::GraphicsCommands &graphicsCommands)
            : m_MeshLibrary(meshLibrary)
            , m_PhysicalDevice(physicalDevice)
            , m_LogicalDevice(logicalDevice)
            , m_GraphicsCommands(graphicsCommands) {}

    bool RuntimeAssetLoader::LoadMeshAssets(const std::filesystem::path &packageRoot, const RuntimeManifest &manifest)
    {
        bool success = true;

        for (const RuntimeAssetRecord& record : manifest.Assets)
        {
            if (record.Type != AssetType::Mesh) continue;
            if (!LoadMeshAsset(packageRoot, record)) success = false;
        }
        return success;
    }

    bool RuntimeAssetLoader::LoadMeshAsset(const std::filesystem::path &packageRoot, const RuntimeAssetRecord &record)
    {
        if (!record.IsValid() || record.Type != AssetType::Mesh)
        {
            QUIET_CORE_WARN("Cannot load invalid runtime mesh asset '{}'.", record.ID);
            return false;
        }

        const std::filesystem::path assetPath = (packageRoot / record.Path).lexically_normal();
        std::error_code ec;

        if (!std::filesystem::is_regular_file(assetPath, ec) || ec)
        {
            QUIET_CORE_WARN("Runtime mesh asset '{}' points to missing file '{}'.", record.ID, assetPath.string());
            return false;
        }

        Assets::QMeshData meshData{};
        std::string readError;

        if (!Assets::QMeshReader::LoadFromFile(assetPath, meshData, readError))
        {
            QUIET_CORE_WARN("Failed to read runtime mesh asset '{}' from '{}': {}", record.ID, assetPath.string(), readError);
            return false;
        }

        std::vector<Renderer::Vulkan::StaticSubmesh> submeshes;
        submeshes.reserve(meshData.Submeshes.size());

        std::uint32_t materialSlotCount = 0;

        for (std::size_t i = 0; i < meshData.Submeshes.size(); ++i)
        {
            const Assets::QMeshSubmeshV1& source = meshData.Submeshes[i];
            Renderer::Vulkan::StaticSubmesh submesh{};

            // Source submesh names are editor/import metadata and are
            // intentionally not stored in QMesh v1.
            submesh.Name = "Submesh_" + std::to_string(i);
            submesh.BaseVertex = source.BaseVertex;
            submesh.VertexCount = source.VertexCount;
            submesh.BaseIndex = source.BaseIndex;
            submesh.IndexCount = source.IndexCount;
            submesh.MaterialSlot = source.MaterialSlot;
            materialSlotCount = std::max(materialSlotCount, source.MaterialSlot + 1u);
            submeshes.push_back(std::move(submesh));
        }

        Renderer::Vulkan::MeshStats stats{};
        stats.VertexCount = static_cast<std::uint32_t>(meshData.Vertices.size());
        stats.IndexCount = static_cast<std::uint32_t>(meshData.Indices.size());
        stats.SubmeshCount = static_cast<std::uint32_t>(meshData.Submeshes.size());
        stats.MaterialCount = materialSlotCount;
        stats.BoundsValid = meshData.Bounds.Valid;
        stats.BoundsMin = meshData.Bounds.Min;
        stats.BoundsMax = meshData.Bounds.Max;
        stats.ImporterName = "QMesh";

        const bool alreadyLoaded = m_MeshLibrary.Has(record.ID);

        try
        {
            std::vector<Renderer::Vulkan::StaticMeshMaterialSlot> importedMaterialSlots;
            if (alreadyLoaded)
            {
                m_MeshLibrary.ReplaceStaticMesh(
                    record.ID,
                    m_PhysicalDevice,
                    m_LogicalDevice,
                    m_GraphicsCommands,
                    meshData.Vertices,
                    meshData.Indices,
                    std::move(submeshes),
                    std::move(importedMaterialSlots),
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
                    meshData.Vertices,
                    meshData.Indices,
                    std::move(submeshes),
                    std::move(importedMaterialSlots),
                    stats
                );
            }
        }
        catch (const std::exception& e)
        {
            QUIET_CORE_ERROR("Failed to create runtime mesh resource '{}' from '{}': {}", record.ID, assetPath.string(), e.what());
            return false;
        }

        QUIET_CORE_INFO("{} cooked mesh asset '{}' from '{}'. submeshes={} materialSlots={} vertices={} indices={}",
            alreadyLoaded ? "Reloaded" : "Loaded",
            record.ID,
            assetPath.string(),
            meshData.Submeshes.size(),
            materialSlotCount,
            meshData.Vertices.size(),
            meshData.Indices.size()
        );

        return true;
    }
}
