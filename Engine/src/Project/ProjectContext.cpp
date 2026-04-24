#include "Quiet/Project/ProjectContext.h"

#include <filesystem>
#include <utility>

namespace Quiet::Project
{
    bool ProjectContext::LoadOrCreateAssetRegistry()
    {
        AssetRegistry registry{};

        const std::filesystem::path registryPath = GetAssetRegistryPath();
        if (std::filesystem::exists(registryPath))
        {
            if (!AssetRegistryIO::LoadFromFile(registryPath, registry))
                return false;
        }
        else
            registry = AssetRegistryIO::MakeDefault();

        AssetRegistryIO::EnsureBuiltInAssets(registry);
        m_AssetRegistry = std::move(registry);

        return AssetRegistryIO::SaveToFile(registryPath, m_AssetRegistry);
    }

    bool LoadOrCreateProject(ProjectContext& projectContext,
                             const std::filesystem::path& projectFilePath)
    {
        ProjectDescriptor descriptor{};

        if (std::filesystem::exists(projectFilePath))
        {
            if (!ProjectDescriptorIO::LoadFromFile(projectFilePath, descriptor))
                return false;
        }
        else
        {
            const std::string projectName = projectFilePath.stem().string();
            descriptor = ProjectDescriptorIO::MakeDefault(projectName);

            if (!ProjectDescriptorIO::SaveToFile(projectFilePath, descriptor))
                return false;
        }

        projectContext.Initialize(projectFilePath, std::move(descriptor));
        if (!projectContext.EnsureDirectoriesExist())
            return false;

        return projectContext.LoadOrCreateAssetRegistry();
    }
}