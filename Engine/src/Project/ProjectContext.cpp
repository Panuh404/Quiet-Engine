#include "Quiet/Project/ProjectContext.h"

#include <filesystem>
#include <utility>

namespace Quiet::Project
{
    bool LoadOrCreateProject(ProjectContext& projectContext,
                             const std::filesystem::path& projectFilePath)
    {
        ProjectDescriptor descriptor{};

        if (std::filesystem::exists(projectFilePath))
        {
            if (!ProjectDescriptorIO::LoadFromFile(projectFilePath, descriptor)) return false;
        }
        else
        {
            const std::string projectName = projectFilePath.stem().string();
            descriptor = ProjectDescriptorIO::MakeDefault(projectName);

            if (!ProjectDescriptorIO::SaveToFile(projectFilePath, descriptor)) return false;
        }

        projectContext.Initialize(projectFilePath, std::move(descriptor));
        return projectContext.EnsureDirectoriesExist();
    }
}