#include "Quiet/Project/ProjectAssetLoader.h"

#include <filesystem>

#include "Quiet/Core/Log.h"
#include "Quiet/Project/ProjectContext.h"
#include "Renderer/Vulkan/GraphicsCommands.h"
#include "Renderer/Vulkan/LogicalDevice.h"
#include "Renderer/Vulkan/MeshLibrary.h"
#include "Renderer/Vulkan/PhysicalDevice.h"


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
            InspectExternalMeshRecord(*record);
            allSupportedAssetsLoaded = false;
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

    void ProjectAssetLoader::InspectExternalMeshRecord(const AssetRecord &record) const
    {
        const std::filesystem::path assetPath = record.Path.empty() ? std::filesystem::path{}
            : (m_ProjectContext.GetRootPath() / record.Path).lexically_normal();

        if (record.Path.empty())
        {
            QUIET_CORE_WARN(
                "Mesh asset '{}' is registered without a source path.",
                record.ID
            );
            return;
        }

        if (!std::filesystem::exists(assetPath))
        {
            QUIET_CORE_WARN(
                "Mesh asset '{}' points to missing file '{}'.",
                record.ID,
                assetPath.string()
            );
            return;
        }

        QUIET_CORE_INFO(
            "Mesh asset '{}' is registered at '{}' but external mesh importing is not implemented yet.",
            record.ID,
            assetPath.string()
        );
    }
}
